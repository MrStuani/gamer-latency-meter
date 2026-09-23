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
#include "hid_report_parser.h"
#include "ch32v30x_usbfs_device.h"

/******************************************************************************/
/* Pin definitions */
#define PIN_CLICK       GPIO_Pin_0
#define PIN_MOTION      GPIO_Pin_1
#define PIN_DEBUG       GPIO_Pin_2

/* Boot-keyboard report is 8 bytes (modifier, reserved, 6 keycodes); it sizes
   s_hid_buf and bounds the key scan loop. */
#define BOOT_KEYB_LEN   8

/******************************************************************************/
/* Global variables */
ROOT_HUB_DEVICE  Dev = {0};
HOST_CTL         Ctl = {0};
uint8_t          DevDesc_Buf[ 18 ];
uint8_t          Com_Buf[ DEF_COM_BUF_LEN ];

volatile uint8_t g_button_state = 0;
volatile uint8_t g_click_armed = 0;

/* Generic HID reports can be bigger than the 8-byte boot format (Report ID +
   buttons + wheel + X/Y/z + vendors), so the polling buffer is 64 bytes. The
   keyboard scan loop stays bounded by BOOT_KEYB_LEN. */
#define HID_REPORT_BUF_LEN  64

/* CDC telemetry (debug/diag build). Mirrors in text what the firmware decides
   to do, so it can be validated over the serial link instead of on a scope. */
#define HID_CDC_DIAG 0

#if HID_CDC_DIAG
volatile uint16_t g_diag_len;     /* last decoded report length         */
volatile uint8_t  g_diag_id;      /* report byte 0 (Report ID / buttons) */
volatile uint8_t  g_diag_buttons; /* last decoded button mask           */
volatile int32_t  g_diag_dx;      /* last decoded X movement            */
volatile int32_t  g_diag_dy;      /* last decoded Y movement            */
volatile uint32_t g_diag_motion;  /* motion pulses since last print     */
volatile uint8_t  g_diag_armed;   /* click arming state                 */
volatile uint8_t  g_diag_dirty;   /* state changed since last print     */
volatile uint32_t g_diag_ok;      /* successful IN transactions         */
volatile uint32_t g_diag_nak;     /* NAK (idle) responses (0x2A)        */
volatile uint32_t g_diag_err;     /* other failed transactions          */
volatile uint8_t  g_diag_last_err;/* last non-success USB code          */
volatile uint32_t g_diag_age;     /* polls w/o success (8 == ~1 ms)     */
volatile uint16_t g_diag_burst;   /* frames left in plug-in burst       */
uint8_t           g_diag_raw[ HID_REPORT_BUF_LEN ]; /* last raw report   */
#endif

/* Live report-format detection. Many mice ACK SET_PROTOCOL(Boot) but keep
   sending Report-ID frames (byte0 = report id). When the descriptor parser
   failed (LayoutAuto == 0) and the first frames show a constant nonzero
   byte0 in 1..15 with no frame ever having byte0 == 0, the device is
   report-format: switch to the empirical 1-byte layout and filter by the
   detected id. Decided once per enumeration. */
volatile uint8_t g_det_report_id = 0; /* 0 = undetected                */
volatile uint8_t g_det_done    = 0;   /* decision made (switch or boot)*/
volatile uint8_t g_det_saw_zero = 0;  /* saw byte0 == 0 (real buttons) */
volatile uint8_t g_det_probe   = 0;   /* candidate report id           */
volatile uint8_t g_det_n       = 0;   /* consecutive equal cands       */

/* Motion suppression after enumeration (~50 ms): some mice emit a one-shot
   "pilot" motion burst right after plugging in. Just like clicks (which are
   already gated by g_click_armed), the motion output stays LOW until this
   window elapses so plugging a mouse provokes nothing on the scope. */
volatile uint16_t g_motion_suppress_ticks = 0; /* 400 ticks == 50 ms */

