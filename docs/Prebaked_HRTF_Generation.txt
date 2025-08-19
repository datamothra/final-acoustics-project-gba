Prebaked HRTF Noise Generation (az32) — Design and Rationale

Goals
- Generate a family of seamless, loop-perfect stereo noise textures that simulate how broadband noise sounds at different azimuths around a head, without doing any live EQ on the GBA.
- Keep loudness even across angles, minimize pops when switching “angles,” and keep CPU cost minimal at runtime.

What We Generate
- 32 azimuth buckets (full 360°, azimuth step = 11.25°), each as a linked stereo pair: left and right 8-bit signed PCM loops of identical length.
- Loop length: 16,384 samples at 18,157 Hz (~0.90s per loop). This matches the mixer rate to avoid resampling.
- Output C files: include/az32_table.h and src/az32_all.c with const int8_t* const az32_L[32] and az32_R[32] tables for fast lookup.

Core Idea: Periodic Base Noise + Frequency-Domain Magnitude Shaping
- We generate one shared, perfectly periodic base noise signal that loops seamlessly.
- For each azimuth, we apply an ear-specific magnitude response (HRTF magnitude only, zero phase) in the frequency domain and inverse-FFT back to time.
- Because every azimuth starts from the same periodic base and only changes spectral magnitudes, switching angles during playback tends to avoid hard phase discontinuities, minimizing pops/zippering.

Pipeline Details (tools/gen_prebaked.py)
1) Shared periodic noise
   - Create random phases on the RFFT grid, apply a soft band-pass (default ~120–8000 Hz), set DC=0, honor Nyquist realness, then IRFFT to time domain.
   - Normalize to fixed RMS. Length is power-of-two (16,384) to make FFTs efficient.

2) HRTF magnitude per ear
   - Try to load KEMAR HRIR WAVs (if available) from "/Users/drewatz/Desktop/compact" and compute a smoothed magnitude spectrum at the target length/sample rate.
   - If not found, use an analytic ILD-only model: near ear has a gentle high-shelf boost, far ear gets high-frequency attenuation, with additional back-of-head HF loss. This avoids ITD (time delay) to keep runtime simple and avoid combing.

3) Apply magnitude, zero phase
   - Multiply the base noise spectrum by the ear’s magnitude (left/right separately), using zero phase (pure magnitude shaping). IRFFT to produce time-domain ear signals.

4) Equal-loudness normalization (pair-wise)
   - Compute RMS for the L/R pair and apply a single gain so pairs feel level-matched across azimuths. This reduces perceived loudness jumps when panning.

5) Soft limiting + TPDF dithering + 8-bit
   - Apply tanh soft clip before quantization to control peaks gracefully.
   - Add simple TPDF dither (difference of two uniforms) at ~1 LSB to decorrelate quantization noise.
   - Quantize to signed 8-bit PCM for direct use by the GBA mixer.

6) Emit tables for fast runtime selection
   - For each azimuth index i, write az32_L_i[] and az32_R_i[] arrays.
   - Emit pointer tables az32_L[i] and az32_R[i] plus constants AZ32_COUNT and AZ32_LEN.

Runtime Integration (B‑mode)
- Two mixer channels are reserved: PB_LEFT_CH=1 (hard-left), PB_RIGHT_CH=2 (hard-right).
- X position maps to azimuth index 0..31. Y position toggles front/back hemisphere with ±8px hysteresis by rotating the azimuth 180°.
- Volume (0..64) applies equally to both ears; ILD is baked into the samples.
- Because every bucket uses the same periodic base, switching azimuths resets both channels at loop boundaries (by resetting pos together) to reduce artifacts.

Why This Works Better
- Seamless, identical loop points for all buckets avoid phase jumps on azimuth change.
- Level normalization reduces sudden loudness cliffs.
- Dithered 8-bit output reduces “zippery” quantization texture.
- Azimuth density (32 buckets) provides smoother spatial coverage.

Tuning Notes
- Adjust AZ_COUNT, LOOP_LEN, and band-pass for different trade-offs (ROM size vs quality).
- If full KEMAR coverage becomes available, swap the analytic model with magnitude curves derived from the dataset for each azimuth.
- ITD can be added in prebake by applying phase delays, but this complicates seamless switching; magnitude-only is a pragmatic compromise for GBA.

File Outputs
- include/az32_table.h: declares AZ32_COUNT, AZ32_LEN, and pointer tables.
- src/az32_all.c: contains all stereo arrays and pointer tables (ROM-resident).

Regeneration
- Edit parameters in tools/gen_prebaked.py if needed.
- Run: python3 tools/gen_prebaked.py
- Rebuild: make -j4
