# build.ps1 - Build script for CH32V307 USB HID Latency firmware
# Usage: .\build.ps1          (compile)
#        .\build.ps1 clean    (clean build artifacts)

$ErrorActionPreference = "Stop"

# Toolchain
$TOOLCHAIN = "E:\mount\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin"
$CC    = "$TOOLCHAIN\riscv-none-embed-gcc.exe"
$AS    = "$TOOLCHAIN\riscv-none-embed-gcc.exe"
$LD    = "$TOOLCHAIN\riscv-none-embed-gcc.exe"
$OBJCOPY = "$TOOLCHAIN\riscv-none-embed-objcopy.exe"
$SIZE  = "$TOOLCHAIN\riscv-none-embed-size.exe"

$TARGET = "ch32v307-usbhid-latency"

# Flags
$DEFS   = "-DCH32V30x_D8C -DCDC_PRINT"
$MCU    = "-march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -msave-restore"
$OPT    = "-O2 -g"
$F_CPU  = "-DSYSCLK_FREQ=144000000"

$INCLUDES = @(
    "-Isrc",
    "-IUSB_Host",
    "-IUSB_Device",
    "-IEVT_Support/Core",
    "-IEVT_Support/Debug",
    "-IEVT_Support/Peripheral/inc"
) -join " "

$CFLAGS = "$MCU $DEFS $F_CPU $INCLUDES $OPT -Wall -Wno-unused-variable -Wno-unused-function -fdata-sections -ffunction-sections"
$ASFLAGS = "$MCU $DEFS $INCLUDES -Wall -fdata-sections -ffunction-sections"

$LDSCRIPT = "EVT_Support\Ld\Link.ld"
$LDFLAGS = "$MCU -T$LDSCRIPT -nostartfiles -Wl,--gc-sections -Wl,-Map=$TARGET.map,--cref -specs=nosys.specs -specs=nano.specs -lc -lm -lnosys"

# Handle clean
if ($args -contains "clean") {
    Write-Host "Cleaning..." -ForegroundColor Yellow
    Get-ChildItem -Recurse -Filter "*.o" | Remove-Item -Force -ErrorAction SilentlyContinue
    Remove-Item -Force "$TARGET.elf", "$TARGET.hex", "$TARGET.bin", "$TARGET.map" -ErrorAction SilentlyContinue
    Write-Host "Done." -ForegroundColor Green
    exit 0
}

# Source files
$C_SOURCES = @(
    "EVT_Support\Core\core_riscv.c",
    "EVT_Support\Debug\debug.c",
    "src\system_ch32v30x.c",
    "EVT_Support\Peripheral\src\ch32v30x_gpio.c",
    "EVT_Support\Peripheral\src\ch32v30x_rcc.c",
    "EVT_Support\Peripheral\src\ch32v30x_tim.c",
    "EVT_Support\Peripheral\src\ch32v30x_usart.c",
    "EVT_Support\Peripheral\src\ch32v30x_misc.c",
    "USB_Host\ch32v30x_usbhs_host.c",
    "USB_Host\usb_host_hid.c",
    "USB_Device\ch32v30x_usbfs_device.c",
    "USB_Device\usb_desc.c",
    "src\main.c",
    "src\ch32v30x_it.c"
)

$ASM_SOURCES = @(
    "EVT_Support\Startup\startup_ch32v30x_D8C.S"
)

# Compile C sources
Write-Host "=== Compiling C sources ===" -ForegroundColor Cyan
$objects = @()
foreach ($src in $C_SOURCES) {
    $obj = $src -replace '\.c$', '.o'
    $objects += $obj
    Write-Host "  CC  $src"
    & $CC -c $CFLAGS.Split(" ") $src -o $obj
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $src" -ForegroundColor Red
        exit 1
    }
}

# Compile ASM sources
Write-Host "=== Compiling ASM sources ===" -ForegroundColor Cyan
foreach ($src in $ASM_SOURCES) {
    $obj = $src -replace '\.S$', '.o'
    $objects += $obj
    Write-Host "  AS  $src"
    & $AS -c $ASFLAGS.Split(" ") $src -o $obj
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED: $src" -ForegroundColor Red
        exit 1
    }
}

# Link
Write-Host "=== Linking ===" -ForegroundColor Cyan
& $LD $objects $LDFLAGS.Split(" ") -o "$TARGET.elf"
if ($LASTEXITCODE -ne 0) {
    Write-Host "LINK FAILED" -ForegroundColor Red
    exit 1
}

# Generate hex and bin
Write-Host "=== Generating HEX/BIN ===" -ForegroundColor Cyan
& $OBJCOPY -O ihex "$TARGET.elf" "$TARGET.hex"
& $OBJCOPY -O binary -S "$TARGET.elf" "$TARGET.bin"

# Print size
Write-Host "=== Size ===" -ForegroundColor Cyan
& $SIZE "$TARGET.elf"

Write-Host "`nBuild SUCCESS: $TARGET.elf / .hex / .bin" -ForegroundColor Green
