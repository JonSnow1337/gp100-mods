# CMake toolchain file for the MIMXRT1052.
#
#   cmake -S sources -B sources/build \
#         -DCMAKE_TOOLCHAIN_FILE="$PWD/sources/cmake/arm-none-eabi.cmake" \
#         -DMCUX_SDK_ROOT=/path/to/mcux-sdk \
#         -DTOOLCHAIN=/path/to/gcc-arm-none-eabi/bin/arm-none-eabi-
#
# TOOLCHAIN defaults to whatever arm-none-eabi- is on PATH. Passing it explicitly is
# what the verification does, so the build does not depend on the ambient PATH.
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN "arm-none-eabi-" CACHE STRING "arm-none-eabi- prefix, with the trailing dash")

set(CMAKE_ASM_COMPILER ${TOOLCHAIN}gcc)
set(CMAKE_C_COMPILER   ${TOOLCHAIN}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
