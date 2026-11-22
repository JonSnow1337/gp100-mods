set(GP100_ASSET_DIR "${GP100_GENERATED_DIR}/factory_banks")
set(GP100_ASSET_PACKER "${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_factory_banks.py")
set(GP100_IMAGE_COMPOSER "${CMAKE_CURRENT_SOURCE_DIR}/tools/compose_factory_assets.py")
set(GP100_FACTORY_BANKS
    "${GP100_ASSET_DIR}/rhythm_samples.bin"
    "${GP100_ASSET_DIR}/rhythm_patterns.bin"
    "${GP100_ASSET_DIR}/factory_cabinets.bin")

file(GLOB GP100_FACTORY_INPUTS CONFIGURE_DEPENDS
    "${GP100_ASSET_SOURCE_DIR}/rhythm/*.wav"
    "${GP100_ASSET_SOURCE_DIR}/cabinets/*.wav")

function(gp100_compile_asset name source)
    add_library(gp100_${name} OBJECT "${GP100_ASSET_SOURCE_DIR}/${source}")
    target_include_directories(gp100_${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(gp100_${name} PRIVATE c_std_11)
    target_compile_options(gp100_${name} PRIVATE ${CPU_FLAGS} -Wall -Wextra -Werror)

    set(data "${GP100_ASSET_DIR}/${name}.bin")
    add_custom_command(
        OUTPUT "${data}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${GP100_ASSET_DIR}"
        COMMAND "${CMAKE_OBJCOPY}" -O binary --only-section=.gp100_asset
                $<TARGET_OBJECTS:gp100_${name}> "${data}"
        DEPENDS gp100_${name} $<TARGET_OBJECTS:gp100_${name}>
        COMMENT "Extract ${source} data"
        VERBATIM)
    set(${name}_data "${data}" PARENT_SCOPE)
endfunction()

gp100_compile_asset(drum_pattern_words rhythm/patterns.c)
gp100_compile_asset(cabinet_names cabinets/cabinet_names.c)
set(GP100_COMPILED_ASSETS "${drum_pattern_words_data}" "${cabinet_names_data}")

add_custom_command(
    OUTPUT ${GP100_FACTORY_BANKS}
    COMMAND python3 -B "${GP100_ASSET_PACKER}"
            "${GP100_ASSET_SOURCE_DIR}" ${GP100_COMPILED_ASSETS} "${GP100_ASSET_DIR}"
    DEPENDS ${GP100_COMPILED_ASSETS}
            ${GP100_FACTORY_INPUTS} "${GP100_ASSET_PACKER}" "${GP100_IMAGE_COMPOSER}"
    COMMENT "Pack factory asset tables and WAVs for flash"
    VERBATIM)
add_custom_target(gp100_factory_banks DEPENDS ${GP100_FACTORY_BANKS})
add_dependencies(gp100_base.elf gp100_factory_banks)

add_custom_command(TARGET gp100_base.elf POST_BUILD
    COMMAND python3 "${GP100_IMAGE_COMPOSER}"
            "${CMAKE_CURRENT_BINARY_DIR}/gp100_base.bin" "${GP100_ASSET_DIR}"
    COMMENT "Place factory asset banks at their flash offsets"
    VERBATIM)
set_property(TARGET gp100_base.elf APPEND PROPERTY LINK_DEPENDS
    "${GP100_IMAGE_COMPOSER}" "${CMAKE_CURRENT_SOURCE_DIR}/storage/flash_layout.h"
    ${GP100_FACTORY_BANKS})
set(GP100_IMAGE_CHECK_ASSETS --assets "${GP100_ASSET_DIR}")
