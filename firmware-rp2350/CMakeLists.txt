cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board")      # RP2350 (Pico 2). Troque se usar outra placa.

# Copie pico_sdk_import.cmake de $PICO_SDK_PATH/external/ para esta pasta.
include(pico_sdk_import.cmake)

project(medidor_rp2350 C CXX ASM)
set(CMAKE_C_STANDARD 11)

# Nao trava o core 1 por 500 ms se o host parar de ler a serial.
add_compile_definitions(PICO_STDIO_USB_STDOUT_TIMEOUT_US=20000)

pico_sdk_init()

add_executable(medidor main.c)
pico_generate_pio_header(medidor ${CMAKE_CURRENT_LIST_DIR}/edge_ts.pio)

target_link_libraries(medidor
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_gpio
    hardware_clocks
    hardware_sync
)

pico_enable_stdio_usb(medidor 1)
pico_enable_stdio_uart(medidor 0)
pico_add_extra_outputs(medidor)
