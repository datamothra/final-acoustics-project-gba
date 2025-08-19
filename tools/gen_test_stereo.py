#!/usr/bin/env python3
"""Generate simple test stereo samples - white noise with different patterns for L/R"""
import numpy as np
import os

# Generate test samples
SAMPLE_LEN = 2048
np.random.seed(42)

# Left channel: white noise
left = np.random.randint(-64, 64, SAMPLE_LEN, dtype=np.int8)

# Right channel: different white noise (uncorrelated)
np.random.seed(43)
right = np.random.randint(-64, 64, SAMPLE_LEN, dtype=np.int8)

# Write as C arrays
os.makedirs("src", exist_ok=True)

with open("src/test_stereo.c", "w") as f:
    f.write("// Test stereo white noise samples\n\n")
    
    # Left channel
    f.write("const signed char test_noise_left[2048] = {\n")
    for i in range(0, SAMPLE_LEN, 16):
        f.write("    ")
        for j in range(16):
            if i+j < SAMPLE_LEN:
                f.write(f"{left[i+j]:4d},")
        f.write("\n")
    f.write("};\n\n")
    
    # Right channel  
    f.write("const signed char test_noise_right[2048] = {\n")
    for i in range(0, SAMPLE_LEN, 16):
        f.write("    ")
        for j in range(16):
            if i+j < SAMPLE_LEN:
                f.write(f"{right[i+j]:4d},")
        f.write("\n")
    f.write("};\n")

print("Generated test_stereo.c with uncorrelated white noise for L/R channels")
