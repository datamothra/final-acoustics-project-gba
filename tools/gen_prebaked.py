#!/usr/bin/env python3
import os
import numpy as np
from scipy.signal import fftconvolve, resample_poly
from scipy.io import wavfile

# Config
SRC_WAV = "/Users/drewatz/Desktop/CarSound.wav"
SR = 22050
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "data", "prebaked")
INC_DIR = os.path.join(ROOT, "include")
SRC_DIR = os.path.join(ROOT, "src")
HEADER_PATH = os.path.join(INC_DIR, "prebaked_assets.h")
C_FIXED_PATH = os.path.join(SRC_DIR, "pb_fixed.c")
TAB_H_PATH = os.path.join(INC_DIR, "pb_table.h")
TAB_C_PATH = os.path.join(SRC_DIR, "pb_all.c")

# Simple loop smooth (crossfade N samples)
def loop_smooth(x, n=256):
    if len(x) < 2*n:
        return x
    head = x[:n]
    tail = x[-n:]
    w = np.linspace(0,1,n)
    xf = head*w + tail[::-1]*(1-w)
    y = x.copy()
    y[:n] = xf
    y[-n:] = xf[::-1]
    return y

# Placeholder HRIR kernel (replace with real KEMAR/CIPIC if available)
def simple_hrtf_kernel(sr, az_deg, is_back, ear_left=True):
    taps = 63
    t = np.arange(taps)
    fc = 4000.0 if not is_back else 2500.0
    alpha = np.exp(-2*np.pi*fc/sr)
    h = alpha**t
    if ear_left and az_deg < 0:
        shelf = 1.0 - 0.15*np.linspace(0,1,taps)
    elif (not ear_left) and az_deg > 0:
        shelf = 1.0 - 0.15*np.linspace(0,1,taps)
    else:
        shelf = 1.0 - 0.45*np.linspace(0,1,taps)
    return (h * shelf).astype(np.float32)

AZ = [-60,-45,-30,-15, 15,30,45,60]
FB = ["front","back"]

os.makedirs(OUT_DIR, exist_ok=True)
os.makedirs(INC_DIR, exist_ok=True)
os.makedirs(SRC_DIR, exist_ok=True)

# Load source mono with scipy.io.wavfile
sr_in, x = wavfile.read(SRC_WAV)
# Convert to float [-1,1]
if x.dtype == np.int16:
    x = x.astype(np.float32) / 32768.0
elif x.dtype == np.int32:
    x = x.astype(np.float32) / 2147483648.0
elif x.dtype == np.uint8:
    x = (x.astype(np.float32) - 128.0) / 128.0
else:
    x = x.astype(np.float32)
# Downmix if stereo
if x.ndim > 1:
    x = np.mean(x, axis=1)
# Resample if needed
if sr_in != SR:
    x = resample_poly(x, SR, sr_in)
# Normalize
x = x / (np.max(np.abs(x))+1e-9)
# Loop smooth base
x = loop_smooth(x, n=256)

entries = []

for az in AZ:
    for fb in FB:
        is_back = (fb=="back")
        hL = simple_hrtf_kernel(SR, az, is_back, ear_left=True)
        hR = simple_hrtf_kernel(SR, az, is_back, ear_left=False)
        yL = fftconvolve(x, hL, mode='same')
        yR = fftconvolve(x, hR, mode='same')
        yL = loop_smooth(yL, n=256)
        yR = loop_smooth(yR, n=256)
        m = max(np.max(np.abs(yL)), np.max(np.abs(yR)), 1e-9)
        yL = yL/m; yR=yR/m
        qL = np.clip(np.round(yL*127.0), -128, 127).astype(np.int8)
        qR = np.clip(np.round(yR*127.0), -128, 127).astype(np.int8)
        base = f"az{abs(az):02d}{'L' if az<0 else 'R'}_{fb}"
        entries.append((base, qL, qR))
        (np.asarray(qL,dtype=np.int8)).tofile(os.path.join(OUT_DIR, base+"_L.raw"))
        (np.asarray(qR,dtype=np.int8)).tofile(os.path.join(OUT_DIR, base+"_R.raw"))

with open(HEADER_PATH, 'w') as f:
    f.write("#ifndef PREBAKED_ASSETS_H\n#define PREBAKED_ASSETS_H\n\n")
    if entries:
        f.write("#define PREBAKED_HAVE_ASSETS 1\n\n")
        base, qL, qR = entries[0]
        symL = f"pb_{base}_L"
        symR = f"pb_{base}_R"
        f.write(f"extern const signed char {symL}[];\n")
        f.write(f"extern const signed char {symR}[];\n")
        f.write(f"#define PB_FIXED_LEN {len(qL)}\n")
        f.write(f"#define PB_FIXED_LEFT {symL}\n")
        f.write(f"#define PB_FIXED_RIGHT {symR}\n")
    else:
        f.write("#define PREBAKED_HAVE_ASSETS 0\n")
    f.write("\n#endif\n")

if entries:
    with open(TAB_H_PATH, 'w') as th:
        th.write("#ifndef PB_TABLE_H\n#define PB_TABLE_H\n\n")
        th.write("#include <stdint.h>\n\n")
        th.write(f"#define PB_AZ_COUNT {len(AZ)}\n")
        th.write(f"#define PB_FB_COUNT {len(FB)}\n")
        th.write(f"#define PB_LEN {len(entries[0][1])}\n\n")
        th.write("extern const int8_t* const pb_table_L[PB_AZ_COUNT*PB_FB_COUNT];\n")
        th.write("extern const int8_t* const pb_table_R[PB_AZ_COUNT*PB_FB_COUNT];\n")
        th.write("#endif\n")
    with open(TAB_C_PATH, 'w') as tc:
        tc.write("#include <stdint.h>\n\n")
        for base,qL,qR in entries:
            symL = f"pb_{base}_L"
            symR = f"pb_{base}_R"
            tc.write(f"static const int8_t {symL}[] = {{\n    ")
            for i,v in enumerate(qL):
                tc.write(str(int(v)))
                if i != len(qL)-1: tc.write(",")
                if (i+1)%32==0: tc.write("\n    ")
            tc.write("\n};\n\n")
            tc.write(f"static const int8_t {symR}[] = {{\n    ")
            for i,v in enumerate(qR):
                tc.write(str(int(v)))
                if i != len(qR)-1: tc.write(",")
                if (i+1)%32==0: tc.write("\n    ")
            tc.write("\n};\n\n")
        tc.write("const int8_t* const pb_table_L["+str(len(entries))+"] = {\n    ")
        for idx,(base,_,_) in enumerate(entries):
            tc.write(f"pb_{base}_L")
            if idx != len(entries)-1: tc.write(",")
            if (idx+1)%8==0: tc.write("\n    ")
        tc.write("\n};\n\n")
        tc.write("const int8_t* const pb_table_R["+str(len(entries))+"] = {\n    ")
        for idx,(base,_,_) in enumerate(entries):
            tc.write(f"pb_{base}_R")
            if idx != len(entries)-1: tc.write(",")
            if (idx+1)%8==0: tc.write("\n    ")
        tc.write("\n};\n")

print("Generated:", HEADER_PATH, C_FIXED_PATH, TAB_H_PATH, TAB_C_PATH)
