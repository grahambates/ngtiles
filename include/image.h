#ifndef IMAGE_H
#define IMAGE_H

#include "colors.h"
#include <stdint.h>

typedef struct Image {
  int width;
  int height;
  bool indexed; // Was the original PNG indexed?
  bool fixed_palette; // Are we able to use the indexed palette?
  // RGB mode
  RGBA *pixels;
  // Fixed palette mode
  uint8_t *pixel_indices;
  Palette *palette;
} Image;

Image *create_image(int width, int height, bool fixed_palette);

void free_image(Image *image);

Image *load_image(const char *filename);

int save_image(const char *filename, Image *image);

#endif
