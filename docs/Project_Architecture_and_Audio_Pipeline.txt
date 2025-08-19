Project Architecture and Audio Pipeline (Tonc + Sappy-style Mixer)

Overview
This project is a pure-Tonc GBA program with a custom, Sappy-inspired software mixer. It provides two interactive audio modes:
- A-mode (live engine): plays a looping engine sample with position-controlled panning and pitch.
- B-mode (prebaked HRTF): plays prebaked stereo noise textures simulating spatial azimuth around a head.

Key Components
- Makefile: devkitARM + libtonc build; links against libtonc only. Produces Fuck.gba.
- include/Sound.h, src/Sound.c: Deku-style software mixer with linear interpolation, double buffering, and stereo output to FIFOs via DMA.
- include/EngineSound.h, src/EngineSound.c: A-mode engine logic (looping sample, pan, volume, pitch).
- include/Prebaked.h, src/Prebaked.c: B-mode logic (selects prebaked stereo pair based on position).
- tools/gen_prebaked.py: Offline generator producing az32 tables using periodic noise and magnitude-only HRTF shaping.
- include/az32_table.h, src/az32_all.c: Generated prebaked stereo assets.
- include/car_front.h, src/car_front.c, include/car_front_smooth.h, src/car_front_smooth.c: Engine samples.
- src/main.c: Program entry, input handling, visuals, and mode switching.

Display / Input
- Mode 3 (bitmap) is used for a simple UI: a white center “head” dot (5x5) and a green position dot (3x3).
- D-Pad moves the green dot; SELECT resets to center; A toggles A-mode; B toggles B-mode.

Audio Hardware Setup (SndInit)
- Enables sound (REG_SOUNDCNT_X) and sets bias.
- Configures Direct Sound A/B at 100% volume, routed left/right.
- Timer 0 runs at the mixer sample rate (SND_MIX_RATE_HZ = 18157 Hz).
- DMA1 streams left buffer to FIFO A; DMA2 streams right buffer to FIFO B, re-armed each VBlank.

Software Mixer (SndMix)
- Buffering: 303-sample stereo double buffers in EWRAM.
- Channels: up to 4 software channels (SND_MAX_CHANNELS=4), each with 12.12 fixed-point position/increment, volume (0..64), and per-ear pan (0..64).
- Interpolation: linear interpolation between samples to reduce metallic aliasing.
- Looping: channels may loop a specified segment; mixer wraps positions accordingly.
- Accumulation: 32-bit intermediate accumulators per ear. Final downscale uses a heuristic to avoid over-attenuating hard-panned stereo (L-only and R-only treated like 1ch per ear).

A-mode (EngineSound)
- Data: front and rear versions of the same engine sample (rear played at 2/3 frequency).
- Controls: position X → pan (ILD); position Y → front/back sample and pitch; distance from center → volume.
- API: EngineSound_init, EngineSound_start, EngineSound_update, EngineSound_update_volume, EngineSound_stop, EngineSound_set_pan.
- Tuning: FRONT_SAMPLE_HZ defaults to 26 kHz (rear 2/3 of that), mixed down at 18,157 Hz via increment math.

B-mode (Prebaked)
- Assets: 32 azimuth buckets of stereo 8-bit loops, each LOOP_LEN=16384 samples.
- Runtime: channel 1 hard-left with left ear sample; channel 2 hard-right with right ear sample.
- Position mapping: X → azimuth index; Y → back hemisphere flag with ±8px hysteresis; if back, index rotates by 180°.
- Volume: shared volume (0..64) applied equally; ILD baked into the samples. Switching between buckets resets positions together to maintain phase alignment.

Prebake Generation (tools/gen_prebaked.py)
- Base: perfectly periodic band-limited noise shared by all azimuths.
- Magnitude: KEMAR-derived magnitude (if available) or analytic ILD-only frequency response per ear; zero phase (no ITD) to keep loops consistent.
- Normalization: per-pair RMS normalization for even loudness.
- Quantization: tanh soft limiting, TPDF dithering, 8-bit signed PCM output.
- Outputs: include/az32_table.h and src/az32_all.c.

Performance Considerations
- All mixing runs per-frame in IWRAM/EWRAM; channel count limited to 4.
- Avoids heavy VRAM DMA during audio to prevent starving FIFO DMA.
- No runtime filtering/EQ in B-mode; all spectral shaping is prebaked.

Typical Flow
1) Boot: initialize sound, draw background/center dot, set up input and callbacks.
2) A-mode: on A, start/stop engine with position-derived pan/volume/pitch.
3) B-mode: on B, enable prebaked channels and call Prebaked_update_by_position every frame with pan/volume derived from position.

Regeneration / Build
- Regenerate prebaked assets: python3 tools/gen_prebaked.py
- Build and run: make -j4

Known Limitations / Future Work
- Magnitude-only HRTF (no ITD) for stability; full KEMAR phase/ITD could be prebaked with more careful loop alignment.
- Azimuth resolution is 32 buckets; can be raised at cost of ROM size.
- Additional equal-loudness contouring (e.g., ISO 226) could improve perceptual uniformity.
