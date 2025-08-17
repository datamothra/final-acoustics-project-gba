# FINAL Acoustics Project - GBA

A Game Boy Advance project built with Butano, featuring a custom title screen background.

## Requirements

- devkitPro with devkitARM
- Butano library
- Python 3
- Pillow (PIL)

## Building

```bash
make -j4
```

The output ROM will be `Fuck.gba`.

## Project Structure

- `src/main.cpp` - Main application code
- `graphics/` - Image assets and configuration
- `tools/prepare_image.py` - Image conversion utility
- `Makefile` - Build configuration

## Image Conversion

The project includes a tool to convert images to 8-bit BMP format suitable for Butano:

```bash
python3 tools/prepare_image.py <input_image> <output_bmp> [width height]
```

## License

This project uses Butano which is licensed under the zlib License.
