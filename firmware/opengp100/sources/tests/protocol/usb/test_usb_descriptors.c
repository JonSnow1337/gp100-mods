#include "protocol/usb/usb_descriptors.h"
#include "protocol/usb/usb_device_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FACTORY_SCATTER_OFFSET 0x24d970L
#define FACTORY_INIT_BYTES 0x244u
#define FACTORY_DEVICE_OFFSET 0x7cu
#define FACTORY_CONFIGURATION_OFFSET 0x90u
#define FACTORY_CLOCK_RATE_RANGE_OFFSET 0x2cu
#define FACTORY_CLOCK_RATE_RANGE_BYTES 14u

static int test_identity(void)
{
    gp100_usb_descriptor_view_t device =
        gp100_usb_device_descriptor();
    const uint8_t expected[] = {
        0x12u, 0x01u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x40u,
        0xefu, 0x84u, 0x21u, 0x00u, 0x00u, 0x01u, 0x01u, 0x02u,
        0x03u, 0x01u
    };

    return device.length == sizeof(expected) &&
                   memcmp(device.bytes, expected, sizeof(expected)) == 0
               ? 0
               : 1;
}

static int test_configuration_shape(void)
{
    gp100_usb_descriptor_view_t view =
        gp100_usb_configuration_descriptor();
    size_t position = 0u;
    unsigned interface_descriptors = 0u;
    unsigned endpoint_descriptors = 0u;
    unsigned audio_capture_formats = 0u;
    unsigned audio_playback_formats = 0u;
    unsigned endpoint_mask = 0u;
    uint8_t current_interface = UINT8_MAX;

    if (view.length != GP100_USB_CONFIGURATION_DESCRIPTOR_BYTES ||
        view.bytes[0] != 9u || view.bytes[1] != 2u ||
        ((size_t)view.bytes[2] | ((size_t)view.bytes[3] << 8u)) !=
            view.length ||
        view.bytes[4] != 4u) {
        return 1;
    }

    while (position < view.length) {
        uint8_t length = view.bytes[position];
        uint8_t type;

        if (length < 2u || length > view.length - position) {
            return 2;
        }
        type = view.bytes[position + 1u];
        if (type == 4u) {
            ++interface_descriptors;
            current_interface = view.bytes[position + 2u];
        } else if (type == 5u && length >= 7u) {
            uint8_t endpoint = view.bytes[position + 2u];

            ++endpoint_descriptors;
            if (endpoint == GP100_USB_AUDIO_CAPTURE_ENDPOINT) {
                endpoint_mask |= 1u;
            } else if (endpoint == GP100_USB_AUDIO_PLAYBACK_ENDPOINT) {
                endpoint_mask |= 2u;
            } else if (endpoint == GP100_USB_MIDI_IN_ENDPOINT) {
                endpoint_mask |= 4u;
            } else if (endpoint == GP100_USB_MIDI_OUT_ENDPOINT) {
                endpoint_mask |= 8u;
            }
        } else if (type == 0x24u && length == 6u &&
                   view.bytes[position + 2u] == 2u) {
            if (view.bytes[position + 4u] == 2u &&
                view.bytes[position + 5u] == 16u) {
                if (current_interface == 1u) {
                    ++audio_capture_formats;
                } else if (current_interface == 2u) {
                    ++audio_playback_formats;
                }
            } else if (view.bytes[position + 4u] == 4u &&
                       view.bytes[position + 5u] == 24u) {
                if (current_interface == 1u) {
                    ++audio_capture_formats;
                } else if (current_interface == 2u) {
                    ++audio_playback_formats;
                }
            }
        }
        position += length;
    }

    return position == view.length && interface_descriptors == 8u &&
                   endpoint_descriptors == 6u && endpoint_mask == 15u &&
                   audio_capture_formats == 2u &&
                   audio_playback_formats == 2u
               ? 0
               : 3;
}

static int test_feature_routing(void)
{
    gp100_usb_descriptor_view_t view =
        gp100_usb_configuration_descriptor();
    uint8_t terminal_sources[256] = {0};
    uint8_t feature_sources[256] = {0};
    uint8_t interface = UINT8_MAX;
    size_t capture_links = 0u;
    size_t playback_links = 0u;
    size_t position = 0u;

    while (position < view.length) {
        const uint8_t *descriptor = view.bytes + position;
        size_t length = descriptor[0];

        if (length < 2u || length > view.length - position) {
            return 1;
        }
        if (length >= 9u && descriptor[1] == 0x04u) {
            interface = descriptor[2];
        } else if (length >= 5u && descriptor[1] == 0x24u) {
            if (interface == GP100_USB_INTERFACE_AUDIO_CONTROL) {
                if (descriptor[2] == 0x06u) {
                    feature_sources[descriptor[3]] = descriptor[4];
                } else if (length >= 8u && descriptor[2] == 0x03u) {
                    terminal_sources[descriptor[3]] = descriptor[7];
                }
            } else if (length == 16u && descriptor[2] == 0x01u) {
                uint8_t terminal = descriptor[3];

                if (interface == GP100_USB_INTERFACE_CAPTURE) {
                    if (terminal_sources[terminal] !=
                        GP100_USB_AUDIO_CAPTURE_FEATURE) {
                        return 2;
                    }
                    ++capture_links;
                } else if (interface == GP100_USB_INTERFACE_PLAYBACK) {
                    if (feature_sources[GP100_USB_AUDIO_PLAYBACK_FEATURE] !=
                        terminal) {
                        return 3;
                    }
                    ++playback_links;
                }
            }
        }
        position += length;
    }
    return capture_links == 2u && playback_links == 2u ? 0 : 4;
}