__attribute__((aligned(4))) static uint8_t s_hid_buf[HID_REPORT_BUF_LEN];
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
/* TIM2 PWM One-Shot — Motion pulse 100% hardware
 *
 * The rising edge of the motion pulse happens ~10 µs AFTER the moment the ISR
 * detects motion: the channel starts LOW (PWM1 with OCPolarity_Low) and the
 * CH2CCxR preload is kept at 10, so the comparator only turns the output HIGH
 * after ~10 timer ticks (1 µs prescaler). This ~10 µs is a systematic offset
 * of the measurement and is documented as such in the README. */
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

    if (g_motion_suppress_ticks)
        g_motion_suppress_ticks--;   /* time window since enumeration */

    uint16_t len;
    uint8_t s = USBHSH_GetEndpData(
        Ctl.Interface[0].InEndpAddr[0],
        &Ctl.Interface[0].InEndpTog[0],
        s_hid_buf, &len);

    if (s == ERR_SUCCESS)
    {
#if HID_CDC_DIAG
        g_diag_age = 0;
        g_diag_ok++;
#endif

        /* Only decode frames belonging to the expected Report ID (when the
           device uses one). Auxiliary reports (wheel/vendor) keep the last
           click/motion state instead of corrupting the offsets. */
        uint8_t frame_ok = (Ctl.Interface[0].ReportID == 0)
                           || (len >= 1 && s_hid_buf[0] == Ctl.Interface[0].ReportID);

        /* Live detection when there is no parsed layout (boot fallback) */
        if (frame_ok && len >= 1 && !Ctl.Interface[0].LayoutAuto && !g_det_done)
        {
            uint8_t b0 = s_hid_buf[0];

            if (b0 == 0)
            {
                g_det_saw_zero = 1;    /* boot mouse idle/motion frame */
                g_det_probe = 0;
                g_det_n = 0;
            }
            else if (b0 <= 15)
            {
                if (g_det_probe == 0)        { g_det_probe = b0; g_det_n = 1; }
                else if (b0 == g_det_probe)  { g_det_n++; }
                else                         { g_det_saw_zero = 1; g_det_probe = 0; g_det_n = 0; }

                if (g_det_n >= 4)
                {
                    g_det_done = 1;
                    if (!g_det_saw_zero)
                    {
                        g_det_report_id = g_det_probe;

                        if (Ctl.Interface[0].Type == DEC_MOUSE)
                        {
                            Ctl.Interface[0].BtnOffset = 1;
                            Ctl.Interface[0].BtnBits   = 8;
                            Ctl.Interface[0].XOffset   = 2;
                            Ctl.Interface[0].YOffset   = 3;
                        }
                        else
                        {
                            Ctl.Interface[0].ModOffset = 1;
                            Ctl.Interface[0].KeyOffset = 2;
                        }
                        Ctl.Interface[0].ReportID = g_det_report_id;
                        printf("[hid] live: report_id=%u, empirical layout\r\n",
                               g_det_report_id);
#if HID_CDC_DIAG
                        g_diag_dirty = 1;
#endif
                    }
                    g_det_probe = 0;
                    g_det_n = 0;
                }
            }
            else
            {
                g_det_saw_zero = 1;
                g_det_probe = 0;
                g_det_n = 0;
            }
        }

        if (frame_ok && Ctl.Interface[0].Type == DEC_MOUSE)
        {
            int32_t buttons;
            int32_t dx, dy;

            if (Ctl.Interface[0].LayoutAuto)
            {
                /* Automatic layout (HID Report Descriptor): offsets are in
                   BITS. Guard by comparing against the bytes actually
                   received (len * 8). Buttons read as one count-bit mask. */
                buttons = (Ctl.Interface[0].BtnValid &&
                           ((uint32_t)Ctl.Interface[0].BtnBitOff + Ctl.Interface[0].BtnBitCount)
                               <= ((uint32_t)len * 8u))
                          ? (hid_bits(s_hid_buf, Ctl.Interface[0].BtnBitOff,
                                      Ctl.Interface[0].BtnBitCount, 0) != 0)
                          : 0;
                dx = ((uint32_t)Ctl.Interface[0].XBitOff + Ctl.Interface[0].XBitSize)
                           <= ((uint32_t)len * 8u)
                     ? hid_bits(s_hid_buf, Ctl.Interface[0].XBitOff,
                                Ctl.Interface[0].XBitSize, Ctl.Interface[0].XSigned)
                     : 0;
                dy = ((uint32_t)Ctl.Interface[0].YBitOff + Ctl.Interface[0].YBitSize)
                           <= ((uint32_t)len * 8u)
                     ? hid_bits(s_hid_buf, Ctl.Interface[0].YBitOff,
                                Ctl.Interface[0].YBitSize, Ctl.Interface[0].YSigned)
                     : 0;
            }
            else
            {
                uint8_t btnMask = (Ctl.Interface[0].BtnBits >= 8)
                                  ? 0xFF : (uint8_t)((1u << Ctl.Interface[0].BtnBits) - 1);
                buttons = (Ctl.Interface[0].BtnOffset < len)
                          ? (s_hid_buf[Ctl.Interface[0].BtnOffset] & btnMask) : 0;
                dx = (Ctl.Interface[0].XOffset == 0xFF || Ctl.Interface[0].XOffset >= len)
                     ? 0 : (int8_t)s_hid_buf[Ctl.Interface[0].XOffset];
                dy = (Ctl.Interface[0].YOffset == 0xFF || Ctl.Interface[0].YOffset >= len)
                     ? 0 : (int8_t)s_hid_buf[Ctl.Interface[0].YOffset];
            }

#if HID_CDC_DIAG
            static uint16_t l_len = 0xFFFF;
            static uint8_t  l_id = 0xFF, l_btn = 0xFF;
            static int32_t  l_dx = 0x7FFFFFFF, l_dy = 0x7FFFFFFF;

            if (g_diag_burst)
            {
                g_diag_burst--;
                g_diag_dirty = 1;   /* log every frame during plug-in burst */
            }

            g_diag_len     = len;
            g_diag_id      = (len >= 1) ? s_hid_buf[0] : 0;
            g_diag_buttons = (uint8_t)buttons;
            g_diag_dx      = dx;
            g_diag_dy      = dy;
            if (l_len != g_diag_len || l_id != g_diag_id ||
                l_btn != g_diag_buttons || l_dx != g_diag_dx || l_dy != g_diag_dy)
            {
                memcpy(g_diag_raw, s_hid_buf,
                       (len <= HID_REPORT_BUF_LEN) ? len : HID_REPORT_BUF_LEN);
                g_diag_dirty = 1;
                l_len = g_diag_len; l_id = g_diag_id; l_btn = g_diag_buttons;
                l_dx = g_diag_dx;   l_dy = g_diag_dy;
            }
#endif

            if (buttons == 0) g_click_armed = 1;
            if (g_click_armed)
            {
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
            }
            else if (g_button_state)
            {
                GPIOA->BCR = PIN_CLICK;
                g_button_state = 0;
            }

#if HID_CDC_DIAG
            if (g_diag_armed != g_click_armed)
            {
                g_diag_armed = g_click_armed;
                g_diag_dirty = 1;
            }
#endif

            if (dx != 0 || dy != 0)
            {
                if (g_motion_suppress_ticks == 0)
                {
#if HID_CDC_DIAG
                    g_diag_motion++;
                    g_diag_dirty = 1;
#endif
                    /* CH2CCxR preload = 10 ticks (1 µs prescaler): the motion pulse
                       only rises ~10 µs after this write — systematic measurement
                       offset, documented in the README. */
                    TIM2->CH2CVR = 10;
                    TIM2->CNT = 0;
                    TIM2->SWEVGR |= TIM_UG;
                    TIM2->CTLR1 |= TIM_CEN;
                }
            }
        }
        else if (frame_ok && Ctl.Interface[0].Type == DEC_KEY)
        {
            uint8_t modifiers = (Ctl.Interface[0].ModOffset < len)
                                ? s_hid_buf[Ctl.Interface[0].ModOffset] : 0;
            uint8_t any_key = (modifiers != 0);

            if (!any_key)
            {
                for (uint8_t k = Ctl.Interface[0].KeyOffset; k < BOOT_KEYB_LEN; k++)
                {
                    if (k < len && s_hid_buf[k] != 0) { any_key = 1; break; }
                }
            }

            if (any_key == 0) g_click_armed = 1;
            if (g_click_armed)
            {
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
            else if (g_button_state)
            {
                GPIOA->BCR = PIN_CLICK;
                g_button_state = 0;
            }
        }
    }
    else
    {
#if HID_CDC_DIAG
        g_diag_age++;
        if (s == (uint8_t)(USB_PID_NAK | ERR_USB_TRANSFER))
            g_diag_nak++;
        else
        {
            g_diag_err++;
            g_diag_last_err = s;
        }
#endif

        if (s == (USB_PID_STALL | ERR_USB_TRANSFER))
        {
            USBHSH_ClearEndpStall(Dev.bEp0MaxPks,
                                  Ctl.Interface[0].InEndpAddr[0] | 0x80);
            Ctl.Interface[0].InEndpTog[0] = 0;
        }
    }

    GPIOA->BCR = PIN_DEBUG;
}

