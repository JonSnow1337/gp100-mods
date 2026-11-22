#ifndef GP100_POWER_ENVELOPE_INTERNAL_H
#define GP100_POWER_ENVELOPE_INTERNAL_H

/* Exact second-order envelope coefficients stored by the amplifier
 * initializers for their 441- and 2205-sample rise settings. */
#define GP100_ENVELOPE_10_MS_B0 0x1.9e679ep-16f
#define GP100_ENVELOPE_10_MS_A1 (-0x1.fd7494p+0f)
#define GP100_ENVELOPE_10_MS_A2 0x1.faec64p-1f

#define GP100_ENVELOPE_50_MS_B0 0x1.0a49f2p-20f
#define GP100_ENVELOPE_50_MS_A1 (-0x1.ff7d74p+0f)
#define GP100_ENVELOPE_50_MS_A2 0x1.fefb0ap-1f

#endif
