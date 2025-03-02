#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "consts.h"
#include "colors.h"
#include "safe_mem.h"

Palette *create_palette(int index) {
  Palette *palette = safe_calloc(sizeof(Palette), 1);
  palette->count = 1;
  palette->index = index;
  return palette;
}

// Add an RGBA colour to a palette, updating count and hash
int add_to_palette(Palette *palette, const RGBA *color) {
  if (palette->count == NUM_COLORS) return 1;
  palette->hash_set[palette->count] = color_hash(*color);
  palette->entries[palette->count++] = *color;
  return 0;
}

// Check whether palette contians a given colour
bool palette_contains(const Palette *p, const RGBA *color) {
  return palette_contains_hash(p, color_hash(*color));
}

// Check if a color exists in a palette
bool palette_contains_hash(const Palette *p, uint32_t color_hash) {
  for (int i = 0; i < p->count; i++) {
    if (p->hash_set[i] == color_hash) return true;
  }
  return false;
}

// Count shared colors between two palettes
int shared_colors(const Palette *a, const Palette *b) {
  int count = 0;
  for (int i = 0; i < a->count; i++) {
    if (palette_contains_hash(b, a->hash_set[i])) count++;
  }
  return count;
}

void print_palette(const Palette *palette) {
  for (int i = 1; i < palette->count; i++)
    printf("%02x%02x%02x ", palette->entries[i].r, palette->entries[i].g, palette->entries[i].b);
  printf("\n");
}

// Euclidean rgb colour difference
float color_distance(RGBA a, RGBA b) {
  float dr = a.r - b.r;
  float dg = a.g - b.g;
  float db = a.b - b.b;
  return dr * dr + dg * dg + db * db;
}

int find_closest_palette_color(const RGBA *pixel, const Palette *palette) {
  int best_index = 1;
  double best_distance = INFINITY;
  for (int i = 1; i < palette->count; i++) {
    double distance = color_distance(*pixel, palette->entries[i]);
    if (distance < best_distance) {
      best_distance = distance;
      best_index = i;
    }
  }
  return best_index;
}

// Quantise RGB to NeoGeo colour depth
RGBA quantize_rgb(const RGBA color) {
  // 5 bits per channel
  uint8_t r = color.r >> 3;
  uint8_t g = color.g >> 3;
  uint8_t b = color.b >> 3;
  // +1 shared lsb
  uint8_t luma = (int)((54.213 * r) + (182.376 * g) + (18.411 * b)) & 1;
  // Shift back to 8 bit RGB for preview
  return (RGBA){((r << 1) + luma) << 2, ((g << 1) + luma) << 2, ((b << 1) + luma) << 2, 0xff};
}
