#ifndef GP100_ROOM_REVERB_H
#define GP100_ROOM_REVERB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GP100_ROOM_PREDELAY_SAMPLES 9000u
#define GP100_ROOM_FDN_LINE_SAMPLES 4100u
#define GP100_SHIMMER_PITCH_SAMPLES 4096u

typedef struct gp100_room_biquad_state {
    float x1;
    float x2;
    float y1;
    float y2;
} gp100_room_biquad_state_t;

typedef struct gp100_room_reverb {
    float predelay[GP100_ROOM_PREDELAY_SAMPLES];
    float fdn_line[4][GP100_ROOM_FDN_LINE_SAMPLES];
    gp100_room_biquad_state_t diffusion_filter[4];
    gp100_room_biquad_state_t output_filter[2];
    float feedback_state[4];
    float target_mix;
    float current_mix;
    float target_predelay;
    float current_predelay;
    float decay;
    float damping;
    float lo_end;
    float hi_end;
    float pitch_buffer[2][GP100_SHIMMER_PITCH_SAMPLES];
    uint16_t pitch_window[258];
    float pitch_previous_input[2];
    float pitch_previous_output[2];
    uint32_t pitch_phase[2][2];
    size_t pitch_write[2];
    float lfo_phase;
    size_t predelay_write;
    size_t fdn_write[4];
    bool trail;
    bool bypass_cleared;
    bool history_written;
    bool configured;
} gp100_room_reverb_t;

bool gp100_room_reverb_reset(gp100_room_reverb_t *reverb,
                                  uint8_t mix,
                                  uint8_t predelay,
                                  uint8_t decay,
                                  bool trail);

bool gp100_room_reverb_process(gp100_room_reverb_t *reverb,
                                    uint8_t mix,
                                    uint8_t predelay,
                                    uint8_t decay,
                                    bool trail,
                                    float *interleaved_stereo,
                                    size_t frame_count,
                                    bool enabled);

#ifdef __cplusplus
}
#endif

#endif