static int test_strings(void)
{
    static const char *const expected[GP100_USB_STRING_COUNT] = {
        NULL,
        "OpenGP100 Project",
        "OpenGP100",
        "8641647642716732864728641174",
        "OpenGP100 High Speed",
        "OpenGP100 Audio",
        "OpenGP100 MIDI"
    };
    uint8_t descriptor[64];
    size_t length;
    uint8_t index;

    if (!gp100_usb_build_string_descriptor(
            0u, descriptor, sizeof(descriptor), &length) ||
        length != 4u || memcmp(descriptor, "\x04\x03\x09\x04", 4u) != 0) {
        return 1;
    }
    for (index = 1u; index < GP100_USB_STRING_COUNT; ++index) {
        size_t character;
        size_t expected_length = 2u + strlen(expected[index]) * 2u;

        if (!gp100_usb_build_string_descriptor(
                index, descriptor, sizeof(descriptor), &length) ||
            length != expected_length || descriptor[0] != expected_length ||
            descriptor[1] != 3u) {
            return 2;
        }
        for (character = 0u; character < strlen(expected[index]); ++character) {
            if (descriptor[2u + character * 2u] !=
                    (uint8_t)expected[index][character] ||
                descriptor[3u + character * 2u] != 0u) {
                return 3;
            }
        }
    }
    if (gp100_usb_build_string_descriptor(
            GP100_USB_STRING_COUNT,
            descriptor,
            sizeof(descriptor),
            &length) ||
        gp100_usb_build_string_descriptor(
            1u, descriptor, 2u, &length) ||
        length != 2u + 2u * strlen(expected[1]) ||
        gp100_usb_build_string_descriptor(
            1u, descriptor, sizeof(descriptor), NULL)) {
        return 4;
    }
    return 0;
}

static int decode_factory_scatter(FILE *artifact, uint8_t *output)
{
    size_t produced = 0u;

    if (fseek(artifact, FACTORY_SCATTER_OFFSET, SEEK_SET) != 0) {
        return 1;
    }
    while (produced < FACTORY_INIT_BYTES) {
        int control = fgetc(artifact);
        unsigned literal_count;
        unsigned repeat_count;
        unsigned index;

        if (control == EOF) {
            return 2;
        }
        literal_count = (unsigned)control & 7u;
        repeat_count = (unsigned)control >> 4u;
        if (literal_count == 0u) {
            int value = fgetc(artifact);
            if (value == EOF || value == 0) {
                return 3;
            }
            literal_count = (unsigned)value;
        }
        if (repeat_count == 0u) {
            int value = fgetc(artifact);
            if (value == EOF) {
                return 4;
            }
            repeat_count = (unsigned)value;
        }
        for (index = 1u; index < literal_count; ++index) {
            int value = fgetc(artifact);

            if (value == EOF || produced >= FACTORY_INIT_BYTES) {
                return 5;
            }
            output[produced++] = (uint8_t)value;
        }
        if (((unsigned)control & 8u) == 0u) {
            if (repeat_count > FACTORY_INIT_BYTES - produced) {
                return 6;
            }
            memset(output + produced, 0, repeat_count);
            produced += repeat_count;
        } else {
            int distance = fgetc(artifact);

            repeat_count += 2u;
            if (distance <= 0 || (size_t)distance > produced ||
                repeat_count > FACTORY_INIT_BYTES - produced) {
                return 7;
            }
            for (index = 0u; index < repeat_count; ++index) {
                output[produced] = output[produced - (size_t)distance];
                ++produced;
            }
        }
    }
    return 0;
}

static int test_against_artifact(const char *artifact_path)
{
    uint8_t initialized[FACTORY_INIT_BYTES];
    gp100_usb_descriptor_view_t device =
        gp100_usb_device_descriptor();
    gp100_usb_descriptor_view_t configuration =
        gp100_usb_configuration_descriptor();
    gp100_usb_descriptor_view_t clock_rate_range =
        gp100_usb_clock_rate_range();
    FILE *artifact = fopen(artifact_path, "rb");
    int result;

    if (artifact == NULL) {
        return 1;
    }
    result = decode_factory_scatter(artifact, initialized);
    fclose(artifact);
    if (result != 0) {
        return 10 + result;
    }
    if (memcmp(
            initialized + FACTORY_DEVICE_OFFSET,
            device.bytes,
            device.length) != 0 ||
        memcmp(
            initialized + FACTORY_CONFIGURATION_OFFSET,
            configuration.bytes,
            configuration.length) != 0) {
        return 20;
    }
    if (clock_rate_range.length != FACTORY_CLOCK_RATE_RANGE_BYTES ||
        memcmp(initialized + FACTORY_CLOCK_RATE_RANGE_OFFSET,
               clock_rate_range.bytes, clock_rate_range.length) != 0) {
        return 21;
    }
    return 0;
}

int main(int argc, char **argv)
{
    int result;

    if (argc != 2) {
        fprintf(stderr, "usage: %s APP_SDRAM_BIN\n", argv[0]);
        return 1;
    }
    result = test_identity();
    if (result != 0) {
        fprintf(stderr, "identity test failed: %d\n", result);
        return 10 + result;
    }
    result = test_configuration_shape();
    if (result != 0) {
        fprintf(stderr, "configuration test failed: %d\n", result);
        return 20 + result;
    }
    result = test_feature_routing();
    if (result != 0) {
        fprintf(stderr, "feature routing test failed: %d\n", result);
        return 50 + result;
    }
    result = test_strings();
    if (result != 0) {
        fprintf(stderr, "string test failed: %d\n", result);
        return 30 + result;
    }
    result = test_against_artifact(argv[1]);
    if (result != 0) {
        fprintf(stderr, "artifact test failed: %d\n", result);
        return 40 + result;
    }
    return 0;
}
