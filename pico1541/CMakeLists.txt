# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")
set(PI1541SRCDIR "../../src")
set(PICO2SRC pico1541.cpp hw_config.c)
set(PI1541SRC 
    ${PI1541SRCDIR}/main.cpp 
    ${PI1541SRCDIR}/Pi1541.cpp
    ${PI1541SRCDIR}/Pi1581.cpp
    ${PI1541SRCDIR}/iec_commands.cpp
    ${PI1541SRCDIR}/iec_bus.cpp
    ${PI1541SRCDIR}/Drive.cpp
    ${PI1541SRCDIR}/options.cpp
    ${PI1541SRCDIR}/m6522.cpp
    ${PI1541SRCDIR}/m6502.cpp
    ${PI1541SRCDIR}/m8520.cpp
    ${PI1541SRCDIR}/InputMappings.cpp
    ${PI1541SRCDIR}/ROMs.cpp
    ${PI1541SRCDIR}/DiskCaddy.cpp
    ${PI1541SRCDIR}/FileBrowser.cpp
    ${PI1541SRCDIR}/DiskImage.cpp
    ${PI1541SRCDIR}/gcr.cpp
    ${PI1541SRCDIR}/prot.cpp
    ${PI1541SRCDIR}/lz.c
    )

set(SDLIB lib/no-OS-FatFs)
# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico1541 C CXX ASM)
#set(PICO_COPY_TO_RAM 1)
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# generate version.h from git version
set(VERSION_H ${CMAKE_BINARY_DIR}/version.h)
add_custom_command(
    OUTPUT ${VERSION_H}
    COMMAND ${CMAKE_COMMAND} -E echo "Generating version.h"
    COMMAND echo "\\#define PPI1541VERSION \\"`git describe --tags`"" >/tmp/__version_cmp
    COMMAND cmp -s /tmp/__version_cmp ${VERSION_H} || echo "\\#define PPI1541VERSION \\"`git describe --tags`"" >${VERSION_H}
    #COMMENT "Generating header file..." ${VERSION_H}
)

add_compile_options(-D__PICO2__ -DEXPERIMENTALZERO -DPICO_DEBUG_MALLOC)
add_executable(pico1541 ${PICO2SRC} ${PI1541SRC} ${VERSION_H})
add_subdirectory(lib/no-OS-FatFs/src build)
pico_set_program_name(pico1541 "pico1541")
pico_set_program_version(pico1541 "0.1")

# Generate PIO header
#pico_generate_pio_header(pico1541 ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico1541 0)
pico_enable_stdio_usb(pico1541 1)

# Add the standard library to the build
target_link_libraries(pico1541
        pico_stdlib
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
        )

# Add the standard include files to the build
target_include_directories(pico1541 PRIVATE
    BEFORE
    ${CMAKE_CURRENT_LIST_DIR}
    AFTER
    ${SDLIB}/src/ff15/source
    ${SDLIB}/src/include
    ${SDLIB}/src/sd_driver
    ${CMAKE_BINARY_DIR}
)

# Add any user requested libraries
target_link_libraries(pico1541 
        hardware_spi
        hardware_pio
        )
if (PICO_CYW43_SUPPORTED)
    target_link_libraries(pico1541 pico_cyw43_arch_none)
endif()
    
pico_add_extra_outputs(pico1541)

