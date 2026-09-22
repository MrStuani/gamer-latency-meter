/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32v30x_usbfs_device.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2022/08/20
* Description        : USBFS device functions (CDC virtual COM for debug).
*                      Adapted: UART bridge removed. Debug strings are pushed
*                      directly to the CDC IN endpoint (UEP3).
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
*******************************************************************************/

#include "ch32v30x_usbfs_device.h"

/*******************************************************************************/
/* Variable Definition */
const    uint8_t  *pUSBFS_Descr;

/* Setup Request */
volatile uint8_t  USBFS_SetupReqCode;
volatile uint8_t  USBFS_SetupReqType;
volatile uint16_t USBFS_SetupReqValue;
volatile uint16_t USBFS_SetupReqIndex;
volatile uint16_t USBFS_SetupReqLen;

/* USB Device Status */
volatile uint8_t  USBFS_DevConfig;
volatile uint8_t  USBFS_DevAddr;
volatile uint8_t  USBFS_DevSleepStatus;
volatile uint8_t  USBFS_DevEnumStatus;

/* Endpoint Buffer */
__attribute__ ((aligned(4))) uint8_t USBFS_EP0_Buf[ DEF_USBD_UEP0_SIZE ];
__attribute__ ((aligned(4))) uint8_t USBFS_EP1_Buf[ DEF_USBD_ENDP1_SIZE ];
__attribute__ ((aligned(4))) uint8_t USBFS_EP2_Buf[ DEF_USBD_ENDP2_SIZE ];
__attribute__ ((aligned(4))) uint8_t USBFS_EP3_Buf[ DEF_USBD_ENDP3_SIZE ];

/* USB IN Endpoint Busy Flag */
volatile uint8_t  USBFS_Endp_Busy[ DEF_UEP_NUM ];

/******************************************************************************/
/* CDC line coding (presented to host; not wired to UART) */
volatile uint8_t CDC_LineCoding[ 7 ] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };

/* CDC debug TX ring buffer (drained to UEP3 by CDC_flush) */
#define CDC_TX_BUF_SIZE  1024
static volatile uint16_t s_cdc_tx_head = 0;
static volatile uint16_t s_cdc_tx_tail = 0;
static uint8_t s_cdc_tx_buf[ CDC_TX_BUF_SIZE ];

/******************************************************************************
 * Interrupt Service Routine Declaration
 ******************************************************************************/
void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      USBFS_RCC_Init
 *
 * @brief   Initializes the usbfs clock configuration.
 *
 * @return  none
 */
void USBFS_RCC_Init(void)
{
#ifdef CH32V30x_D8C
    RCC_USBCLK48MConfig( RCC_USBCLK48MCLKSource_USBPHY );
    RCC_USBHSPLLCLKConfig( RCC_HSBHSPLLCLKSource_HSE );
    RCC_USBHSConfig( RCC_USBPLL_Div2 );
    RCC_USBHSPLLCKREFCLKConfig( RCC_USBHSPLLCKREFCLK_4M );
    RCC_USBHSPHYPLLALIVEcmd( ENABLE );
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_USBHS, ENABLE );
#else
    if( SystemCoreClock == 144000000 )
        RCC_USBFSCLKConfig( RCC_USBFSCLKSource_PLLCLK_Div3 );
    else if( SystemCoreClock == 96000000 )
        RCC_USBFSCLKConfig( RCC_USBFSCLKSource_PLLCLK_Div2 );
    else
        RCC_USBFSCLKConfig( RCC_USBFSCLKSource_PLLCLK_Div1 );
#endif
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_USBFS, ENABLE );
}

/*********************************************************************
 * @fn      USBFS_Device_Endp_Init
 *
 * @brief   Initializes USB device endpoints.
 *
 * @return  none
 */
