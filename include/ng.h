#ifndef NG_H
#define NG_H

#include <stdint.h>
#include "colors.h"
#include "image.h"

// Converted image data
typedef struct NgImage {
  uint16_t palette_count;
  uint16_t **palettes;
  uint16_t tile_width;
  uint16_t tile_height;
  uint16_t tile_count;
  uint16_t *tile_map;
  uint16_t *palette_map;
  Image *preview;
} NgImage;

NgImage *create_ng_image(int w, int h);
void free_ng_image(NgImage *image);
void convert_tile(const uint8_t indexed_pixels[], uint8_t *tile_ptr);
void convert_fixed(const uint8_t indexed_pixels[], uint8_t *tile_ptr);
uint16_t *convert_palette(const Palette *palette);

#endif // NG_H
