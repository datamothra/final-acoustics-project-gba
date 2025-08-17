#!/usr/bin/env python3
import sys
from pathlib import Path
from PIL import Image

def fit_letterbox(img: Image.Image, width: int, height: int) -> Image.Image:
	img = img.convert("RGB")
	src_w, src_h = img.size
	if src_w == 0 or src_h == 0:
		raise ValueError("Invalid source image size")
	scale = min(width / src_w, height / src_h)
	new_size = (max(1, int(src_w * scale)), max(1, int(src_h * scale)))
	resized = img.resize(new_size, Image.LANCZOS)
	canvas = Image.new("RGB", (width, height), (0, 0, 0))
	offset = ((width - new_size[0]) // 2, (height - new_size[1]) // 2)
	canvas.paste(resized, offset)
	return canvas

def to_8bit_bmp(img: Image.Image, out_bmp: Path) -> None:
	pal = img.convert("P", palette=Image.Palette.ADAPTIVE, colors=256)
	out_bmp.parent.mkdir(parents=True, exist_ok=True)
	pal.save(out_bmp, format="BMP")

def main():
	if len(sys.argv) < 3:
		print("Usage: prepare_image.py <input_image> <output_bmp> [width height]", file=sys.stderr)
		return 2
	in_path = Path(sys.argv[1])
	out_bmp = Path(sys.argv[2])
	if len(sys.argv) >= 5:
		width, height = int(sys.argv[3]), int(sys.argv[4])
	else:
		width, height = 240, 160
	img = Image.open(in_path)
	img = fit_letterbox(img, width, height)
	to_8bit_bmp(img, out_bmp)
	print(f"Saved {out_bmp} ({width}x{height})")

if __name__ == "__main__":
	raise SystemExit(main())
