#ifndef __HID_REPORT_PARSER_H
#define __HID_REPORT_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/******************************************************************************/
/* Mouse layout extracted from a HID Report Descriptor.
 *
 * All offsets are in BITS, relative to byte 0 of the report. If the descriptor
 * contains a Report ID item, it is taken into account: the Report ID byte
 * shifts every bit offset by 8. */
typedef struct
{
    uint8_t report_id;              /* 0 = the report has no Report ID    */
    struct
    {
        uint8_t  valid;             /* 1 = a button block was found       */
        uint16_t bit_off;           /* bit position of the first button   */
        uint8_t  bit_size;          /* bits per button                    */
        uint8_t  count;             /* number of button bits (mask width) */
    } btn;
    struct
    {
        uint16_t bit_off;
        uint8_t  bit_size;
        uint8_t  is_signed;
    } x;
    struct
    {
        uint16_t bit_off;
        uint8_t  bit_size;
        uint8_t  is_signed;
    } y;
    uint8_t valid;                  /* 1 = X and Y both found             */
} hid_mouse_layout_t;

/******************************************************************************/
/* Extract bit_size bits from buf starting at bit_off (LSB first inside each
 * byte, little-endian across bytes). Sign-extended when is_signed.
 *
 * Light-weight on purpose (no allocation, no division, bounded loop) so it can
 * run inside the 8 kHz ISR; bit_size must be <= 31. */
static inline int32_t hid_bits( const uint8_t *buf, uint16_t bit_off, uint8_t bit_size, uint8_t is_signed )
{
    int32_t v = 0;
    uint8_t i;

    for( i = 0; ( i < bit_size ) && ( i < 32 ); i++ )
    {
        if( buf[ ( bit_off + i ) >> 3 ] & ( 1u << ( ( bit_off + i ) & 7 ) ) )
        {
            v |= ( 1u << i );
        }
    }
    if( is_signed && ( bit_size > 0 ) && ( bit_size < 32 ) &&
        ( v & ( 1 << ( bit_size - 1 ) ) ) )
    {
        v = (int32_t)( (uint32_t)v | ~( ( 1u << bit_size ) - 1u ) );
    }
    return v;
}

/******************************************************************************/
/* Function Declaration */
extern uint8_t HID_ParseMouseLayout( const uint8_t *desc, uint16_t len, hid_mouse_layout_t *out );
extern uint8_t HID_GetReportDescr( uint8_t ep0_size, uint8_t intf_num, uint8_t *pbuf, uint16_t wanted_len, uint16_t *plen );

#ifdef __cplusplus
}
#endif

#endif