/******************************************************************************/
/* HID report layout.
 *
 * boot_protocol == 1: SET_PROTOCOL(Boot) succeeded, so the report is the fixed
 *   format from the HID 1.11 spec — mouse: [buttons, X, Y] (3 bytes, no Report
 *   ID); keyboard: [modifier, reserved, key0..5] (8 bytes).
 * boot_protocol == 0: DEV keeps its factory Report Protocol (SET_PROTOCOL not
 *   supported). The layout is the empirically observed one: a 0x01 Report ID
 *   byte in position 0, then buttons, X, Y. */
static void HID_SetReportLayout(uint8_t boot_protocol)
{
    Ctl.Interface[0].ModOffset = 0;
    Ctl.Interface[0].KeyOffset = 2;

    if (boot_protocol)
    {
        Ctl.Interface[0].ReportID  = 0;
        Ctl.Interface[0].BtnOffset = 0;
        Ctl.Interface[0].BtnBits   = 3;
        Ctl.Interface[0].XOffset   = 1;
        Ctl.Interface[0].YOffset   = 2;
        printf("[hid] layout (boot): btn@0, X@1, Y@2\r\n");
    }
    else
    {
        Ctl.Interface[0].ReportID  = 1;
        Ctl.Interface[0].BtnOffset = 1;
        Ctl.Interface[0].BtnBits   = 3;
        Ctl.Interface[0].XOffset   = 2;
        Ctl.Interface[0].YOffset   = 3;
        printf("[hid] layout (report): ReportID=1, btn@1, X@2, Y@3\r\n");
    }
}

