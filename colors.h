#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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

// Create unique hash for RGBA colour
static inline uint32_t color_hash(RGBA c) {
    return ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) | ((uint32_t)c.b << 8) | c.a;
}

// Check if two RGB colours are identical (use hash instead for palette colours)
static inline int same_color(RGBA *a, RGBA *b) {
  return memcmp(a, b, 3) == 0;
}

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
