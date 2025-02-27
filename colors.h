#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>
#include <stdbool.h>

#include "consts.h"

typedef struct RGBA {
  uint8_t r, g, b, a;
} RGBA;

typedef struct Pallete {
  int count;
  int index;
  RGBA entries[NUM_COLORS];
  uint32_t hash_set[NUM_COLORS];
} Palette;

Palette *create_palette(int index);

int add_to_palette(Palette *palette, RGBA *color);

int palette_contains(Palette *palette, RGBA *color);

bool palette_contains_hash(Palette *p, uint32_t color_hash);

int shared_colors(Palette *a, Palette *b);

void print_palette(Palette *palette);

int find_closest_palette_color(RGBA *pixel, Palette *palette);


uint32_t color_hash(RGBA c);

int same_color(RGBA *a, RGBA *b);

float color_distance(RGBA a, RGBA b);

RGBA quantize_rgb(RGBA color);

#endif
