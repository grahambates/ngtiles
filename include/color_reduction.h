#ifndef COLOR_REDUCTION
#define COLOR_REDUCTION

#include "colors.h"
#include "image.h"

Palette *create_palette_from_tile(RGBA tile_pixels[], int index);
void index_tile_pixels(RGBA pixels[], Palette *palette, uint8_t indexed_pixels[]);
void apply_dithering(Image *source);

#endif // !COLOR_REDUCTION
