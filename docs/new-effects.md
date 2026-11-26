# New effects in OpenGP100

Six effects were added since the previous release (the effect catalog went from 185 to 191 models). They are in the effect list on the pedal
under the group shown in the table. Descriptions follow the OpenGP100 author's documentation.

| Effect | Group | What it is |
| --- | --- | --- |
| [Studio Comp](#studio-comp) | Dynamics | Parallel compressor with separate Wet and Dry levels |
| [Limitter](#limitter) | Dynamics | A limiter you can place anywhere in the chain |
| [Muff](#muff) | Drive | 1974 "White Can" Big Muff fuzz |
| [Octaver2](#octaver2) | Pitch | Analog-style single-note octaver |
| [Balls](#balls) | Wah | Clean twin envelope filter |
| [Envelope](#envelope) | Wah | M82-inspired envelope filter |

Also new, but not an effect in the chain: **Global > Output Limiter**, a brickwall limiter on the final stereo output
(PARA switches it on and off; Q1 Threshold, Q2 Ceiling, Q3 Release; off by default).

## Studio Comp

Parallel compression. The compressor uses the louder channel's current peak to drive a shared gain reduction. Dry and Wet have
independent levels, and the effect adds zero samples of audio delay. Press PARA to cycle the parameter pages.

| Control | Range | Default |
| --- | --- | --- |
| Wet | -inf to +12 dB | 0 dB |
| Dry | -inf to +12 dB | -inf (muted) |
| Threshold | -60 to +12 dBFS | 0 dBFS |
| Attack | 0 to 500 ms | 3 ms |
| Release | 0 to 5,000 ms | 100 ms |
| Ratio | 1:1 to 40:1, plus inf:1 | 4:1 |
| Knee | 0 to 24 dB | 0 dB |

## Limitter

Limits the signal at the point where you put it in the chain. Threshold, Ceiling and Release are saved in the preset. Each active
Limitter adds about 2 ms of lookahead.

## Muff

The 1974 White Can (74 #3) Big Muff. **Sustain** drives two silicon-diode feedback stages, **Tone** blends the bass and treble
branches, **Volume** sets the output level. Circuit traced by Kit Rae.

## Octaver2

Analog-style octave division of a single note. **Direct**, **Oct 1** and **Oct 2** mix the input with voices one and two octaves below
it. Press PARA for **Range**: start with Low for bass. Range changes the detector and the tone of the first octave.

## Balls

A clean twin envelope filter (after the Electro-Harmonix Bassballs). **Response** sets how far the two resonances sweep with
your playing dynamics.

## Envelope

An envelope filter inspired by the M82.
**Dry** and **FX** set the direct and filtered levels, **Decay** sets where the sweep settles, **Q** shapes the resonance and **Sens**
adjusts how easily playing opens the filter. The model sweeps from 76 Hz to 3.2 kHz, with a decay stop frequency of 76 Hz to 1.3 kHz.
Its envelope timing and sensitivity are estimates, not measured values.
