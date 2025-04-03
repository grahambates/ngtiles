#ifndef COLOR_REDUCTION_H
#define COLOR_REDUCTION_H

#include "colors.h"
#include "image.h"

Palette *create_palette_from_tile(const RGBA tile_pixels[], int px_count, int index);

void index_tile_pixels(const RGBA pixels[], const Palette *palette, uint8_t indexed_pixels[], int tile_span);

void apply_dithering(Image *source);

#endif // COLOR_REDUCTION_H
