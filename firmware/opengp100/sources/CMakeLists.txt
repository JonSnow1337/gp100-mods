cmake_minimum_required(VERSION 3.16)
project(gp100 LANGUAGES C)

include(CTest)

set(GP100_ASSET_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(GP100_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
include(cmake/effect_profile.cmake)
include(cmake/core.cmake)

if(CMAKE_CROSSCOMPILING)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    endif()
    enable_language(ASM)
    include(cmake/firmware.cmake)
elseif(BUILD_TESTING)
    add_subdirectory(tests)
endif()
