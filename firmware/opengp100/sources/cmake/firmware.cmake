option(GP100_INCLUDE_CABINET_ASSETS
    "Pack cabinet and rhythm assets into the standalone image" ON)
option(GP100_AUTOSTART_AUDIO
    "Start the recovered 44.1 kHz SAI/eDMA path during normal boot" ON)
option(GP100_AUTOSTART_TUNER_CAPTURE
    "Start the recovered continuous GPIO/QTMR tuner capture during boot" ON)
set(GP100_STOCK_UPDATE_TEMPLATE "" CACHE FILEPATH
    "Optional Valeton GP-100 v2.1 BIN for wrapping the stock slot as a USB update")

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

if(GP100_AUTOSTART_AUDIO)
    # The 64-frame audio callback must still meet its deadline in debug builds.
    set_source_files_properties(audio/engine/audio_binding.c audio/transport/audio_transport.c
        PROPERTIES COMPILE_OPTIONS -O3)
    target_compile_options(gp100_core PRIVATE -O3)
endif()

function(gp100_add_firmware name boot_dcd)
    set(firmware "${name}.elf")
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
        ${boot_dcd}
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

    add_executable(${firmware} ${SOURCES})

    target_link_libraries(${firmware} PRIVATE gp100_core)
    target_sources(${firmware} PRIVATE
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
        target_compile_definitions(${firmware} PRIVATE GP100_AUTOSTART_AUDIO=1)
    endif()

    if(GP100_AUTOSTART_TUNER_CAPTURE)
        target_compile_definitions(${firmware} PRIVATE
            GP100_AUTOSTART_TUNER_CAPTURE=1)
    endif()

    target_include_directories(${firmware} PRIVATE
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

    target_compile_definitions(${firmware} PRIVATE
        CPU_MIMXRT1052DVL6B
        XIP_EXTERNAL_FLASH=1
        BOARD_FLASH_SIZE=0x00800000
        __START=main
        __STARTUP_CLEAR_BSS
        __STARTUP_INITIALIZE_RAMFUNCTION
        __STARTUP_INITIALIZE_NONCACHEDATA)

    target_compile_options(${firmware} PRIVATE
        ${CPU_FLAGS}
        -ffunction-sections
        -fdata-sections
        -fno-common
        -Wall
        -Wextra
        -Werror=implicit-function-declaration
        $<$<COMPILE_LANGUAGE:C>:-std=c11>)

    target_link_options(${firmware} PRIVATE
        ${CPU_FLAGS}
        -nostartfiles
        --specs=nano.specs
        --specs=nosys.specs
        -T${CMAKE_CURRENT_SOURCE_DIR}/linker/gp100_flexspi.ld
        -Wl,--gc-sections
        -Wl,--build-id=none
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${name}.map)

    set_target_properties(${firmware} PROPERTIES
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/linker/gp100_flexspi.ld;${CMAKE_CURRENT_BINARY_DIR}/CMakeCache.txt")

endfunction()

gp100_add_firmware(gp100_base board/dcd.c)
target_compile_definitions(gp100_base.elf PRIVATE
    XIP_BOOT_HEADER_ENABLE=1
    XIP_BOOT_HEADER_DCD_ENABLE=1)

gp100_add_firmware(gp100_stock_slot "")
target_sources(gp100_stock_slot.elf PRIVATE board/stock_slot_entry.S)
target_link_options(gp100_stock_slot.elf PRIVATE -Wl,--defsym=__stock_slot_build=1)

include(cmake/factory_assets.cmake)

set(standalone "${CMAKE_CURRENT_BINARY_DIR}/gp100_base.bin")
if(GP100_INCLUDE_CABINET_ASSETS)
    set(standalone_asset_command
        COMMAND python3 -B "${GP100_IMAGE_COMPOSER}" "${standalone}" "${GP100_ASSET_DIR}")
    set(standalone_asset_dependencies
        "${GP100_IMAGE_COMPOSER}" "${CMAKE_CURRENT_SOURCE_DIR}/storage/flash_layout.h"
        ${GP100_FACTORY_BANKS})
    set(GP100_IMAGE_CHECK_ASSETS --assets "${GP100_ASSET_DIR}")
endif()
add_custom_command(OUTPUT "${standalone}"
    COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:gp100_base.elf> "${standalone}"
    ${standalone_asset_command}
    DEPENDS gp100_base.elf ${standalone_asset_dependencies}
    COMMENT "Create standalone FlexSPI image gp100_base.bin"
    VERBATIM)
add_custom_target(gp100_base ALL DEPENDS "${standalone}")
if(GP100_INCLUDE_CABINET_ASSETS)
    add_dependencies(gp100_base gp100_factory_banks)
endif()

set(raw_slot "${CMAKE_CURRENT_BINARY_DIR}/gp100_stock_slot.bin")
set(payload "${CMAKE_CURRENT_BINARY_DIR}/gp100_stock_slot_payload.bin")
set(packer "${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_stock_update.py")
if(GP100_STOCK_UPDATE_TEMPLATE)
    set(update "${CMAKE_CURRENT_BINARY_DIR}/OpenGP100-stock-update.bin")
    set(update_arguments --template "${GP100_STOCK_UPDATE_TEMPLATE}" --update "${update}")
endif()
add_custom_command(OUTPUT "${raw_slot}" ${update}
    BYPRODUCTS "${payload}"
    COMMAND "${CMAKE_OBJCOPY}" -O binary --gap-fill=0xff
            $<TARGET_FILE:gp100_stock_slot.elf> "${payload}"
    COMMAND python3 -B "${packer}" "${payload}" "${GP100_ASSET_DIR}" "${raw_slot}"
            ${update_arguments}
    DEPENDS gp100_stock_slot.elf "${packer}" "${GP100_IMAGE_COMPOSER}"
            ${GP100_FACTORY_BANKS} ${GP100_STOCK_UPDATE_TEMPLATE}
    COMMENT "Create stock application slot image gp100_stock_slot.bin"
    VERBATIM)
add_custom_target(gp100_stock_slot ALL DEPENDS "${raw_slot}" ${update})
add_dependencies(gp100_stock_slot gp100_factory_banks)

if(BUILD_TESTING)
    add_test(NAME image_layout
        COMMAND python3 -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_image.py"
                "${standalone}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
                ${GP100_IMAGE_CHECK_ASSETS})
    if(GP100_AUTOSTART_AUDIO AND GP100_AUTOSTART_TUNER_CAPTURE AND NOT GP100_EFFECT_PROFILE)
        get_target_property(GP100_CORE_SOURCES gp100_core SOURCES)
        list(FILTER GP100_CORE_SOURCES INCLUDE REGEX "[.]c$")
    endif()

    foreach(image IN ITEMS gp100_base gp100_stock_slot)
        add_test(NAME ${image}_sdram_layout
            COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_sdram.py"
                    $<TARGET_FILE:${image}.elf> "${CMAKE_NM}")
        if(GP100_AUTOSTART_AUDIO AND GP100_AUTOSTART_TUNER_CAPTURE AND NOT GP100_EFFECT_PROFILE)
            add_test(NAME ${image}_core_link
                COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_core_link.py"
                        "${CMAKE_CURRENT_BINARY_DIR}/${image}.map"
                        ${GP100_CORE_SOURCES})
        endif()
    endforeach()
endif()
