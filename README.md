# NG Tiles

NeoGeo image conversion tool

## Usage:

```
Usage: ngtiles [options] <source.png>...
Options:
  -r, --rom-dir               ROM directory
  -o, --output-dir=<dir>      Tiles output directory
  -v, --verbose               Enable verbose output
  -h, --help                  Display this help message
```

## Example

```
ngtiles -r build/roms/mslug2 -o data example-1.png example-2.png example-3.png
```
