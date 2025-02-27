#ifndef NG_H
#define NG_H

#include <stdint.h>
#include "colors.h"
#include "image.h"

// Converted image data
typedef struct NgImage {
  uint16_t tile_width;
  uint16_t tile_height;
  uint16_t tile_count;
  uint16_t *sprite_map;
  uint16_t *palette_map;
  uint16_t sprite_count;
  uint8_t **sprites;
  uint16_t palette_count;
  uint16_t **palettes;
  Image *preview;
} NgImage;

NgImage *create_ng_image(int w, int h);

void free_ng_image(NgImage *image);

uint8_t* convert_sprite(uint8_t indexed_pixels[]);

uint16_t *convert_palette(Palette *palette);

#endif
