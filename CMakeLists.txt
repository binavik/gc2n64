cmake_minimum_required(VERSION 3.13)

IF(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
  set(PICO_SDK_FETCH_FROM_GIT on)
endif()

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(n64_adapter)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

include_directories(
    include
    )

add_executable(n64_adapter
    src/main.cpp
    src/gamecube_controller.cpp
    src/n64.cpp
    )

pico_generate_pio_header(n64_adapter 
    ${CMAKE_CURRENT_LIST_DIR}/pio/joybus.pio
    )

pico_generate_pio_header(n64_adapter 
    ${CMAKE_CURRENT_LIST_DIR}/pio/joybusN64.pio
    )


target_link_libraries(n64_adapter 
    pico_stdlib
    pico_multicore
    hardware_pio
    )

pico_enable_stdio_usb(n64_adapter 1)
pico_enable_stdio_uart(n64_adapter 0)

pico_add_extra_outputs(n64_adapter)
