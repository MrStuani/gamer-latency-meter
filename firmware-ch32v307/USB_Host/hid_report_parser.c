#include "usb_host_config.h"
#include "hid_report_parser.h"

/*********************************************************************
 * @fn      HID_ItemSigned
 *
 * @brief   Sign-extend an item's raw little-endian data according to
 *          how many data bytes the item actually carries.
 *
 * @param   data: raw item data (little-endian)
 *          dlen: 1, 2 or 4 (number of data bytes)
 *
 * @return  The value sign-extended to 32 bits.
 */
static int32_t HID_ItemSigned( uint32_t data, uint8_t dlen )
{
    switch( dlen )
    {
        case 1:  return (int8_t)data;
        case 2:  return (int16_t)data;
        default: return (int32_t)data;
    }
}

/*********************************************************************
 * @fn      HID_ParseMouseLayout
 *
 * @brief   Parse a HID Report Descriptor (short items) and recover the
 *          bit positions of the mouse buttons, X and Y fields.
 *
 *          Item prefix: bits[7:4]=tag, bits[3:2]=type (0 Main, 1 Global,
 *          2 Local), bits[1:0]=data size (0,1,2,4 bytes). The parser
 *          tracks Usage Page, the local Usage list (reset on every Main
 *          item), Report Size/Count, Report ID (shifts the bit offset by
 *          8) and Logical Minimum (decides is_signed), and on each Input
 *          item — ignoring fields whose "constant" bit is set — records:
 *            - Usage Page 0x09 (Button)  -> first button block;
 *            - Usage Page 0x01, Usage 0x30 -> X;
 *            - Usage Page 0x01, Usage 0x31 -> Y.
 *
 *          On an out-of-bounds / malformed item this stops and reports
 *          failure, letting the caller fall back to a fixed layout.
 *
 * @param   desc: report descriptor bytes
 *          len:  descriptor length
 *          out:  filled with the recovered layout on success
 *
 * @return  ERR_SUCCESS if X and Y were found (buttons are optional).
 *          ERR_USB_UNSUPPORT otherwise.
 */
