cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(n64_adapter)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_executable(n64_adapter
    src/main.cpp
    )

target_link_libraries(n64_adapter pico_stdlib)

pico_enable_stdio_usb(n64_adapter 1)
pico_enable_stdio_uart(n64_adapter 0)

pico_add_extra_outputs(n64_adapter)