void USBFS_Device_Endp_Init( void )
{
    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_RX_EN|USBFS_UEP3_TX_EN;

    USBFSD->UEP0_DMA = (uint32_t)USBFS_EP0_Buf;
    USBFSD->UEP1_DMA = (uint32_t)USBFS_EP1_Buf;
    USBFSD->UEP2_DMA = (uint32_t)USBFS_EP2_Buf;
    USBFSD->UEP3_DMA = (uint32_t)USBFS_EP3_Buf;

    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFSD->UEP2_RX_CTRL = USBFS_UEP_R_RES_ACK;

    USBFSD->UEP1_TX_LEN = 0;
    USBFSD->UEP3_TX_LEN = 0;

    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP3_TX_CTRL = USBFS_UEP_T_RES_NAK;

    for(uint8_t i=0; i<DEF_UEP_NUM; i++ )
    {
        USBFS_Endp_Busy[ i ] = 0;
    }
}

/*********************************************************************
 * @fn      USBFS_Device_Init
 *
 * @brief   Initializes USB device.
 *
 * @return  none
 */
void USBFS_Device_Init( FunctionalState sta )
{
    if( sta )
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFSH->BASE_CTRL = 0x00;
        USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
        USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
        USBFS_Device_Endp_Init( );
        USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
        NVIC_EnableIRQ( USBFS_IRQn );
    }
    else
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFSD->BASE_CTRL = 0x00;
        NVIC_DisableIRQ( USBFS_IRQn );
    }
}

/*********************************************************************
 * @fn      USBFS_Endp_DataUp
 *
 * @brief   USBFS device data upload
 *
 * @return  none
 */
