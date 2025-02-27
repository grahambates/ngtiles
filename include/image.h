#ifndef IMAGE_H
#define IMAGE_H

#include "colors.h"

typedef struct Image {
  RGBA *pixels;
  int width;
  int height;
} Image;

Image *create_image(int width, int height);

void free_image(Image *image);

Image *load_image(const char *filename);

int save_image(const char *filename, Image *image);

#endif
