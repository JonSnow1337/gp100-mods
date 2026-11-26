SDK_ROOT ?= $(MCUX_SDK_ROOT)
USB_ROOT ?= $(MCUX_USB_ROOT)
TOOLCHAIN ?= arm-none-eabi-
BUILD_DIR ?= build
HOST_BUILD_DIR ?= build-host
BUILD_TYPE ?= RelWithDebInfo
AUDIO_START ?= ON
TUNER_START ?= ON
STOCK_UPDATE_TEMPLATE ?=
EFFECT_PROFILE ?=

# CMake builds in parallel. Serialize wrapper goals that share BUILD_DIR.
.NOTPARALLEL:
.PHONY: all gp100_base gp100_stock_slot configure check host-check clean

all: configure
	cmake --build "$(BUILD_DIR)" --parallel

gp100_base gp100_stock_slot: configure
	cmake --build "$(BUILD_DIR)" --target "$@" --parallel

configure:
	cmake -S . -B "$(BUILD_DIR)" \
		-DCMAKE_TOOLCHAIN_FILE="$(CURDIR)/cmake/arm-none-eabi.cmake" \
		-DTOOLCHAIN="$(if $(findstring /,$(TOOLCHAIN)),$(abspath $(TOOLCHAIN)),$(TOOLCHAIN))" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
		-DMCUX_SDK_ROOT="$(abspath $(SDK_ROOT))" \
		-DMCUX_USB_ROOT="$(abspath $(USB_ROOT))" \
		-DGP100_AUTOSTART_AUDIO="$(AUDIO_START)" \
		-DGP100_AUTOSTART_TUNER_CAPTURE="$(TUNER_START)" \
		-DGP100_STOCK_UPDATE_TEMPLATE="$(abspath $(STOCK_UPDATE_TEMPLATE))" \
		-DGP100_EFFECT_PROFILE="$(abspath $(EFFECT_PROFILE))"

check: all
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure

host-check:
	cmake -S . -B "$(HOST_BUILD_DIR)" -DBUILD_TESTING=ON \
		-DGP100_EFFECT_PROFILE="$(abspath $(EFFECT_PROFILE))" \
		$(if $(REFERENCE_FLASH),-DGP100_FACTORY_FLASH_DUMP="$(abspath $(REFERENCE_FLASH))")
	cmake --build "$(HOST_BUILD_DIR)" --parallel
	ctest --test-dir "$(HOST_BUILD_DIR)" --output-on-failure

clean:
	cmake -E remove_directory "$(BUILD_DIR)"
