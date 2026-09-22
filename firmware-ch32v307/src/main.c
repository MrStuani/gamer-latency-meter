/*
 * CH32V307 — USB HID 8kHz → GPIO: Bare-Metal Latency Measurement
 *
 * Single HID device (mouse or keyboard) on USBHS port, polling at 8kHz (125us/microframe).
 * GPIO outputs:
 *   PA0 — click level (HIGH = pressed)
 *   PA1 — motion pulse (TIM2 CH2 one-shot, ~10us, 100% hardware)
 *   PA2 — ISR debug toggle (measure ISR duration on oscilloscope)
 */

#include "usb_host_config.h"
#include "ch32v30x_usbfs_device.h"

/******************************************************************************/
/* Pin definitions */
#define PIN_CLICK       GPIO_Pin_0
#define PIN_MOTION      GPIO_Pin_1
#define PIN_DEBUG       GPIO_Pin_2

#define BOOT_MOUSE_LEN  3
#define BOOT_KEYB_LEN   8

/******************************************************************************/
/* Global variables */
ROOT_HUB_DEVICE  Dev = {0};
HOST_CTL         Ctl = {0};
uint8_t          DevDesc_Buf[ 18 ];
uint8_t          Com_Buf[ DEF_COM_BUF_LEN ];

volatile uint8_t g_button_state = 0;

__attribute__((aligned(4))) static uint8_t s_hid_buf[8];
static uint8_t s_ep_toggle = 0;

/* Setup requests: SetupGetDevDesc, SetupGetCfgDesc, SetupSetAddr, SetupSetConfig
   are already defined in ch32v30x_usbhs_host.h (guarded by DEF_USB_GEN_ENUM_CMD) */

/******************************************************************************/
/* GPIO Init — PA0/PA2 output, PA1 alternate function (TIM2 CH2) */
static void GPIO_Init_All(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = PIN_CLICK | PIN_DEBUG;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, PIN_CLICK);
    GPIO_ResetBits(GPIOA, PIN_DEBUG);

    GPIO_InitStructure.GPIO_Pin = PIN_MOTION;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOA, PIN_MOTION);
}

/******************************************************************************/
/* TIM2 PWM One-Shot — Motion pulse 100% hardware */
static void TIM2_Motion_Init(uint16_t pulse_us)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Period must be > pulse so PWM1 comparator turns the output LOW and the
       one-shot finishes in the LOW (idle) state instead of hanging HIGH. */
    TIM_TimeBaseStructure.TIM_Period = (pulse_us * 2) - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = pulse_us;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);

    TIM_SelectOnePulseMode(TIM2, TIM_OPMode_Single);
}

/******************************************************************************/
/* TIM3 8kHz — Heartbeat ISR trigger */
static void TIM3_Init_8KHz(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = 125 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = (SystemCoreClock / 1000000) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM3, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
}

/******************************************************************************/
/* TIM3 ISR — The critical path (~16us of 125us available) */
void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == RESET) return;
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

    GPIOA->BSHR = PIN_DEBUG;

    /* No HID interface enumerated yet (e.g. device disconnected) */
    if (Ctl.InterfaceNum == 0)
    {
        GPIOA->BCR = PIN_DEBUG;
        return;
    }

    uint16_t len;
    uint8_t s = USBHSH_GetEndpData(
        Ctl.Interface[0].InEndpAddr[0],
        &Ctl.Interface[0].InEndpTog[0],
        s_hid_buf, &len);

    if (s == ERR_SUCCESS)
    {
        if (Ctl.Interface[0].Type == DEC_MOUSE)
        {
            uint8_t btnMask = (Ctl.Interface[0].BtnBits >= 8)
                              ? 0xFF : (uint8_t)((1u << Ctl.Interface[0].BtnBits) - 1);
            uint8_t buttons = s_hid_buf[Ctl.Interface[0].BtnOffset] & btnMask;
            int8_t dx = (Ctl.Interface[0].XOffset == 0xFF)
                        ? 0 : (int8_t)s_hid_buf[Ctl.Interface[0].XOffset];
            int8_t dy = (Ctl.Interface[0].YOffset == 0xFF)
                        ? 0 : (int8_t)s_hid_buf[Ctl.Interface[0].YOffset];

            if (buttons)
            {
                if (!g_button_state)
                    GPIOA->BSHR = PIN_CLICK;
                g_button_state = 1;
            }
            else
            {
                if (g_button_state)
                    GPIOA->BCR = PIN_CLICK;
                g_button_state = 0;
            }

            if (dx != 0 || dy != 0)
            {
                TIM2->CH2CVR = 10;
                TIM2->CNT = 0;
                TIM2->SWEVGR |= TIM_UG;
                TIM2->CTLR1 |= TIM_CEN;
            }
        }
        else if (Ctl.Interface[0].Type == DEC_KEY)
        {
            uint8_t modifiers = s_hid_buf[0];
            uint8_t any_key = (modifiers != 0);

            if (!any_key)
            {
                for (uint8_t k = 2; k < 8; k++)
                {
                    if (s_hid_buf[k] != 0) { any_key = 1; break; }
                }
            }

            if (any_key)
            {
                if (!g_button_state)
                    GPIOA->BSHR = PIN_CLICK;
                g_button_state = 1;
            }
            else
            {
                if (g_button_state)
                    GPIOA->BCR = PIN_CLICK;
                g_button_state = 0;
            }
        }
    }
    else if (s == (USB_PID_STALL | ERR_USB_TRANSFER))
    {
        USBHSH_ClearEndpStall(Dev.bEp0MaxPks,
                              Ctl.Interface[0].InEndpAddr[0] | 0x80);
        Ctl.Interface[0].InEndpTog[0] = 0;
    }

    GPIOA->BCR = PIN_DEBUG;
}

