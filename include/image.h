#ifndef IMAGE_H
#define IMAGE_H

#include "colors.h"
#include <stdint.h>

typedef struct Image {
  int width;
  int height;
  bool indexed;
  // RGB mode
  RGBA *pixels;
  // Indexed mode
  uint8_t *pixel_indices;
  Palette *palette;
} Image;

Image *create_image(int width, int height, bool indexed);

void free_image(Image *image);

Image *load_image(const char *filename);

int save_image(const char *filename, Image *image);

#endif
