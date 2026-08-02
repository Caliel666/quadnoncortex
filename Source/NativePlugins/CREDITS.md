# Native plugins — licenses & credits

## Pitch Shifter (`PitchShifter/`)
- **Engine adapted from** [VoLum](https://github.com/guitarlum/VoLum) `VoLumPitchShifter.h`
- **License:** MIT — see `PitchShifter/LICENSE-VoLum.txt`
- Copyright (c) 2024-2026 Steffen Dangmann and VoLum contributors; portions Steven Atkinson

## Native NAM (`Source/NativeNam/`)
- **DSP:** [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) — MIT (Steven Atkinson)
- **Eigen** (via NAM): Mozilla Public License 2.0
- **Tone3000 API** client: use per https://www.tone3000.com / their API terms (not a code license; service ToS)
- Keep NAM’s LICENSE if you vendor or FetchContent the core

## Echo, Reverb, Compressor, Limiter, Parametric EQ, Denoiser
- Original implementations for quadnoncortex (Airwindows-inspired *style* only — no Airwindows source was copied)