/******************************************************************************/
/* HID report layout used by every mouse this firmware has seen:
 *   raw = 01 01 00 00 00 00 00 00  (click pressed)
 *   raw = 01 00 00 00 00 00 00 00  (click released)
 * i.e. byte0 = Report ID (always 0x01), byte1 = buttons, byte2 = X, byte3 = Y.
 * The motion byte travels with the report the same way on every mouse, so this
 * fixed layout is all the push-button part needs. */
static void HID_SetReportLayout(void)
{
    Ctl.Interface[0].ReportID  = 1;
    Ctl.Interface[0].BtnOffset = 1;
    Ctl.Interface[0].BtnBits   = 3;
    Ctl.Interface[0].XOffset   = 2;
    Ctl.Interface[0].YOffset   = 3;
    Ctl.Interface[0].ModOffset = 0;
    Ctl.Interface[0].KeyOffset = 2;
    printf("[hid] layout: ReportID=1, btn@1, X@2, Y@3\r\n");
}

/******************************************************************************/
/* Minimal Configuration Descriptor parser — find HID interface + IN endpoint */
static uint8_t Find_HID_EP_In(uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;

    while (i < len)
    {
        uint8_t dlen  = buf[i];
        uint8_t dtype = buf[i + 1];
        if (dlen == 0) break;

        if (dtype == 0x04)
        {
            PUSB_ITF_DESCR itf = (PUSB_ITF_DESCR)&buf[i];

            if (itf->bInterfaceClass == 0x03 &&
                itf->bInterfaceProtocol >= 1 &&
                itf->bInterfaceProtocol <= 2)
            {
                Ctl.Interface[0].Type =
                    (itf->bInterfaceProtocol == 1) ? DEC_KEY : DEC_MOUSE;

                uint16_t j = i + dlen;
                while (j < len)
                {
                    uint8_t elen  = buf[j];
                    uint8_t etype = buf[j + 1];
                    if (elen == 0) break;
                    if (etype == 0x04) break;

                    if (etype == 0x05)
                    {
                        PUSB_ENDP_DESCR ep = (PUSB_ENDP_DESCR)&buf[j];
                        if (ep->bEndpointAddress & 0x80)
                        {
                            Ctl.Interface[0].InEndpAddr[0] =
                                ep->bEndpointAddress & 0x0F;
                            Ctl.Interface[0].InEndpSize[0] =
                                ep->wMaxPacketSizeL |
                                ((uint16_t)ep->wMaxPacketSizeH << 8);
                            Ctl.Interface[0].InEndpInterval[0] =
                                ep->bInterval;
                            Ctl.Interface[0].InEndpNum = 1;
                            Ctl.InterfaceNum = 1;
                            return ERR_SUCCESS;
                        }
                    }
                    j += elen;
                }
            }
        }
        i += dlen;
    }
    return ERR_USB_UNSUPPORT;
}

