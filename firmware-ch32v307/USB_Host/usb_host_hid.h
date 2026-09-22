#ifndef __USB_HOST_HID_H
#define __USB_HOST_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/******************************************************************************/
/* Setup requests for HID Boot Protocol */

__attribute__((aligned(4))) static const uint8_t SetupSetProtocol[ ] =
{
    0x21, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

__attribute__((aligned(4))) static const uint8_t SetupSetIdle[ ] =
{
    0x21, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/******************************************************************************/
/* Function Declaration */
extern uint8_t HID_SetProtocol( uint8_t ep0_size, uint8_t intf_num );
extern uint8_t HID_SetIdle( uint8_t ep0_size, uint8_t intf_num, uint8_t duration, uint8_t reportid );

#ifdef __cplusplus
}
#endif

#endif