/******************************************************************************/
/* Minimal Configuration Descriptor parser — find HID interface + IN endpoint */
static uint8_t Find_HID_EP_In(uint8_t *buf, uint16_t len)
{
    uint16_t i = 0;

    while (i + 1 < len)
    {
        uint8_t dlen  = buf[i];
        uint8_t dtype = buf[i + 1];
        if (dlen == 0 || i + dlen > len) break;

        if (dtype == 0x04)
        {
            PUSB_ITF_DESCR itf = (PUSB_ITF_DESCR)&buf[i];

            if (itf->bInterfaceClass == 0x03 &&
                itf->bInterfaceProtocol >= 1 &&
                itf->bInterfaceProtocol <= 2)
            {
                Ctl.Interface[0].Type =
                    (itf->bInterfaceProtocol == 1) ? DEC_KEY : DEC_MOUSE;
                Ctl.Interface[0].IntfNum = itf->bInterfaceNumber;

                uint16_t j = i + dlen;
                while (j + 1 < len)
                {
                    uint8_t elen  = buf[j];
                    uint8_t etype = buf[j + 1];
                    if (elen == 0 || j + elen > len) break;
                    if (etype == 0x04) break;

                    if (etype == DEF_DECR_HID)
                    {
                        /* HID descriptor: bLength, bType, bcdHID(2), bCountry,
                           bNumDescriptors, bClassDescType(0x22),
                           wDescriptorLength(2) — bytes 7 and 8. */
                        if (elen >= 9)
                        {
                            Ctl.Interface[0].ReportDescLen =
                                (uint16_t)buf[j + 7] |
                                ((uint16_t)buf[j + 8] << 8);
                        }
                        j += elen;
                        continue;
                    }

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
                            /* Ctl.InterfaceNum is set by Enumerate_Device only
                               at the very end, so the 8 kHz ISR does not poll
                               the IN endpoint while EP0 control transfers
                               (SET_PROTOCOL/SET_IDLE/GET_DESCRIPTOR) are still
                               in flight (they share the same host controller). */
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

    /* Arm the pilot-jolt window as soon as the interface goes live: on a
       re-enumeration the ISR is already polling while this function keeps
       running, and the device's plug-in burst can arrive in this gap. */
    g_motion_suppress_ticks = 400;   /* 50 ms of silent motion */

    /* PC-like behaviour: do NOT force Boot protocol on mice whose report
       descriptor we are going to parse. Every device tested acked
       SET_PROTOCOL(Boot) but keeps sending Report-ID frames anyway (fake-boot),
       and at least one mouse degrades to button-only reports while in Boot
       mode — it works normally on a PC, which leaves mice in the native Report
       protocol. The keyboard keeps the legacy Boot protocol. SET_IDLE(0) is
       kept: the device reports immediately on change. */
    uint8_t boot = 0;
    if (Ctl.Interface[0].Type != DEC_MOUSE)
    {
        boot = (HID_SetProtocol(Dev.bEp0MaxPks, Ctl.Interface[0].IntfNum)
                == ERR_SUCCESS);
        if (!boot)
            printf("[hid] warn: SET_PROTOCOL(Boot) failed, using report layout\r\n");
    }
    HID_SetIdle(Dev.bEp0MaxPks, Ctl.Interface[0].IntfNum, 0, 0);

    HID_SetReportLayout(boot);

    /* Automatic layout from the HID Report Descriptor (mouse only). When the
       parser recovers the X/Y/buttons bit positions, the ISR reads them through
       the *Bit fields; otherwise the fixed boot layout set above is kept as
       fallback. The keyboard always uses the boot layout. */
    if (Ctl.Interface[0].Type == DEC_MOUSE)
    {
        static uint8_t rdesc_buf[256];
        uint16_t want = (Ctl.Interface[0].ReportDescLen
                         && Ctl.Interface[0].ReportDescLen <= 256)
                        ? Ctl.Interface[0].ReportDescLen : 256;
        hid_mouse_layout_t layout;
        uint8_t rdesc_ok = 0;

        /* The descriptor read on EP0 is transiently flaky (it failed even on
           cold boots); retry a few times before falling back to the
           empirical layout. */
        for (uint8_t attempt = 0; attempt < 4 && !rdesc_ok; attempt++)
        {
            if (HID_GetReportDescr(Dev.bEp0MaxPks, Ctl.Interface[0].IntfNum,
                                   rdesc_buf, want, &len) == ERR_SUCCESS
                && HID_ParseMouseLayout(rdesc_buf, len, &layout) == ERR_SUCCESS)
            {
                rdesc_ok = 1;
            }
            else
            {
                Delay_Ms(20);
            }
        }

        if (rdesc_ok)
        {
            Ctl.Interface[0].ReportID    = layout.report_id;
            Ctl.Interface[0].LayoutAuto  = 1;
            Ctl.Interface[0].BtnValid    = layout.btn.valid;
            Ctl.Interface[0].BtnBitOff   = layout.btn.bit_off;
            Ctl.Interface[0].BtnBitSize  = layout.btn.bit_size;
            Ctl.Interface[0].BtnBitCount = layout.btn.count;
            Ctl.Interface[0].XBitOff     = layout.x.bit_off;
            Ctl.Interface[0].XBitSize    = layout.x.bit_size;
            Ctl.Interface[0].XSigned     = layout.x.is_signed;
            Ctl.Interface[0].YBitOff     = layout.y.bit_off;
            Ctl.Interface[0].YBitSize    = layout.y.bit_size;
            Ctl.Interface[0].YSigned     = layout.y.is_signed;

            printf("[hid] auto: report_id=%u btn{off=%u,count=%u} "
                   "X{off=%u,size=%u,signed=%u} Y{off=%u,size=%u,signed=%u}\r\n",
                   layout.report_id,
                   layout.btn.bit_off, layout.btn.count,
                   layout.x.bit_off, layout.x.bit_size, layout.x.is_signed,
                   layout.y.bit_off, layout.y.bit_size, layout.y.is_signed);
        }
        else
        {
            /* No report descriptor recovered. The mouse stays in its native
               Report protocol (like on a PC) — never force Boot here: some
               devices really switch to a degraded/garbage boot mode that the
               live format detection cannot undo (byte0 == 0). The empirical
               report layout is kept and live detection refines the Report ID
               from the actual frames. */
            printf("[hid] warn: descriptor parse failed, "
                   "using empirical report layout (rdesc_len=%u)\r\n",
                   (unsigned)Ctl.Interface[0].ReportDescLen);
#if HID_CDC_DIAG
            printf("[rdesc] ");
            for (i = 0; i < len && i < 256; i++)
                printf("%02X ", rdesc_buf[i]);
            printf("\r\n");
#endif
        }
    }

    /* Interface goes live only now: the whole enumeration (EP0 control
       transfers) has finished, so the 8 kHz ISR starts polling the IN
       endpoint from a clean state on both first boot and re-enumeration. */
    Ctl.InterfaceNum = 1;

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
        g_click_armed = 0;
        GPIO_ResetBits(GPIOA, PIN_CLICK);
#if HID_CDC_DIAG
        g_diag_ok = 0; g_diag_nak = 0; g_diag_err = 0;
        g_diag_last_err = 0; g_diag_age = 0;
        g_diag_burst = 40; g_diag_dirty = 1;
#endif
        g_det_report_id = 0; g_det_done = 0;
        g_det_saw_zero = 0; g_det_probe = 0; g_det_n = 0;
        g_motion_suppress_ticks = 400;   /* 50 ms of silent motion */
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
            g_det_report_id = 0; g_det_done = 0;
            g_det_saw_zero = 0; g_det_probe = 0; g_det_n = 0;
#if HID_CDC_DIAG
            g_diag_burst = 0;
            g_diag_dirty = 1;
#endif
g_button_state = 0;
                g_click_armed = 0;
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
                g_click_armed = 0;
                GPIO_ResetBits(GPIOA, PIN_CLICK);
#if HID_CDC_DIAG
                g_diag_ok = 0; g_diag_nak = 0; g_diag_err = 0;
                g_diag_last_err = 0; g_diag_age = 0;
                g_diag_burst = 40; g_diag_dirty = 1;
#endif
                g_det_report_id = 0; g_det_done = 0;
                g_det_saw_zero = 0; g_det_probe = 0; g_det_n = 0;
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

#if HID_CDC_DIAG
        static uint32_t diag_tick = 0;
        static uint32_t diag_last_ms = 0xFFFFFFF0u;
        static uint8_t  diag_idle_notified = 1;
        uint8_t diag_print = 0;

        if (g_diag_dirty)
            diag_idle_notified = 0;

        /* one line when the device goes quiet (> ~100 ms without data) */
        if (!diag_idle_notified && (g_diag_age > 800))
        {
            diag_idle_notified = 1;
            g_diag_dirty = 0;
            printf("[hb] idle: ok=%lu nak=%lu err=%lu e=0x%02X "
                   "age=%lums B=%u X=%ld Y=%ld\r\n",
                   (unsigned long)g_diag_ok, (unsigned long)g_diag_nak,
                   (unsigned long)g_diag_err, g_diag_last_err,
                   (unsigned long)(g_diag_age / 8),
                   g_diag_buttons, (long)g_diag_dx, (long)g_diag_dy);
        }

        if (g_diag_dirty)
        {
            uint32_t now = diag_tick;
            uint32_t min_dt = (g_diag_burst > 0) ? 2u : 50u;  /* 20ms burst, 500ms else */
            if ((now - diag_last_ms) >= min_dt)
            {
                diag_print = 1;
                diag_last_ms = now;
            }
        }
        diag_tick++;

        if (diag_print)
        {
            uint32_t mot = g_diag_motion;
            g_diag_motion = 0;
            g_diag_dirty = 0;

            printf("[diag] A=%u C=%u B=%u X=%ld Y=%ld M=%u l=%u id=%u\r\n",
                   g_diag_armed, g_button_state, g_diag_buttons,
                   (long)g_diag_dx, (long)g_diag_dy, (unsigned)mot,
                   g_diag_len, g_diag_id);
            printf("[raw] ");
            {
                uint8_t r;
                for (r = 0; r < g_diag_len && r < HID_REPORT_BUF_LEN; r++)
                    printf("%02X ", g_diag_raw[r]);
            }
            printf("\r\n");
        }
#endif
    }
}
