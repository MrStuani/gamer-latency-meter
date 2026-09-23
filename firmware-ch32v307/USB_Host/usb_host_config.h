#ifndef __USB_HOST_CONFIG_H
#define __USB_HOST_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "string.h"
#include "debug.h"
#include "ch32v30x_usb.h"
#include "ch32v30x_usbhs_host.h"
#include "usb_host_hid.h"

/******************************************************************************/
/* Debug */
#define DEF_DEBUG_PRINTF 1
#if ( DEF_DEBUG_PRINTF == 1 )
#define DUG_PRINTF( format, arg... )    printf( format, ##arg )
#else
#define DUG_PRINTF( format, arg... )    do{ if( 0 )printf( format, ##arg ); }while( 0 );
#endif

/******************************************************************************/
/* USB Host — USBHS only, no HUB, single device */
#define DEF_TOTAL_ROOT_HUB          1
#define DEF_USBFS_PORT_EN           0
#define DEF_USBHS_PORT_EN           1
#define DEF_USBFS_PORT_INDEX        0x00
#define DEF_USBHS_PORT_INDEX        0x01
#define DEF_ONE_USB_SUP_DEV_TOTAL   1
#define DEF_NEXT_HUB_PORT_NUM_MAX   0
#define DEF_INTERFACE_NUM_MAX       1

#define DEF_COM_BUF_LEN             512

/******************************************************************************/
/* Device status */
#define ROOT_DEV_DISCONNECT         0
#define ROOT_DEV_CONNECTED          1
#define ROOT_DEV_FAILED             2
#define ROOT_DEV_SUCCESS            3

#define USB_DEVICE_ADDR             0x02

/* USB Speed */
#define USB_LOW_SPEED               0x00
#define USB_FULL_SPEED              0x01
#define USB_HIGH_SPEED              0x02
#define USB_SPEED_CHECK_ERR         0xFF

/* Descriptor types */
#define DEF_DECR_CONFIG             0x02
#define DEF_DECR_INTERFACE          0x04
#define DEF_DECR_ENDPOINT           0x05
#define DEF_DECR_HID                0x21

/* USB status codes */
#define ERR_SUCCESS                 0x00
#define ERR_USB_CONNECT             0x15
#define ERR_USB_DISCON              0x16
#define ERR_USB_BUF_OVER            0x17
#define ERR_USB_DISK_ERR            0x1F
#define ERR_USB_TRANSFER            0x20
#define ERR_USB_UNSUPPORT           0xFB
#define ERR_USB_UNAVAILABLE         0xFC
#define ERR_USB_UNKNOWN             0xFE

/* Enumeration status codes */
#define DEF_DEV_DESCR_GETFAIL       0x45
#define DEF_DEV_ADDR_SETFAIL        0x46
#define DEF_CFG_DESCR_GETFAIL       0x47
#define DEF_REP_DESCR_GETFAIL       0x48
#define DEF_CFG_VALUE_SETFAIL       0x49
#define DEF_DEV_TYPE_UNKNOWN        0xFF

/* Timings */
#define DEF_BUS_RESET_TIME          11
#define DEF_RE_ATTACH_TIMEOUT       100
#define DEF_WAIT_USB_TRANSFER_CNT   1000
/* Pior caso aceitável de espera por uma transferência dentro da ISR de 8kHz
   (125µs). Se o device não completar neste tempo, a ISR aborta o poll e volta
   em vez de travar vários ciclos. */
#define DEF_ISR_TRANSFER_WAIT_CNT   32
#define DEF_CTRL_TRANS_TIMEOVER_CNT 200000/20

/******************************************************************************/
/* HID device type */
#define DEC_KEY                     0x01
#define DEC_MOUSE                   0x02
#define DEC_UNKNOW                  0xFF

/******************************************************************************/
/* Structs — simplified for single device */

typedef struct _ROOT_HUB_DEVICE
{
    uint8_t  bStatus;
    uint8_t  bType;
    uint8_t  bAddress;
    uint8_t  bSpeed;
    uint8_t  bEp0MaxPks;
    uint8_t  DeviceIndex;
    uint8_t  bPortNum;
} ROOT_HUB_DEVICE, *PROOT_HUB_DEVICE;

typedef struct __HOST_CTL
{
    uint8_t  InterfaceNum;
    uint8_t  ErrorCount;

    struct interface
    {
        uint8_t  Type;
        uint8_t  IntfNum;      /* bInterfaceNumber (wIndex of class requests) */
        uint8_t  InEndpNum;
        uint8_t  InEndpAddr[ 4 ];
        uint8_t  InEndpType[ 4 ];
        uint16_t InEndpSize[ 4 ];
        uint8_t  InEndpTog[ 4 ];
        uint8_t  InEndpInterval[ 4 ];
        uint8_t  InEndpTimeCount[ 4 ];

        /* HID report-descriptor derived field layout (valid when Type is HID).
         * If the device uses a Report ID, ReportID is 1 and it occupies byte 0
         * of every report; all the offsets below are then relative to byte 1. */
        uint8_t  ReportID;     /* 1 = first byte is Report ID, 0 = no ID */
        uint8_t  BtnOffset;    /* byte index where the button field starts */
        uint8_t  BtnBits;      /* number of button bits starting at bit 0 */
        uint8_t  XOffset;      /* byte index of X delta (0xFF = absent)  */
        uint8_t  YOffset;      /* byte index of Y delta (0xFF = absent)  */
        uint8_t  ModOffset;    /* byte index of keyboard modifiers       */
        uint8_t  KeyOffset;    /* byte index of keyboard key codes       */

        /* Automatic layout, extracted from the HID Report Descriptor (see
         * hid_report_parser). When LayoutAuto is 1 the ISR reads X/Y/buttons
         * through the *Bit* fields below; otherwise it uses the byte offsets
         * above (boot/report fallback). Domains of {X,Y,Btn}BitOff are 0..511
         * bit for a 64-byte report. */
        uint16_t ReportDescLen;  /* wDescriptorLength of the HID descriptor */
        uint8_t  LayoutAuto;     /* 1 = use the *Bit* fields below         */
        uint8_t  BtnValid;       /* 1 = a button block was found           */
        uint16_t BtnBitOff;      /* bit position of the first button       */
        uint8_t  BtnBitSize;     /* bits per button                        */
        uint8_t  BtnBitCount;    /* button mask width (bits read at once)  */
        uint16_t XBitOff;
        uint8_t  XBitSize;
        uint8_t  XSigned;
        uint16_t YBitOff;
        uint8_t  YBitSize;
        uint8_t  YSigned;


        uint8_t  OutEndpNum;
        uint8_t  OutEndpAddr[ 4 ];
        uint8_t  OutEndpType[ 4 ];
        uint16_t OutEndpSize[ 4 ];
        uint8_t  OutEndpTog[ 4 ];
    }Interface[ DEF_INTERFACE_NUM_MAX ];
} HOST_CTL, *PHOST_CTL;

extern ROOT_HUB_DEVICE  Dev;
extern HOST_CTL         Ctl;
extern uint8_t          DevDesc_Buf[ 18 ];
extern uint8_t          Com_Buf[ DEF_COM_BUF_LEN ];

#ifdef __cplusplus
}
#endif

#endif
