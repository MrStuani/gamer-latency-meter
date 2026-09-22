cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(medidor_latencia C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plataforma-alvo: Raspberry Pi Pico 2 (RP2350).
# Valores default; sobrescrevíveis por linha de comando:
#   -DPICO_PLATFORM=rp2350-arm-s   (Arm Cortex-M33)  [default SDK master]
#   -DPICO_BOARD=pico2
set(PICO_BOARD pico2 CACHE STRING "Raspberry Pi Pico 2")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "RP2350 arm")
# C++ exceptions/rt neok nas recomendações do SDK para ganhar flash.
set(PICO_NO_RTTI 1)
set(PICO_NO_EXCEPTIONS 1)
# Mantém o TinyUSB montado mesmo sem host conectado (pinos T0/T1 livres).
set(PICO_STDIO_USB_CONNECT_WITHOUT_CONFIGURATION 1)

pico_sdk_init()

add_executable(medidor_latencia
    src/config.h
    src/measure.h
    src/main.c
    src/measure.c
    pio/timestamp.pio
)

pico_generate_pio_header(medidor_latencia ${CMAKE_CURRENT_LIST_DIR}/pio/timestamp.pio)

target_include_directories(medidor_latencia PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(medidor_latencia
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
)

# Console do medidor = CDC USB. UART desligada para não gastar pinos.
pico_enable_stdio_usb(medidor_latencia 1)
pico_enable_stdio_uart(medidor_latencia 0)

pico_add_extra_outputs(medidor_latencia)