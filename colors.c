#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "consts.h"
#include "colors.h"
#include "safe_mem.h"

Palette *create_palette(int index) {
  Palette *palette = safe_calloc(sizeof(Palette), 1);
  palette->count = 1;
  palette->index = index;
  return palette;
}

// Create unique hash for RGBA colour
uint32_t color_hash(RGBA c) {
    return ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) | ((uint32_t)c.b << 8) | c.a;
}

// Add an RGBA colour to a palette, updating count and hash
int add_to_palette(Palette *palette, RGBA *color) {
  if (palette->count == NUM_COLORS) return 1;
  palette->hash_set[palette->count] = color_hash(*color);
  palette->entries[palette->count++] = *color;
  return 0;
}

// Check whether palette contians a given colour
int palette_contains(Palette *palette, RGBA *color) {
  uint32_t hash = color_hash(*color);
  for (int i = 0; i < palette->count; i++) {
    if (hash == palette->hash_set[i]) {
      return true;
    }
  }
  return false;
}

// Check if a color exists in a palette
bool palette_contains_hash(Palette *p, uint32_t color_hash) {
    for (int i = 0; i < p->count; i++) {
        if (p->hash_set[i] == color_hash) return true;
    }
    return false;
}

// Count shared colors between two palettes
int shared_colors(Palette *a, Palette *b) {
    int count = 0;
    for (int i = 0; i < a->count; i++) {
        if (palette_contains_hash(b, a->hash_set[i])) count++;
    }
    return count;
}

void print_palette(Palette *palette) {
  for (int i = 1; i < palette->count; i++)
    printf("%02x%02x%02x ", palette->entries[i].r, palette->entries[i].g, palette->entries[i].b);
  printf("\n");
}

// Check if two RGB colours are identical (use hash instead for palette colours)
int same_color(RGBA *a, RGBA *b) {
  return memcmp(a, b, 3) == 0;
}

#if LAB_SPACE
// Convert sRGB to Linear RGB
static inline float srgb_to_linear(float c) {
    return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
}

// Convert RGB to XYZ
static void rgb_to_xyz(float r, float g, float b, float *x, float *y, float *z) {
    r = srgb_to_linear(r);
    g = srgb_to_linear(g);
    b = srgb_to_linear(b);
    *x = r * 0.4124564f + g * 0.3575761f + b * 0.1804375f;
    *y = r * 0.2126729f + g * 0.7151522f + b * 0.0721750f;
    *z = r * 0.0193339f + g * 0.1191920f + b * 0.9503041f;
}

// Convert XYZ to LAB
static void xyz_to_lab(float x, float y, float z, float *l, float *a, float *b) {
    const float Xn = 0.95047f, Yn = 1.00000f, Zn = 1.08883f;
    float f_x = (x / Xn > 0.008856f) ? powf(x / Xn, 1.0f / 3.0f) : (7.787f * x / Xn + 16.0f / 116.0f);
    float f_y = (y / Yn > 0.008856f) ? powf(y / Yn, 1.0f / 3.0f) : (7.787f * y / Yn + 16.0f / 116.0f);
    float f_z = (z / Zn > 0.008856f) ? powf(z / Zn, 1.0f / 3.0f) : (7.787f * z / Zn + 16.0f / 116.0f);
    *l = (116.0f * f_y) - 16.0f;
    *a = 500.0f * (f_x - f_y);
    *b = 200.0f * (f_y - f_z);
}

// Compute Euclidean Distance in LAB Space
float color_distance(RGBA a, RGBA b) {
    float x1, y1, z1, x2, y2, z2;
    rgb_to_xyz(a.r, a.g, a.b, &x1, &y1, &z1);
    rgb_to_xyz(b.r, b.g, b.b, &x2, &y2, &z2);
    float l1, a1, b1, l2, a2, b2;
    xyz_to_lab(x1, y1, z1, &l1, &a1, &b1);
    xyz_to_lab(x2, y2, z2, &l2, &a2, &b2);
    float dl = l1 - l2;
    float da = a1 - a2;
    float db = b1 - b2;
    return dl * dl + da * da + db * db;
}
#else
// Euclidean rgb colour difference
float color_distance(RGBA a, RGBA b) {
  float dr = a.r - b.r;
  float dg = a.g - b.g;
  float db = a.b - b.b;
  return dr * dr + dg * dg + db * db;
}
#endif

int find_closest_palette_color(RGBA *pixel, Palette *palette) {
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
RGBA quantize_rgb(RGBA color) {
  // 5 bits per channel
  uint8_t r = color.r >> 3;
  uint8_t g = color.g >> 3;
  uint8_t b = color.b >> 3;
  // +1 shared lsb
  uint8_t luma = (int)((54.213 * r) + (182.376 * g) + (18.411 * b)) & 1;
  // Shift back to 8 bit RGB for preview
  return (RGBA){((r << 1) + luma) << 2, ((g << 1) + luma) << 2, ((b << 1) + luma) << 2, 0xff};
}