uint8_t HID_ParseMouseLayout( const uint8_t *desc, uint16_t len, hid_mouse_layout_t *out )
{
    uint16_t i = 0;
    uint16_t bit_off = 0;
    uint16_t usage_page = 0;
    uint16_t usages[ 8 ];
    uint8_t  n_usage = 0;
    uint16_t report_size = 0;
    uint16_t report_count = 0;
    int32_t  logic_min = 0;
    uint8_t  x_found = 0;
    uint8_t  y_found = 0;

    memset( out, 0, sizeof( hid_mouse_layout_t ) );

    while( i < len )
    {
        uint8_t prefix = desc[ i ];
        uint8_t btype  = ( prefix >> 2 ) & 0x03;
        uint8_t btag   = prefix >> 4;
        uint8_t dlen;

        if( prefix == 0xFE ) /* long item: 0xFE, bDataSize, data[..] */
        {
            if( i + 1 >= len )
            {
                break;
            }
            i += 2 + desc[ i + 1 ];
            continue;
        }

        dlen = ( prefix & 0x03 );
        if( dlen == 3 )
        {
            dlen = 4;
        }
        if( (uint16_t)( i + 1 + dlen ) > len )
        {
            break;
        }

        {
            uint32_t data = 0;
            uint8_t  k;
            for( k = 0; k < dlen; k++ )
            {
                data |= (uint32_t)desc[ i + 1 + k ] << ( 8 * k );
            }

        if( btype == 0x01 ) /* Global item */
        {
            switch( btag )
            {
                case 0x0: /* Usage Page */
                    usage_page = (uint16_t)data;
                    break;
                case 0x1: /* Logical Minimum */
                    logic_min = HID_ItemSigned( data, dlen );
                    break;
                case 0x7: /* Report Size */
                    report_size = (uint16_t)data;
                    break;
                case 0x8: /* Report ID */
                    out->report_id = (uint8_t)data;
                    bit_off += 8;
                    break;
                case 0x9: /* Report Count */
                    report_count = (uint16_t)data;
                    break;
                default:
                    break;
            }
        }
        else if( btype == 0x02 ) /* Local item */
        {
            if( ( btag == 0x0 ) && ( n_usage < 8 ) ) /* Usage */
            {
                usages[ n_usage++ ] = (uint16_t)data;
            }
        }
        else if( btype == 0x00 ) /* Main item */
        {
            uint16_t f;
            uint8_t  flags = ( dlen ) ? desc[ i + 1 ] : 0;
            uint8_t  constant = flags & 0x01;

            for( f = 0; f < report_count; f++ )
            {
                uint16_t field_off = bit_off + (uint16_t)( f * report_size );
                uint16_t u = ( f < n_usage ) ? usages[ f ] : 0;

                if( !constant )
                {
                    if( ( usage_page == 0x09 ) && ( !out->btn.valid ) )
                    {
                        out->btn.valid   = 1;
                        out->btn.bit_off = bit_off;
                        out->btn.bit_size = ( report_size > 31 ) ? 31 : (uint8_t)report_size;
                        out->btn.count    = ( report_count > 31 ) ? 31 : (uint8_t)report_count;
                    }
                    else if( ( usage_page == 0x01 ) && ( u == 0x30 ) && ( !x_found ) )
                    {
                        out->x.bit_off   = field_off;
                        out->x.bit_size  = (uint8_t)report_size;
                        out->x.is_signed = ( logic_min < 0 );
                        x_found = 1;
                    }
                    else if( ( usage_page == 0x01 ) && ( u == 0x31 ) && ( !y_found ) )
                    {
                        out->y.bit_off   = field_off;
                        out->y.bit_size  = (uint8_t)report_size;
                        out->y.is_signed = ( logic_min < 0 );
                        y_found = 1;
                    }
                }
            }
            bit_off += (uint16_t)( report_size * report_count );
            n_usage = 0; /* local usages are reset on every Main item */
        }
        } /* data scope */

        i += 1 + dlen;
    }

    out->valid = ( x_found && y_found );
    return ( out->valid ) ? ERR_SUCCESS : ERR_USB_UNSUPPORT;
}

/*********************************************************************
 * @fn      HID_GetReportDescr
 *
 * @brief   Fetch the HID Report Descriptor of a HID interface with a
 *          class GET_DESCRIPTOR on control EP0 (bmRequestType 0x81,
 *          bRequest 0x06, wValue 0x2200 = Report Descriptor, wIndex =
 *          interface number). Follows the pattern of USBHSH_GetConfigDescr
 *          / USBHSH_GetDeviceDescr.
 *
 * @param   ep0_size:   device endpoint 0 max packet size
 *          intf_num:   interface number (wIndex)
 *          pbuf:       output buffer
 *          wanted_len: number of bytes requested (wLength)
 *          plen:       receives the number of bytes actually read
 *
 * @return  ERR_SUCCESS or error code.
 */
uint8_t HID_GetReportDescr( uint8_t ep0_size, uint8_t intf_num, uint8_t *pbuf, uint16_t wanted_len, uint16_t *plen )
{
    uint8_t s;

    memset( pUSBHS_SetupRequest, 0, sizeof( USB_SETUP_REQ ) );
    pUSBHS_SetupRequest->bRequestType = 0x81;                        /* IN | CLASS | INTERFACE */
    pUSBHS_SetupRequest->bRequest      = USB_GET_DESCRIPTOR;         /* GET_DESCRIPTOR          */
    pUSBHS_SetupRequest->wValue        = (uint16_t)( 0x22u << 8 );   /* HID Report Descriptor   */
    pUSBHS_SetupRequest->wIndex        = (uint16_t)intf_num;
    pUSBHS_SetupRequest->wLength       = wanted_len;

    s = USBHSH_CtrlTransfer( ep0_size, pbuf, plen );
    if( s != ERR_SUCCESS )
    {
        return s;
    }
    if( ( plen != NULL ) && ( *plen > wanted_len ) )
    {
        *plen = wanted_len;
    }
    return ERR_SUCCESS;
}