/******************************************************************************/
/* Linear enumeration — runs once at startup, blocking */
static uint8_t Enumerate_Device(void)
{
    uint8_t s;
    uint8_t cnt = 0;
    uint16_t len;
    uint16_t i;
    uint8_t cfg_val;

RETRY:
    Delay_Ms(100);
    cnt++;
    Delay_Ms(8 << cnt);

    USBHSH_ResetRootHubPort(0);
    {
        uint8_t rc = 0;
        for (i = 0; i < 100; i++)
        {
            if (USBHSH_EnableRootHubPort(&Dev.bSpeed) == ERR_SUCCESS)
            {
                i = 0;
                rc++;
                if (rc > 6) break;
            }
            Delay_Ms(1);
        }
        if (i && cnt <= 5) goto RETRY;
        if (i) return ERR_USB_DISCON;
    }

    s = USBHSH_GetDeviceDescr(&Dev.bEp0MaxPks, DevDesc_Buf);
    if (s != ERR_SUCCESS && cnt <= 5) goto RETRY;
    if (s != ERR_SUCCESS) return s;

    Dev.bAddress = 0x02;
    s = USBHSH_SetUsbAddress(Dev.bEp0MaxPks, Dev.bAddress);
    if (s != ERR_SUCCESS && cnt <= 5) goto RETRY;
    if (s != ERR_SUCCESS) return s;
    Delay_Ms(5);

    s = USBHSH_GetConfigDescr(Dev.bEp0MaxPks, Com_Buf, sizeof(Com_Buf), &len);
    if (s != ERR_SUCCESS && cnt <= 5) goto RETRY;
    if (s != ERR_SUCCESS) return s;

    cfg_val = ((PUSB_CFG_DESCR)Com_Buf)->bConfigurationValue;

    s = USBHSH_SetUsbConfig(Dev.bEp0MaxPks, cfg_val);
    if (s != ERR_SUCCESS && cnt <= 5) goto RETRY;
    if (s != ERR_SUCCESS) return s;

    s = Find_HID_EP_In(Com_Buf, len);
    if (s != ERR_SUCCESS) return s;

    /* Fixed universal HID layout (all mice observed report the same):
     * byte0=Report ID(0x01), byte1=buttons, byte2=X, byte3=Y. */
    HID_SetReportLayout();

    Dev.bStatus = ROOT_DEV_SUCCESS;
    return ERR_SUCCESS;
}

/******************************************************************************/
/* Main */
int main(void)
{
    SystemCoreClockUpdate();
    Delay_Init();

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    USART_Printf_Init(115200);

    GPIO_Init_All();
    USBHS_RCC_Init();
    USBHS_Host_Init(ENABLE);

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE);

    printf("CH32V307 USB HID 8kHz Latency (CDC debug)\r\n");
    printf("Clk: %lu MHz\r\n", (unsigned long)(SystemCoreClock / 1000000));

    printf("Enumerating...\r\n");
    uint8_t s = Enumerate_Device();
    if (s == ERR_SUCCESS)
    {
        printf("OK: %s, EP IN=0x%x, interval=%d, speed=%s\r\n",
               Ctl.Interface[0].Type == DEC_MOUSE ? "Mouse" : "Keyboard",
               Ctl.Interface[0].InEndpAddr[0],
               Ctl.Interface[0].InEndpInterval[0],
               Dev.bSpeed == USB_HIGH_SPEED ? "HS" :
               Dev.bSpeed == USB_FULL_SPEED ? "FS" : "LS");
    }
    else
    {
        printf("FAIL: 0x%x\r\n", s);
        while(1);
    }

    TIM2_Motion_Init(10);
    TIM3_Init_8KHz();

    printf("Running @ 8kHz\r\n");

    uint8_t need_enum = 0;

    while (1)
    {
        uint8_t status = USBHSH_CheckRootHubPortStatus(Dev.bStatus);

        if (status == ROOT_DEV_DISCONNECT)
        {
            printf("Device removed\r\n");
            Dev.bStatus = ROOT_DEV_DISCONNECT;
            memset(&Dev, 0, sizeof(Dev));
            memset(&Ctl, 0, sizeof(Ctl));
            g_button_state = 0;
            GPIO_ResetBits(GPIOA, PIN_CLICK);

            /* Stop motion timer and force its output LOW (idle) */
            TIM2->CTLR1 &= ~TIM_CEN;
            TIM2->SWEVGR |= TIM_UG;

            need_enum = 1;
        }
        else if ((status == ROOT_DEV_CONNECTED) && need_enum)
        {
            printf("Device detected, re-enumerating...\r\n");
            s = Enumerate_Device();
            if (s == ERR_SUCCESS)
            {
                need_enum = 0;
                Dev.bStatus = ROOT_DEV_SUCCESS;
                printf("Reconnected: %s\r\n",
                       Ctl.Interface[0].Type == DEC_MOUSE ? "Mouse" : "KB");
            }
            else
            {
                printf("Re-enum failed 0x%x, retrying...\r\n", s);
                Delay_Ms(100);
            }
        }

        Delay_Ms(10);
        CDC_Flush();
    }
}
