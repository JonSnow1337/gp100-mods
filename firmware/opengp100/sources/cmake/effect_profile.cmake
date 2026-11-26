set(GP100_EFFECT_PROFILE "" CACHE FILEPATH "CMake file listing the pedals to include")
set(GP100_EFFECT_CATALOG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/audio/effects/effect_catalog.c")
set(GP100_PROFILE_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_effect_profile.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${GP100_EFFECT_CATALOG_SOURCE}" "${GP100_PROFILE_GENERATOR}")

set(GP100_PROFILE_ARGS)
if(GP100_EFFECT_PROFILE)
    unset(GP100_EFFECTS)
    include("${GP100_EFFECT_PROFILE}")
    if(NOT DEFINED GP100_EFFECTS)
        message(FATAL_ERROR "The effect profile must define GP100_EFFECTS")
    endif()
    set(GP100_PROFILE_ARGS --reduced --effects ${GP100_EFFECTS})
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${GP100_PROFILE_GENERATOR}"
            "${GP100_EFFECT_CATALOG_SOURCE}" "${GP100_GENERATED_DIR}" ${GP100_PROFILE_ARGS}
    RESULT_VARIABLE GP100_PROFILE_RESULT)
if(NOT GP100_PROFILE_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not generate the effect profile")
endif()
