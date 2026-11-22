option(GP100_INCLUDE_CABINET_ASSETS
    "Pack cabinet and rhythm assets into the image" ON)
option(GP100_AUTOSTART_AUDIO
    "Start the recovered 44.1 kHz SAI/eDMA path during normal boot" ON)
option(GP100_AUTOSTART_TUNER_CAPTURE
    "Start the recovered continuous GPIO/QTMR tuner capture during boot" ON)

set(MCUX_SDK_ROOT "" CACHE PATH "Path to MCUXpresso SDK 2.11.0")
set(MCUX_USB_ROOT "" CACHE PATH
    "Path to NXP mcuxsdk-middleware-usb tag MCUX_2.11.0")
if(NOT EXISTS "${MCUX_SDK_ROOT}/devices/MIMXRT1052/MIMXRT1052.h")
    message(FATAL_ERROR
        "MCUX_SDK_ROOT must point to an MCUXpresso SDK containing MIMXRT1052")
endif()
if(NOT EXISTS "${MCUX_USB_ROOT}/device/usb_device_ehci.c" OR
   NOT EXISTS "${MCUX_USB_ROOT}/output/source/device/class/usb_device_audio.c")
    message(FATAL_ERROR
        "MCUX_USB_ROOT must point to mcuxsdk-middleware-usb MCUX_2.11.0")
endif()
if(NOT EXISTS "${MCUX_SDK_ROOT}/components/osa/fsl_os_abstraction_bm.c" OR
   NOT EXISTS "${MCUX_SDK_ROOT}/components/lists/fsl_component_generic_list.c")
    message(FATAL_ERROR
        "The SDK checkout needs components/osa and components/lists")
endif()

set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard)

set(SDK ${MCUX_SDK_ROOT})
set(SOURCES
    app/main.c
    board/input/analog_acquisition.c
    audio/transport/audio_transport.c
    board/board.c
    board/board_components.c
    display/display_transport.c
    protocol/midi/midi_input.c
    board/pin_mux.c
    board/peripherals.c
    rhythm/rhythm_clock.c
    audio/codec/codec_i2c.c
    audio/codec/codec_startup.c
    audio/codec/es7243.c
    audio/codec/es8156.c
    display/gp100_display.c
    board/boot_header.c
    board/dcd.c
    ${SDK}/devices/MIMXRT1052/gcc/startup_MIMXRT1052.S
    ${SDK}/devices/MIMXRT1052/system_MIMXRT1052.c
    ${SDK}/devices/MIMXRT1052/xip/fsl_flexspi_nor_boot.c
    ${SDK}/devices/MIMXRT1052/drivers/fsl_clock.c
    ${SDK}/drivers/common/fsl_common.c
    ${SDK}/drivers/common/fsl_common_arm.c
    ${SDK}/drivers/igpio/fsl_gpio.c
    ${SDK}/drivers/adc_12b1msps_sar/fsl_adc.c
    ${SDK}/drivers/adc_etc/fsl_adc_etc.c
    ${SDK}/drivers/dmamux/fsl_dmamux.c
    ${SDK}/drivers/edma/fsl_edma.c
    ${SDK}/drivers/flexspi/fsl_flexspi.c
    ${SDK}/drivers/lpi2c/fsl_lpi2c.c
    ${SDK}/drivers/lpspi/fsl_lpspi.c
    ${SDK}/drivers/lpuart/fsl_lpuart.c
    ${SDK}/drivers/pit/fsl_pit.c
    ${SDK}/drivers/qtmr_1/fsl_qtmr.c
    ${SDK}/drivers/sai/fsl_sai.c
    ${SDK}/drivers/xbara/fsl_xbara.c)

add_executable(gp100_base.elf ${SOURCES})

target_link_libraries(gp100_base.elf PRIVATE gp100_core)
target_sources(gp100_base.elf PRIVATE
    app/application.c
    assets/ui/iq_boot_logo.c
    audio/engine/audio_binding.c
    board/input/control_input.c
    board/input/tuner_capture.c
    protocol/usb/usb_device_adapter.c
    ${MCUX_USB_ROOT}/device/usb_device_dci.c
    ${MCUX_USB_ROOT}/device/usb_device_ehci.c
    ${MCUX_USB_ROOT}/phy/usb_phy.c
    ${MCUX_USB_ROOT}/output/source/device/usb_device_ch9.c
    ${MCUX_USB_ROOT}/output/source/device/class/usb_device_class.c
    ${MCUX_USB_ROOT}/output/source/device/class/usb_device_audio.c
    ${SDK}/components/osa/fsl_os_abstraction_bm.c
    ${SDK}/components/lists/fsl_component_generic_list.c
    ${SDK}/drivers/cache/armv7-m7/fsl_cache.c
    board/flash_storage.c
    ${SDK}/devices/MIMXRT1052/drivers/fsl_romapi.c)

