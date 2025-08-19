#!/usr/bin/env python3
import os
import glob
import numpy as np
from scipy.signal import resample_poly
from scipy.fft import rfft, irfft

# -----------------------------------------------------------------------------
# Config
# -----------------------------------------------------------------------------
# Target mix rate should match SND_MIX_RATE_HZ used by the GBA mixer
SR = 18157
# Length of the periodic loop in samples (power-of-two works well for FFT)
LOOP_LEN = 16384
# Number of azimuth buckets around the head (full 360 degrees)
AZ_COUNT = 32
# Optional: path to KEMAR compact dataset (if available). If not found, uses an
# analytic ILD-only magnitude model to shape spectra per ear.
KEMAR_DIR = "/Users/drewatz/Desktop/compact"
# Base noise seed for deterministic generation
SEED = 1337

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "data", "prebaked")
INC_DIR = os.path.join(ROOT, "include")
SRC_DIR = os.path.join(ROOT, "src")

AZ32_H = os.path.join(INC_DIR, "az32_table.h")
AZ32_C = os.path.join(SRC_DIR, "az32_all.c")

os.makedirs(OUT_DIR, exist_ok=True)
os.makedirs(INC_DIR, exist_ok=True)
os.makedirs(SRC_DIR, exist_ok=True)


# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------
def periodic_noise(length, band=(100.0, 9000.0), sr=SR, rng=None):
    """Generate perfectly periodic band-limited noise of given length.
    Uses random phases in frequency domain with Hermitian symmetry, then IFFT."""
    if rng is None:
        rng = np.random.default_rng()
    n = length
    # Frequency bins for rfft (n//2+1 bins)
    freqs = np.linspace(0.0, sr/2.0, n//2 + 1)
    # Amplitude shaping: 1 inside band, 0 outside (soft edges)
    amp = np.zeros_like(freqs, dtype=np.float32)
    lo, hi = band
    ramp = 200.0  # Hz soft ramp
    amp += np.clip((freqs - (lo - ramp)) / ramp, 0.0, 1.0)
    amp *= np.clip(((hi + ramp) - freqs) / ramp, 0.0, 1.0)
    # Random phase for bins > 0 and < Nyquist
    phase = rng.uniform(0.0, 2*np.pi, size=freqs.shape).astype(np.float32)
    X = amp * np.exp(1j*phase)
    X[0] = 0.0  # DC = 0
    X[-1] = X[-1].real  # Nyquist bin real-only if length even
    y = irfft(X, n=n).astype(np.float32)
    # Normalize RMS
    rms = np.sqrt(np.mean(y*y) + 1e-12)
    if rms > 0:
        y = y / rms
    return y


def try_load_kemar_mag(az_deg, ear_left=True):
    """Try to load KEMAR HRIR magnitude for the given azimuth (deg) and ear.
    This expects WAV HRIRs in KEMAR_DIR; if not found, returns None."""
    if not os.path.isdir(KEMAR_DIR):
        return None
    # Try to find a WAV matching the azimuth and ear. This is best-effort: we look
    # for files containing 'az' and 'L'/'R'. Adjust this as needed for your dataset.
    ear_tag = 'L' if ear_left else 'R'
    candidates = glob.glob(os.path.join(KEMAR_DIR, '**', '*.wav'), recursive=True)
    # Prefer files with exact azimuth string like 'az+030' or '_030_'
    az_strs = [f"{int(az_deg):+03d}", f"{int(az_deg):03d}"]
    chosen = None
    for path in candidates:
        name = os.path.basename(path)
        if ear_tag not in name:
            continue
        if any(s in name for s in az_strs) or f"{int(az_deg)}" in name:
            chosen = path
            break
    if chosen is None:
        return None
    try:
        from scipy.io import wavfile
        sr_in, h = wavfile.read(chosen)
        h = h.astype(np.float32)
        if h.ndim > 1:
            h = h[:, 0]
        # Convert to float [-1,1]
        if h.dtype == np.int16:
            h = h.astype(np.float32) / 32768.0
        elif h.dtype == np.int32:
            h = h.astype(np.float32) / 2147483648.0
        elif h.dtype == np.uint8:
            h = (h.astype(np.float32) - 128.0) / 128.0
        # Resample to SR and window to LOOP_LEN/4 taps
        if sr_in != SR:
            h = resample_poly(h, SR, sr_in)
        taps = min(len(h), max(64, LOOP_LEN // 4))
        h = h[:taps]
        # Use magnitude only (zero-phase) to avoid ITD in prebake
        H = np.abs(rfft(h, n=LOOP_LEN)).astype(np.float32)
        # Smooth magnitude a bit
        from scipy.ndimage import gaussian_filter1d
        H = gaussian_filter1d(H, sigma=1.5).astype(np.float32)
        # Normalize
        H /= (np.max(H) + 1e-9)
        return H
    except Exception:
        return None


def analytic_mag(az_deg, ear_left=True):
    """Analytic ILD-only magnitude shaping: near ear has slight HF boost,
    far ear has HF attenuation. Front/back spectral tilt for realism."""
    # Frequency grid for rfft of LOOP_LEN
    freqs = np.linspace(0.0, SR/2.0, LOOP_LEN//2 + 1, dtype=np.float32)
    # Map azimuth to [-180,180)
    a = ((az_deg + 180.0) % 360.0) - 180.0
    # Near/far determination for each ear
    # Left ear is near for angles in (-180,-0), right ear near for (0,180)
    near = (ear_left and a < 0.0) or ((not ear_left) and a >= 0.0)
    # ILD strength up to ~12 dB at 90 degrees, scaled by |sin(az)|
    ild_db = 12.0 * abs(np.sin(np.deg2rad(a)))
    # Apply sign: near ear boosted slightly in highs, far ear attenuated more
    sign = +1.0 if near else -1.0
    # High-shelf magnitude curve
    f_norm = np.clip(freqs / (SR/2.0), 0.0, 1.0)
    shelf_db = sign * ild_db * (f_norm ** 0.5)
    # Back-of-head extra HF loss (~6 dB by 10 kHz equivalent)
    back = abs(a) > 90.0
    if back:
        shelf_db -= 6.0 * (f_norm ** 0.7)
    mag = 10.0 ** (shelf_db / 20.0)
    # Gentle low-frequency floor to avoid DC gain
    mag = np.maximum(mag, 0.25).astype(np.float32)
    return mag


def tpdf_dither(x, rng=None):
    if rng is None:
        rng = np.random.default_rng()
    d = (rng.random(len(x)) - rng.random(len(x))) * (1.0/127.0)
    return x + d.astype(np.float32)


def main():
    rng = np.random.default_rng(SEED)

    # Base periodic noise shared by all azimuths to minimize pops when switching
    base = periodic_noise(LOOP_LEN, band=(120.0, 8000.0), sr=SR, rng=rng)
    X = rfft(base, n=LOOP_LEN)

    # Azimuths: full 360 degrees, centered so 0 deg is to the right, 180 to the left
    azimuths = np.linspace(-180.0, 180.0, AZ_COUNT, endpoint=False)

    left_list = []
    right_list = []

    # Precompute target pair RMS for equal-loudness normalization
    target_pair_rms = 0.5

    for az in azimuths:
        # Try KEMAR magnitude; fall back to analytic
        HL = try_load_kemar_mag(az, ear_left=True)
        HR = try_load_kemar_mag(az, ear_left=False)
        if HL is None or HR is None:
            HL = analytic_mag(az, ear_left=True)
            HR = analytic_mag(az, ear_left=False)
        # Apply magnitude shaping with zero phase
        YL = X * HL
        YR = X * HR
        yL = irfft(YL, n=LOOP_LEN).astype(np.float32)
        yR = irfft(YR, n=LOOP_LEN).astype(np.float32)
        # Equal-loudness normalize by pair RMS
        pair_rms = np.sqrt(0.5*(np.mean(yL*yL) + np.mean(yR*yR)) + 1e-12)
        if pair_rms > 0:
            gain = target_pair_rms / pair_rms
            yL *= gain
            yR *= gain
        # Limit peaks softly to avoid clips after quantization
        for y in (yL, yR):
            np.tanh(y, out=y)  # soft clip
        # Dither and quantize to 8-bit signed
        yLq = np.clip(np.round(tpdf_dither(yL, rng)*127.0), -128, 127).astype(np.int8)
        yRq = np.clip(np.round(tpdf_dither(yR, rng)*127.0), -128, 127).astype(np.int8)
        left_list.append(yLq)
        right_list.append(yRq)

    # Write headers and sources
    with open(AZ32_H, 'w') as fh:
        fh.write("#ifndef AZ32_TABLE_H\n#define AZ32_TABLE_H\n")
        fh.write("#include <stdint.h>\n")
        fh.write(f"#define AZ32_COUNT {AZ_COUNT}\n")
        fh.write(f"#define AZ32_LEN {LOOP_LEN}\n")
        fh.write("extern const int8_t* const az32_L[AZ32_COUNT];\n")
        fh.write("extern const int8_t* const az32_R[AZ32_COUNT];\n")
        fh.write("#endif\n")

    with open(AZ32_C, 'w') as fc:
        fc.write("#include <stdint.h>\n")
        for i in range(AZ_COUNT):
            fc.write(f"static const int8_t az32_L_{i}[] = {{\n    ")
            for k, v in enumerate(left_list[i]):
                fc.write(str(int(v)))
                if k != len(left_list[i]) - 1:
                    fc.write(",")
                if (k+1) % 32 == 0:
                    fc.write("\n    ")
            fc.write("\n};\n\n")
            fc.write(f"static const int8_t az32_R_{i}[] = {{\n    ")
            for k, v in enumerate(right_list[i]):
                fc.write(str(int(v)))
                if k != len(right_list[i]) - 1:
                    fc.write(",")
                if (k+1) % 32 == 0:
                    fc.write("\n    ")
            fc.write("\n};\n\n")
        # Pointer tables
        fc.write(f"const int8_t* const az32_L[{AZ_COUNT}] = {{\n    ")
        for i in range(AZ_COUNT):
            fc.write(f"az32_L_{i}")
            if i != AZ_COUNT - 1:
                fc.write(",")
            if (i+1) % 8 == 0:
                fc.write("\n    ")
        fc.write("\n};\n\n")
        fc.write(f"const int8_t* const az32_R[{AZ_COUNT}] = {{\n    ")
        for i in range(AZ_COUNT):
            fc.write(f"az32_R_{i}")
            if i != AZ_COUNT - 1:
                fc.write(",")
            if (i+1) % 8 == 0:
                fc.write("\n    ")
        fc.write("\n};\n")

    print("Generated:", AZ32_H, AZ32_C)


if __name__ == "__main__":
    main()
