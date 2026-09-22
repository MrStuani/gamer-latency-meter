#include "usb_host_config.h"

/*********************************************************************
 * @fn      HID_SetProtocol
 *
 * @brief   Force Boot Protocol on a HID interface.
 *
 * @param   ep0_size: endpoint 0 max packet size
 *          intf_num: interface number
 *
 * @return  ERR_SUCCESS or error code
 */
uint8_t HID_SetProtocol( uint8_t ep0_size, uint8_t intf_num )
{
    memcpy( pUSBHS_SetupRequest, SetupSetProtocol, sizeof( USB_SETUP_REQ ) );
    pUSBHS_SetupRequest->wIndex = (uint16_t)intf_num;
    return USBHSH_CtrlTransfer( ep0_size, NULL, NULL );
}

/*********************************************************************
 * @fn      HID_SetIdle
 *
 * @brief   Set HID idle rate (0 = report on change).
 *
 * @param   ep0_size: endpoint 0 max packet size
 *          intf_num: interface number
 *          duration: idle rate (0 = report only on change)
 *          reportid: report ID (0 for boot protocol)
 *
 * @return  ERR_SUCCESS or error code
 */
uint8_t HID_SetIdle( uint8_t ep0_size, uint8_t intf_num, uint8_t duration, uint8_t reportid )
{
    memcpy( pUSBHS_SetupRequest, SetupSetIdle, sizeof( USB_SETUP_REQ ) );
    pUSBHS_SetupRequest->wValue = ( (uint16_t)duration << 8 ) | reportid;
    pUSBHS_SetupRequest->wIndex = (uint16_t)intf_num;
    return USBHSH_CtrlTransfer( ep0_size, NULL, NULL );
}
