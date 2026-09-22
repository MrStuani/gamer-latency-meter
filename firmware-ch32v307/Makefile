TOOLCHAIN_DIR = E:\mount\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC
PREFIX  = riscv-none-embed-
CC      = $(TOOLCHAIN_DIR)\bin\$(PREFIX)gcc
AS      = $(TOOLCHAIN_DIR)\bin\$(PREFIX)gcc -x assembler-with-cpp
LD      = $(TOOLCHAIN_DIR)\bin\$(PREFIX)gcc
OBJCOPY = $(TOOLCHAIN_DIR)\bin\$(PREFIX)objcopy
SIZE    = $(TOOLCHAIN_DIR)\bin\$(PREFIX)size

TARGET  = ch32v307-usbhid-latency
DEFS    = -DCH32V30x_D8C
MCU     = -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -msave-restore
OPT     = -O2 -g
F_CPU   = -DSYSCLK_FREQ=144000000

INCLUDES = \
	-Isrc \
	-IUSB_Host \
	-IEVT_Support/Core \
	-IEVT_Support/Debug \
	-IEVT_Support/Peripheral/inc

CFLAGS  = $(MCU) $(DEFS) $(F_CPU) $(INCLUDES) $(OPT) \
	-Wall -Wno-unused-variable -Wno-unused-function \
	-fdata-sections -ffunction-sections

ASFLAGS = $(MCU) $(DEFS) $(INCLUDES) -Wall -fdata-sections -ffunction-sections

LDSCRIPT = EVT_Support/Ld/Link.ld
LDFLAGS  = $(MCU) -T$(LDSCRIPT) \
	-nostartfiles \
	-Wl,--gc-sections \
	-Wl,-Map=$(TARGET).map,--cref \
	-specs=nosys.specs -specs=nano.specs \
	-lc -lm -lnosys

SRC_ASM = EVT_Support/Startup/startup_ch32v30x_D8C.S

SRC_CORE = \
	EVT_Support/Core/core_riscv.c \
	EVT_Support/Debug/debug.c \
	src/system_ch32v30x.c

SRC_PERIPH = \
	EVT_Support/Peripheral/src/ch32v30x_gpio.c \
	EVT_Support/Peripheral/src/ch32v30x_rcc.c \
	EVT_Support/Peripheral/src/ch32v30x_tim.c \
	EVT_Support/Peripheral/src/ch32v30x_usart.c \
	EVT_Support/Peripheral/src/ch32v30x_misc.c

SRC_USB = \
	USB_Host/ch32v30x_usbhs_host.c \
	USB_Host/usb_host_hid.c

SRC_APP = \
	src/main.c \
	src/ch32v30x_it.c

SRC = $(SRC_CORE) $(SRC_PERIPH) $(SRC_USB) $(SRC_APP)
SRC_ASM_LIST = $(SRC_ASM)

OBJ  = $(SRC:.c=.o)
OBJ += $(SRC_ASM_LIST:.S=.o)

.PHONY: all clean size

all: $(TARGET).elf $(TARGET).hex $(TARGET).bin size

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.S
	$(AS) -c $(ASFLAGS) $< -o $@

$(TARGET).elf: $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary -S $< $@

size: $(TARGET).elf
	$(SIZE) $<

clean:
	del /Q /F $(OBJ) $(TARGET).elf $(TARGET).hex $(TARGET).bin $(TARGET).map 2>nul
