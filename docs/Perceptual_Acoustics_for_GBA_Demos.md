Perceptual Acoustics for GBA Demos — ILD, ITD, and Practical Design

Purpose
This note explains the perceptual cues (ILD, ITD, and spectral shaping) used for spatial audio, why they matter, and how we approximate them efficiently on the Game Boy Advance (GBA). It also outlines practical design patterns for small game demos.

Key Cues for Spatial Hearing
- ILD (Interaural Level Difference):
  - High frequencies are shadowed by the head. The ear nearer to a source tends to receive higher level than the far ear, especially above ~2 kHz.
  - ILD grows with azimuth (side angle) and frequency. It is small near the midline (front/back center) and largest near ±90°.
- ITD (Interaural Time Difference):
  - The arrival time difference between ears (up to ~0.6–0.8 ms for humans at ±90°).
  - Most prominent for low/mid frequencies where wavelength is long relative to head size; powerful for lateralization.
- Spectral (Pinna/Head/Torso) Cues:
  - The outer ear and head create frequency-dependent notches/boosts that vary with azimuth and elevation.
  - These cues help disambiguate front vs back and up vs down. They’re encoded in HRTFs (Head-Related Transfer Functions).
- Distance Cues (simplified):
  - Lower overall level, less high-frequency content, more dry-to-reverb ratio (we usually approximate with volume + HF rolloff).

GBA Constraints and Approach
- Hardware: two 8-bit PCM FIFOs (Direct Sound A/B) mixed at a fixed sample rate (we use 18,157 Hz).
- CPU/RAM: very limited for real-time filtering or long FIRs; VRAM DMA can interfere with audio if overused.
- Strategy:
  - A-mode (engine): live ILD via per-ear gains and pitch changes; no heavy filtering at runtime.
  - B-mode (prebaked HRTF noise): offline magnitude shaping per azimuth to simulate ILD and some spectral coloration. Runtime just streams hard-panned left/right.
  - ITD is intentionally omitted in prebaked assets to minimize loop/transition artifacts. Magnitude-only (zero-phase) shaping avoids combing when switching azimuths.

ILD in This Project
- A-mode: ILD is applied by setting per-channel pan gains (0..64) for left/right ears based on X position.
- B-mode: ILD is “baked in” by filtering the shared base noise with ear-specific magnitude curves. The left sample is played hard-left and the right hard-right, so ILD is encoded in the audio itself.

ITD in This Project
- Not used at runtime; phase/time alignment differences across buckets cause pops or coloration when switching.
- For prebaked data, adding ITD means introducing phase delays per ear. Unless all buckets share aligned, periodic phase, switching angles can produce audible discontinuities.
- If ITD is critical, one can prebake both magnitude and phase with strict periodicity and crossfade between adjacent azimuths. This increases ROM size and complexity.

Spectral Cues (Front/Back)
- Back-of-head typically has extra high-frequency attenuation; magnitude shaping captures some of this.
- We add a runtime hemisphere toggle with hysteresis: when the cursor moves behind the head (Y > center + threshold), the azimuth index rotates 180°, selecting the corresponding “rear” coloration.

Why Prebaked Periodic Noise?
- A single shared periodic base noise (same phases/period across all angles) minimizes pops when switching azimuths because loop boundaries line up.
- Per-ear magnitude shaping (zero-phase) changes the spectral envelope without disturbing the loop phase structure.
- Equal-loudness normalization makes different angles feel consistent in volume.
- Dither and soft limiting smooth quantization artifacts at 8-bit.

Applying This to Simple GBA Demos
- Overhead Stealth/Exploration:
  - Map player/head to screen center, sound sources to world positions.
  - X axis → ILD (pan), Y axis → front/back bucket; distance → volume (and optional HF rolloff via alternate prebakes).
  - Use azimuth hysteresis to prevent rapid flipping near the midline.
- Racing/Pass-by:
  - A-mode: Doppler via playback rate (pitch), ILD via pan, front/rear sample switch at apex.
  - B-mode: Use the prebaked azimuth field for ambient engine/road noise; crossfade near apex if needed.
- Shooter/Arcade:
  - Multiple SFX: prioritize key sounds; avoid more than 2–3 active channels to keep mixer clean.
  - Use coarse azimuth buckets (e.g., 16 or 32) to guide attention without over-spatializing tiny sounds.

Design Tips to Avoid Artifacts
- Quantize the angle to a bucket grid (e.g., 32). Add hysteresis around boundaries to avoid chatter.
- Reset both stereo channels at loop boundaries when switching buckets, or do short crossfades across a few frames.
- Keep per-frame VRAM DMA low to avoid starving audio DMA.
- Normalize prebaked pairs by RMS to prevent sudden loudness shifts.
- For distance, use gentle volume ramps and (optionally) multiple distance layers (near/mid/far prebakes with different spectral rolls).

Extending Toward ITD (Optional)
- Prebake phase: apply fractional-sample delays per ear in frequency domain (complex HRTF). Ensure all azimuths share a common periodic phase grid.
- Transitioning: crossfade between adjacent azimuths over tens of milliseconds to hide phase changes.
- Cost: more ROM and more complex asset generation; runtime remains simple (still two streams), but transitions must be managed.

Takeaways
- ILD and spectral cues provide strong lateral and front/back perception with minimal runtime cost—ideal for GBA.
- ITD can improve realism but demands careful offline phase control to remain pop-free.
- The current pipeline focuses on reliable, even, and low-artifact spatial cues suitable for small, responsive game demos.