uint8_t USBFS_Endp_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len, uint8_t mod)
{
    uint8_t endp_mode;
    uint8_t buf_load_offset;

    if( (endp>=DEF_UEP1) && (endp<=DEF_UEP7) )
    {
        if( USBFS_Endp_Busy[ endp ] == 0 )
        {
            if( (endp == DEF_UEP1) || (endp == DEF_UEP4) )
            {
                endp_mode = USBFSD_UEP_MOD(0);
                if( endp == DEF_UEP1 ) endp_mode = (uint8_t)(endp_mode>>4);
            }
            else if( (endp == DEF_UEP2) || (endp == DEF_UEP3) )
            {
                endp_mode = USBFSD_UEP_MOD(1);
                if( endp == DEF_UEP3 ) endp_mode = (uint8_t)(endp_mode>>4);
            }
            else if( (endp == DEF_UEP5) || (endp == DEF_UEP6) )
            {
                endp_mode = USBFSD_UEP_MOD(2);
                if( endp == DEF_UEP6 ) endp_mode = (uint8_t)(endp_mode>>4);
            }
            else
            {
                endp_mode = USBFSD_UEP_MOD(3);
            }

            if( endp_mode & USBFSD_UEP_TX_EN )
            {
                if( endp_mode & USBFSD_UEP_RX_EN ) buf_load_offset = 64;
                else buf_load_offset = 0;

                if( buf_load_offset == 0 )
                {
                    if( mod == DEF_UEP_DMA_LOAD )
                        USBFSD_UEP_DMA(endp) = (uint16_t)(uint32_t)pbuf;
                    else
                        memcpy( USBFSD_UEP_BUF(endp), pbuf, len );
                }
                else
                {
                    memcpy( USBFSD_UEP_BUF(endp)+buf_load_offset, pbuf, len );
                }

                USBFS_Endp_Busy[ endp ] = 0x01;
                USBFSD_UEP_TLEN(endp) = len;
                USBFSD_UEP_TX_CTRL(endp) = (USBFSD_UEP_TX_CTRL(endp) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            }
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }
    return 0;
}

/*********************************************************************
 * @fn      USBFS_IRQHandler
 *
 * @brief   This function handles HD-FS exception.
 *
 * @return  none
 */
void USBFS_IRQHandler( void )
{
    uint8_t  intflag, intst, errflag;
    uint16_t len;

    intflag = USBFSD->INT_FG;
    intst   = USBFSD->INT_ST;

    if( intflag & USBFS_UIF_TRANSFER )
    {
        switch (intst & USBFS_UIS_TOKEN_MASK)
        {
            /* data-in stage processing */
            case USBFS_UIS_TOKEN_IN:
                switch ( intst & ( USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK ) )
                {
                    case USBFS_UIS_TOKEN_IN | DEF_UEP0:
                        if( USBFS_SetupReqLen == 0 )
                        {
                            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                        }
                        if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                        {
                        }
                        else
                        {
                            switch( USBFS_SetupReqCode )
                            {
                                case USB_GET_DESCRIPTOR:
                                        len = USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                                        memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                                        USBFS_SetupReqLen -= len;
                                        pUSBFS_Descr += len;
                                        USBFSD->UEP0_TX_LEN   = len;
                                        USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                                        break;
                                case USB_SET_ADDRESS:
                                        USBFSD->DEV_ADDR = (USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) | USBFS_DevAddr;
                                        break;
                                default:
                                        break;
                            }
                        }
                        break;

                    case ( USBFS_UIS_TOKEN_IN | DEF_UEP1 ):
                        USBFSD->UEP1_TX_CTRL ^= USBFS_UEP_T_TOG;
                        USBFSD->UEP1_TX_CTRL = (USBFSD->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_Endp_Busy[ DEF_UEP1 ] = 0;
                        break;

                    case ( USBFS_UIS_TOKEN_IN | DEF_UEP3 ):
                        USBFSD->UEP3_TX_CTRL ^= USBFS_UEP_T_TOG;
                        USBFSD->UEP3_TX_CTRL = (USBFSD->UEP3_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_Endp_Busy[ DEF_UEP3 ] = 0;
                        break;

                    default:
                        break;
                }
                break;

            /* data-out stage processing */
            case USBFS_UIS_TOKEN_OUT:
                switch ( intst & ( USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK ) )
                {
                    case USBFS_UIS_TOKEN_OUT | DEF_UEP0:
                        len = USBFSD->RX_LEN;
                        if ( intst & USBFS_UIS_TOG_OK )
                        {
                            if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                            {
                                USBFS_SetupReqLen = 0;
                                /* Non-standard request end-point 0 Data download */
                                if( USBFS_SetupReqCode == CDC_SET_LINE_CODING )
                                {
                                    /* Store line coding (not wired to any UART) */
                                    CDC_LineCoding[ 0 ] = USBFS_EP0_Buf[ 0 ];
                                    CDC_LineCoding[ 1 ] = USBFS_EP0_Buf[ 1 ];
                                    CDC_LineCoding[ 2 ] = USBFS_EP0_Buf[ 2 ];
                                    CDC_LineCoding[ 3 ] = USBFS_EP0_Buf[ 3 ];
                                    CDC_LineCoding[ 4 ] = USBFS_EP0_Buf[ 4 ];
                                    CDC_LineCoding[ 5 ] = USBFS_EP0_Buf[ 5 ];
                                    CDC_LineCoding[ 6 ] = USBFS_EP0_Buf[ 6 ];
                                }
                            }
                            if( USBFS_SetupReqLen == 0 )
                            {
                                USBFSD->UEP0_TX_LEN  = 0;
                                USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                            }
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | DEF_UEP2:
                        USBFSD->UEP2_RX_CTRL ^= USBFS_UEP_R_TOG;
                        USBFSD->UEP2_RX_CTRL = (USBFSD->UEP2_RX_CTRL & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                        break;

                    default:
                        break;
                }
                break;

            /* Setup stage processing */
            case USBFS_UIS_TOKEN_SETUP:
                USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_NAK;
                USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_NAK;
                USBFS_SetupReqType  = pUSBFS_SetupReqPak->bRequestType;
                USBFS_SetupReqCode  = pUSBFS_SetupReqPak->bRequest;
                USBFS_SetupReqLen   = pUSBFS_SetupReqPak->wLength;
                USBFS_SetupReqValue = pUSBFS_SetupReqPak->wValue;
                USBFS_SetupReqIndex = pUSBFS_SetupReqPak->wIndex;
                len = 0;
                errflag = 0;
                if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                {
                    if( USBFS_SetupReqType & USB_REQ_TYP_CLASS )
                    {
                        switch( USBFS_SetupReqCode )
                        {
                            case CDC_GET_LINE_CODING:
                                pUSBFS_Descr = (uint8_t *)&CDC_LineCoding[ 0 ];
                                len = 7;
                                break;
                            case CDC_SET_LINE_CODING:
                                break;
                            case CDC_SET_LINE_CTLSTE:
                                break;
                            case CDC_SEND_BREAK:
                                break;
                            default:
                                errflag = 0xff;
                                break;
                        }
                    }
                    else if( USBFS_SetupReqType & USB_REQ_TYP_VENDOR )
                    {
                    }
                    else
                    {
                        errflag = 0xFF;
                    }

                    len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                    memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                    pUSBFS_Descr += len;
                }
                else
                {
                    switch( USBFS_SetupReqCode )
                    {
                        case USB_GET_DESCRIPTOR:
                            switch( (uint8_t)( USBFS_SetupReqValue >> 8 ) )
                            {
                                case USB_DESCR_TYP_DEVICE:
                                    pUSBFS_Descr = MyDevDescr;
                                    len = DEF_USBD_DEVICE_DESC_LEN;
                                    break;
                                case USB_DESCR_TYP_CONFIG:
                                    pUSBFS_Descr = MyCfgDescr;
                                    len = DEF_USBD_CONFIG_DESC_LEN;
                                    break;
                                case USB_DESCR_TYP_STRING:
                                    switch( (uint8_t)( USBFS_SetupReqValue & 0xFF ) )
                                    {
                                        case DEF_STRING_DESC_LANG:
                                            pUSBFS_Descr = MyLangDescr;
                                            len = DEF_USBD_LANG_DESC_LEN;
                                            break;
                                        case DEF_STRING_DESC_MANU:
                                            pUSBFS_Descr = MyManuInfo;
                                            len = DEF_USBD_MANU_DESC_LEN;
                                            break;
                                        case DEF_STRING_DESC_PROD:
                                            pUSBFS_Descr = MyProdInfo;
                                            len = DEF_USBD_PROD_DESC_LEN;
                                            break;
                                        case DEF_STRING_DESC_SERN:
                                            pUSBFS_Descr = MySerNumInfo;
                                            len = DEF_USBD_SN_DESC_LEN;
                                            break;
                                        default:
                                            errflag = 0xFF;
                                            break;
                                    }
                                    break;
                                default:
                                    errflag = 0xFF;
                                    break;
                            }

                            if( USBFS_SetupReqLen>len ) USBFS_SetupReqLen = len;
                            len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                            memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                            pUSBFS_Descr += len;
                            break;

                        case USB_SET_ADDRESS:
                            USBFS_DevAddr = (uint8_t)( USBFS_SetupReqValue & 0xFF );
                            break;

                        case USB_GET_CONFIGURATION:
                            USBFS_EP0_Buf[0] = USBFS_DevConfig;
                            if ( USBFS_SetupReqLen > 1 ) USBFS_SetupReqLen = 1;
                            break;

                        case USB_SET_CONFIGURATION:
                            USBFS_DevConfig = (uint8_t)( USBFS_SetupReqValue & 0xFF );
                            USBFS_DevEnumStatus = 0x01;
                            break;

                        case USB_CLEAR_FEATURE:
                            if ( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                            {
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_REMOTE_WAKEUP )
                                    USBFS_DevSleepStatus &= ~0x01;
                            }
                            else if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                            {
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_ENDP_HALT )
                                {
                                    switch( (uint8_t)( USBFS_SetupReqIndex & 0xFF ) )
                                    {
                                        case ( DEF_UEP_IN | DEF_UEP1 ): USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK; break;
                                        case ( DEF_UEP_OUT | DEF_UEP2 ): USBFSD->UEP2_RX_CTRL = USBFS_UEP_R_RES_ACK; break;
                                        case ( DEF_UEP_IN | DEF_UEP3 ): USBFSD->UEP3_TX_CTRL = USBFS_UEP_T_RES_NAK; break;
                                        default: errflag = 0xFF; break;
                                    }
                                }
                                else errflag = 0xFF;
                            }
                            else errflag = 0xFF;
                            break;

                        case USB_SET_FEATURE:
                            if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                            {
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_REMOTE_WAKEUP )
                                {
                                    if( MyCfgDescr[ 7 ] & 0x20 ) USBFS_DevSleepStatus |= 0x01;
                                    else errflag = 0xFF;
                                }
                                else errflag = 0xFF;
                            }
                            else if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                            {
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_ENDP_HALT )
                                {
                                    switch( (uint8_t)( USBFS_SetupReqIndex & 0xFF ) )
                                    {
                                        case ( DEF_UEP_IN | DEF_UEP1 ): USBFSD->UEP1_TX_CTRL = ( USBFSD->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK ) | USBFS_UEP_T_RES_STALL; break;
                                        case ( DEF_UEP_OUT | DEF_UEP2 ): USBFSD->UEP2_RX_CTRL = ( USBFSD->UEP2_RX_CTRL & ~USBFS_UEP_R_RES_MASK ) | USBFS_UEP_R_RES_STALL; break;
                                        case ( DEF_UEP_IN | DEF_UEP3 ): USBFSD->UEP3_TX_CTRL = ( USBFSD->UEP3_TX_CTRL & ~USBFS_UEP_T_RES_MASK ) | USBFS_UEP_T_RES_STALL; break;
                                        default: errflag = 0xFF; break;
                                    }
                                }
                                else errflag = 0xFF;
                            }
                            else errflag = 0xFF;
                            break;

                        case USB_GET_INTERFACE:
                            USBFS_EP0_Buf[0] = 0x00;
                            if ( USBFS_SetupReqLen > 1 ) USBFS_SetupReqLen = 1;
                            break;
                        case USB_SET_INTERFACE:
                            break;
                        case USB_GET_STATUS:
                            USBFS_EP0_Buf[ 0 ] = 0x00;
                            USBFS_EP0_Buf[ 1 ] = 0x00;
                            if ( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                            {
                                if( USBFS_DevSleepStatus & 0x01 ) USBFS_EP0_Buf[ 0 ] = 0x02;
                            }
                            else if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                            {
                                switch( (uint8_t)( USBFS_SetupReqIndex & 0xFF ) )
                                {
                                    case ( DEF_UEP_IN | DEF_UEP1 ):
                                        if( ( (USBFSD->UEP1_TX_CTRL) & USBFS_UEP_T_RES_MASK ) == USBFS_UEP_T_RES_STALL ) USBFS_EP0_Buf[ 0 ] = 0x01;
                                        break;
                                    case ( DEF_UEP_OUT | DEF_UEP2 ):
                                        if( ( (USBFSD->UEP2_RX_CTRL) & USBFS_UEP_R_RES_MASK ) == USBFS_UEP_R_RES_STALL ) USBFS_EP0_Buf[ 0 ] = 0x01;
                                        break;
                                    case ( DEF_UEP_IN | DEF_UEP3 ):
                                        if( ( (USBFSD->UEP3_TX_CTRL) & USBFS_UEP_T_RES_MASK ) == USBFS_UEP_T_RES_STALL ) USBFS_EP0_Buf[ 0 ] = 0x01;
                                        break;
                                    default: errflag = 0xFF; break;
                                }
                            }
                            else errflag = 0xFF;
                            if( USBFS_SetupReqLen > 2 ) USBFS_SetupReqLen = 2;
                            break;

                        default:
                            errflag = 0xFF;
                            break;
                    }
                }
                if( errflag == 0xff)
                {
                    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_STALL;
                    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_STALL;
                }
                else
                {
                    if( USBFS_SetupReqType & DEF_UEP_IN )
                    {
                        len = (USBFS_SetupReqLen>DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                        USBFS_SetupReqLen -= len;
                        USBFSD->UEP0_TX_LEN  = len;
                        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_ACK;
                    }
                    else
                    {
                        if( USBFS_SetupReqLen == 0 )
                        {
                            USBFSD->UEP0_TX_LEN  = 0;
                            USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_ACK;
                        }
                        else
                        {
                            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_ACK;
                        }
                    }
                }
                break;

            case USBFS_UIS_TOKEN_SOF:
                break;

            default:
                break;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }
    else if( intflag & USBFS_UIF_BUS_RST )
    {
        USBFS_DevConfig = 0;
        USBFS_DevAddr = 0;
        USBFS_DevSleepStatus = 0;
        USBFS_DevEnumStatus = 0;
        USBFSD->DEV_ADDR = 0;
        USBFS_Device_Endp_Init( );
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    }
    else if( intflag & USBFS_UIF_SUSPEND )
    {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
        Delay_Us(10);
        if ( USBFSD->MIS_ST & USBFS_UMS_SUSPEND )
        {
            USBFS_DevSleepStatus |= 0x02;
        }
        else
        {
            USBFS_DevSleepStatus &= ~0x02;
        }
    }
    else
    {
        USBFSD->INT_FG = intflag;
    }
}

/*********************************************************************
 * @fn      USBFS_Send_Resume
 *
 * @brief   USBFS device sends wake-up signal to host
 *
 * @return  none
 */
void USBFS_Send_Resume(void)
{
}

/*********************************************************************
 * @fn      CDC_SendData
 *
 * @brief   Push a buffer to the host via the CDC bulk-IN endpoint (UEP3).
 *          Only sends while the device is configured; if the endpoint is
 *          busy the data is dropped.
 *
 * @param   pbuf - pointer to data buffer.
 * @param   len  - data length (max DEF_USBD_ENDP3_SIZE).
 *
 * @return  0: queued, 1: busy/dropped, 2: not configured
 */
uint8_t CDC_SendData(uint8_t *pbuf, uint16_t len)
{
    if( USBFS_DevConfig == 0 )
        return 2;

    if( len > DEF_USBD_ENDP3_SIZE )
        len = DEF_USBD_ENDP3_SIZE;

    return USBFS_Endp_DataUp( DEF_UEP3, pbuf, len, DEF_UEP_CPY_LOAD );
}

/*********************************************************************
 * @fn      CDC_WriteChar
 *
 * @brief   Append a single char to the debug TX ring buffer.
 *          Non-blocking; drops the char when the ring is full.
 *
 * @return  none
 */
void CDC_WriteChar(char c)
{
    uint16_t next = (uint16_t)(s_cdc_tx_head + 1);
    if( next >= CDC_TX_BUF_SIZE ) next = 0;
    if( next == s_cdc_tx_tail ) return;
    s_cdc_tx_buf[ s_cdc_tx_head ] = (uint8_t)c;
    s_cdc_tx_head = next;
}

/*********************************************************************
 * @fn      CDC_WriteString
 *
 * @brief   Append a NUL-terminated string to the debug TX ring buffer.
 *
 * @return  none
 */
void CDC_WriteString(char *str)
{
    while( *str )
    {
        CDC_WriteChar( *str );
        str++;
    }
}

/*********************************************************************
 * @fn      CDC_Flush
 *
 * @brief   Drain the debug TX ring buffer to the CDC IN endpoint (UEP3).
 *          Sends up to one endpoint-worth of data per call when the
 *          endpoint is idle and the device is configured. Call this
 *          periodically from the main loop.
 *
 * @return  0: nothing left to send, 1: more data remains
 */
uint8_t CDC_Flush(void)
{
    uint16_t head = s_cdc_tx_head;
    uint16_t tail = s_cdc_tx_tail;

    if( head == tail ) return 0;

    if( USBFS_DevConfig == 0 ) return 1;

    uint16_t avail;
    if( head > tail ) avail = head - tail;
    else avail = (uint16_t)(CDC_TX_BUF_SIZE - tail + head);

    uint16_t chunk = (avail > DEF_USBD_ENDP3_SIZE) ? DEF_USBD_ENDP3_SIZE : avail;

    if( USBFS_Endp_DataUp( DEF_UEP3, (uint8_t *)&s_cdc_tx_buf[ tail ], chunk, DEF_UEP_CPY_LOAD ) )
        return 1;

    tail += chunk;
    if( tail >= CDC_TX_BUF_SIZE ) tail -= CDC_TX_BUF_SIZE;
    s_cdc_tx_tail = tail;

    return ( s_cdc_tx_head != s_cdc_tx_tail ) ? 1 : 0;
}
