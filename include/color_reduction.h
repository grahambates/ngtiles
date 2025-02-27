#ifndef COLOR_REDUCTION
#define COLOR_REDUCTION

#include "colors.h"

Palette *create_palette_from_tile(RGBA tile_pixels[], int index);
void index_tile_pixels(RGBA pixels[], Palette *palette, int dither, uint8_t indexed_pixels[]);

#endif // !COLOR_REDUCTION
