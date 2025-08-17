#!/usr/bin/env python3
"""
Convert any input audio (mp3/wav/flac/ogg/...) into a Butano-friendly WAV:
- mono
- 8-bit unsigned PCM
- 22050 Hz sample rate
Outputs into audio/<name>.wav
Requires: ffmpeg (or macOS afconvert fallback)
"""
import sys, subprocess, shlex
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
AUDIO_DIR = PROJECT / 'audio'
AUDIO_DIR.mkdir(parents=True, exist_ok=True)

SRC = Path(sys.argv[1]) if len(sys.argv) > 1 else None
if not SRC or not SRC.exists():
    print('Usage: tools/convert_audio.py <input_audio> [output_basename]', file=sys.stderr)
    raise SystemExit(2)

base = Path(sys.argv[2]).stem if len(sys.argv) > 2 else SRC.stem
OUT = AUDIO_DIR / f"{base}.wav"

# Prefer ffmpeg; fallback to afconvert (macOS)
ffmpeg = subprocess.run(['bash','-lc','command -v ffmpeg >/dev/null && echo OK || echo NO'], capture_output=True, text=True).stdout.strip()
af = subprocess.run(['bash','-lc','command -v afconvert >/dev/null && echo OK || echo NO'], capture_output=True, text=True).stdout.strip()

if ffmpeg == 'OK':
    cmd = f"ffmpeg -y -i {shlex.quote(str(SRC))} -ac 1 -ar 22050 -f wav -acodec pcm_u8 {shlex.quote(str(OUT))}"
    print(cmd)
    subprocess.check_call(cmd, shell=True)
elif af == 'OK':
    # afconvert can import mp3 and write 8-bit mono 22050 Hz WAV
    cmd = f"afconvert -f WAVE -d UI8@22050 -c 1 {shlex.quote(str(SRC))} -o {shlex.quote(str(OUT))}"
    print(cmd)
    subprocess.check_call(cmd, shell=True)
else:
    print('Neither ffmpeg nor afconvert found. Install ffmpeg, or supply WAV directly.', file=sys.stderr)
    raise SystemExit(1)

print(f"Wrote {OUT}")