if(GP100_AUTOSTART_AUDIO)
    target_compile_definitions(gp100_base.elf PRIVATE
        GP100_AUTOSTART_AUDIO=1)
    # The 64-frame audio callback must meet its deadline even in a build
    # retaining SWD symbols. Keep the host library's optimization unchanged.
    set_source_files_properties(audio/engine/audio_binding.c audio/transport/audio_transport.c
        PROPERTIES COMPILE_OPTIONS -O3)
    target_compile_options(gp100_core PRIVATE -O3)
endif()

if(GP100_AUTOSTART_TUNER_CAPTURE)
    target_compile_definitions(gp100_base.elf PRIVATE
        GP100_AUTOSTART_TUNER_CAPTURE=1)
endif()

target_include_directories(gp100_base.elf PRIVATE
    protocol/usb
    ${MCUX_USB_ROOT}/include
    ${MCUX_USB_ROOT}/device
    ${MCUX_USB_ROOT}/phy
    ${MCUX_USB_ROOT}/output/source/device
    ${MCUX_USB_ROOT}/output/source/device/class
    ${SDK}/components/osa
    ${SDK}/components/lists
    ${SDK}/drivers/cache/armv7-m7
    "${CMAKE_CURRENT_SOURCE_DIR}"
    board
    ${SDK}/CMSIS/Include
    ${SDK}/devices/MIMXRT1052
    ${SDK}/devices/MIMXRT1052/drivers
    ${SDK}/devices/MIMXRT1052/xip
    ${SDK}/drivers/common
    ${SDK}/drivers/igpio
    ${SDK}/drivers/adc_12b1msps_sar
    ${SDK}/drivers/adc_etc
    ${SDK}/drivers/dmamux
    ${SDK}/drivers/edma
    ${SDK}/drivers/flexspi
    ${SDK}/drivers/lpi2c
    ${SDK}/drivers/lpspi
    ${SDK}/drivers/lpuart
    ${SDK}/drivers/pit
    ${SDK}/drivers/qtmr_1
    ${SDK}/drivers/sai
    ${SDK}/drivers/xbara)

target_compile_definitions(gp100_base.elf PRIVATE
    CPU_MIMXRT1052DVL6B
    XIP_EXTERNAL_FLASH=1
    XIP_BOOT_HEADER_ENABLE=1
    XIP_BOOT_HEADER_DCD_ENABLE=1
    BOARD_FLASH_SIZE=0x00800000
    __START=main
    __STARTUP_CLEAR_BSS
    __STARTUP_INITIALIZE_RAMFUNCTION
    __STARTUP_INITIALIZE_NONCACHEDATA)

target_compile_options(gp100_base.elf PRIVATE
    ${CPU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
    -Werror=implicit-function-declaration
    $<$<COMPILE_LANGUAGE:C>:-std=c11>)

target_link_options(gp100_base.elf PRIVATE
    ${CPU_FLAGS}
    -nostartfiles
    --specs=nano.specs
    --specs=nosys.specs
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/gp100_flexspi.ld
    -Wl,--gc-sections
    -Wl,--build-id=none
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/gp100_base.map)

set_target_properties(gp100_base.elf PROPERTIES
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/gp100_flexspi.ld;${CMAKE_CURRENT_BINARY_DIR}/CMakeCache.txt")

add_custom_command(TARGET gp100_base.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:gp100_base.elf>
            "${CMAKE_CURRENT_BINARY_DIR}/gp100_base.bin"
    COMMENT "Create standalone FlexSPI boot image gp100_base.bin"
    VERBATIM)

if(GP100_INCLUDE_CABINET_ASSETS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/factory_assets.cmake")
endif()

if(BUILD_TESTING)
    add_test(NAME image_layout
        COMMAND python3 -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_image.py
                ${CMAKE_CURRENT_BINARY_DIR}/gp100_base.bin
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
                ${GP100_IMAGE_CHECK_ASSETS})
    add_test(NAME sdram_layout
        COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_sdram.py
                ${CMAKE_CURRENT_BINARY_DIR}/gp100_base.elf
                ${CMAKE_NM})

    if(GP100_AUTOSTART_AUDIO AND GP100_AUTOSTART_TUNER_CAPTURE)
        get_target_property(GP100_CORE_SOURCES gp100_core SOURCES)
        list(FILTER GP100_CORE_SOURCES INCLUDE REGEX "[.]c$")
        add_test(NAME core_link
            COMMAND python3
                    ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_core_link.py
                    ${CMAKE_CURRENT_BINARY_DIR}/gp100_base.map
                    ${GP100_CORE_SOURCES})
    endif()
endif()
