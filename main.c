// TODO:
// separate path for indexed mode
// lossy palette reduction
// pre-dither?

#include <math.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "log.h"
#include "safe_mem.h"

#define DITHER 1
#define LAB_SPACE 1

#define NUM_COLORS 16
#define TILE_SIZE 16
#define BORDER_SIZE 1 // Number of pixels to extend around each tile
#define TILE_SIZE_EXP (TILE_SIZE+2*BORDER_SIZE)
#define NUM_BOXES (NUM_COLORS-1) // Colour 0 is always transparent
#define MAX_PALETTES 256
#define MAX_SPRITES 512
#define MAX_TILES 512
#define MAX_FILENAME_LEN 1024
#define ROM_SIZE 0x1000000
#define SPRITE_SIZE (TILE_SIZE*TILE_SIZE/2) // Two px per byte

typedef struct RGBA {
  uint8_t r, g, b, a;
} RGBA;

typedef struct Pallete {
  int count;
  int index;
  RGBA entries[NUM_COLORS];
  uint32_t hash_set[NUM_COLORS];
} Palette;

typedef struct Mapping {
  int original_index;
  int final_index;
} Mapping;

typedef struct Image {
  RGBA *pixels;
  int width;
  int height;
} Image;

// Pixel grouping for median cut
typedef struct Box {
  RGBA *pixels;
  int count;
  int capacity;
  uint8_t rmin, rmax;
  uint8_t gmin, gmax;
  uint8_t bmin, bmax;
} Box;

// Converted image data
typedef struct NgImage {
  uint16_t tile_width;
  uint16_t tile_height;
  uint16_t tile_count;
  uint16_t *sprite_map;
  uint16_t *palette_map;
  uint16_t sprite_count;
  uint8_t **sprites;
  uint16_t palette_count;
  uint16_t **palettes;
  Image *preview;
} NgImage;

typedef struct {
    int a, b;
    int score;
} MergeCandidate;

// Palettes:

static Palette *create_palette(int index) {
  Palette *palette = safe_calloc(sizeof(Palette), 1);
  palette->count = 1;
  palette->index = index;
  return palette;
}

// Create unique hash for RGBA colour
static inline uint32_t color_hash(RGBA c) {
    return ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) | ((uint32_t)c.b << 8) | c.a;
}

// Add an RGBA colour to a palette, updating count and hash
static int add_to_palette(Palette *palette, RGBA *color) {
  if (palette->count == NUM_COLORS) return 1;
  palette->hash_set[palette->count] = color_hash(*color);
  palette->entries[palette->count++] = *color;
  return 0;
}

// Check whether palette contians a given colour
static int palette_contains(Palette *palette, RGBA *color) {
  uint32_t hash = color_hash(*color);
  for (int i = 0; i < palette->count; i++) {
    if (hash == palette->hash_set[i]) {
      return true;
    }
  }
  return false;
}

// Check if a color exists in a palette
static bool color_in_palette(Palette *p, uint32_t color_hash) {
    for (int i = 0; i < p->count; i++) {
        if (p->hash_set[i] == color_hash) return true;
    }
    return false;
}

// Count shared colors between two palettes
static int shared_colors(Palette *a, Palette *b) {
    int count = 0;
    for (int i = 0; i < a->count; i++) {
        if (color_in_palette(b, a->hash_set[i])) count++;
    }
    return count;
}

static void print_palette(Palette *palette) {
  for (int i = 1; i < palette->count; i++)
    printf("%02x%02x%02x ", palette->entries[i].r, palette->entries[i].g, palette->entries[i].b);
  printf("\n");
}

// Check if two RGB colours are identical (use hash instead for palette colours)
static inline int same_color(RGBA *a, RGBA *b) {
  return memcmp(a, b, 3) == 0;
}

// RGBA images (i.e. PNG):

static Image *create_image(int width, int height) {
  Image *image = safe_malloc(sizeof(Image));
  image->width = width;
  image->height = height;
  image->pixels = safe_malloc(width * height * sizeof(RGBA));
  return image;
}

void free_image(Image *image) {
  free(image->pixels);
  free(image);
}

// NeoGeo image data

static NgImage *create_ng_image(int w, int h) {
  NgImage *ng_image = safe_malloc(sizeof(NgImage));
  ng_image->tile_width = w;
  ng_image->tile_height = h;
  ng_image->tile_count = w * h;
  ng_image->sprite_count = 0;
  ng_image->palette_count = 0;
  ng_image->palettes = safe_malloc(MAX_PALETTES * sizeof(uint16_t *));
  ng_image->sprites = safe_malloc(MAX_SPRITES * sizeof(uint8_t *));
  ng_image->sprite_map = safe_malloc(MAX_TILES * sizeof(uint16_t));
  ng_image->palette_map = safe_malloc(MAX_TILES * sizeof(uint16_t));
  return ng_image;
}

void free_ng_image(NgImage *image) {
  for (int i = 0; i < image->palette_count; i++) {
    free(image->palettes[i]);
  }
  for (int i = 0; i < image->sprite_count; i++) {
    free(image->sprites[i]);
  }
  free(image->sprite_map);
  free(image->palettes);
  if (image->preview) free_image(image->preview);
  free(image);
}

// Swap byte order
static inline uint16_t swap16(uint16_t val) { return (val >> 8) | (val << 8); }

// Write NgImage tiles data (palette + mappings) to disk
static int write_tiles(char *filename, NgImage *image, int offset) {
  // UWORD palette_count;                     Number of palette entries
  // UWORD palette_entries[palette_count*16]; Color values in NG format
  // UWORD tile_width;                        Width of image in tiles
  // UWORD tile_height;                       Height of image in tiles
  // struct {
  //     UWORD sprite_index;                  Index of the sprite in the sprite table
  //     UWORD palette_index;                 Index of the palette for this tile
  // } mappings[tile_width * tile_height];    Mappings per tile

  uint16_t data[1 + MAX_PALETTES*NUM_COLORS + 2 + MAX_TILES*2];
  int index = 0;
  data[index++] = swap16(image->palette_count);

  for (int i = 0; i < image->palette_count; i++) {
    if (!image->palettes[i]) continue;
    for (int j = 0; j < NUM_COLORS; j++) {
      data[index++] = swap16(image->palettes[i][j]);
    }
  }
  data[index++] = swap16(image->tile_width);
  data[index++] = swap16(image->tile_height);
  for (int i = 0; i < image->tile_count; i++) {
    data[index++] = swap16(image->sprite_map[i] + offset);
    data[index++] = swap16(image->palette_map[i]);
  }

  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    error_log("Error writing to file %s: %s", filename, strerror(errno));
    return errno;
  }
  fwrite(data, sizeof(uint16_t), index, fp);
  fclose(fp);
  return 0;
}

static const uint8_t bit_reversal_table[256] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
  0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
  0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
  0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
  0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
  0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
  0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
  0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
  0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
  0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
  0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
  0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
  0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
  0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
  0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
  0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
  0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
  0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
  0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
  0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
  0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
  0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
  0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
  0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
  0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
  0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
  0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
  0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
  0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
  0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
  0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
  0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

// Convert indexed data to Neo Geo sprite layout
// see https://wiki.neogeodev.org/index.php?title=Sprite_graphics_format
static uint8_t* convert_sprite(uint8_t indexed_pixels[]) {
  uint8_t *result = safe_malloc(SPRITE_SIZE);
  int index = 0;
  for (int x = 8; x >= 0; x -= 8) {
    for (int y = 0; y < 16; y++) {
      int plane1 = 0, plane2 = 0, plane3 = 0, plane4 = 0;
      int bitpos = 7;
      for (int i = 0; i < 8; i++) {
        int pixel = indexed_pixels[(y * 16) + x + i];
        if (pixel & 1) plane1 |= (1 << bitpos);
        if (pixel & 2) plane2 |= (1 << bitpos);
        if (pixel & 4) plane3 |= (1 << bitpos);
        if (pixel & 8) plane4 |= (1 << bitpos);
        bitpos--;
      }
      result[index++] = bit_reversal_table[plane1];
      result[index++] = bit_reversal_table[plane3];
      result[index++] = bit_reversal_table[plane2];
      result[index++] = bit_reversal_table[plane4];
    }
  }
  return result;
}

// Quantise RGB to NeoGeo colour depth
static RGBA quantize_rgb(RGBA color) {
  // 5 bits per channel
  uint8_t r = color.r >> 3;
  uint8_t g = color.g >> 3;
  uint8_t b = color.b >> 3;
  // +1 shared lsb
  uint8_t luma = (int)((54.213 * r) + (182.376 * g) + (18.411 * b)) & 1;
  // Shift back to 8 bit RGB for preview
  return (RGBA){((r << 1) + luma) << 2, ((g << 1) + luma) << 2, ((b << 1) + luma) << 2, 0xff};
}

// Convert quantised RGB to NeoGeo native bit order
// see https://wiki.neogeodev.org/index.php?title=Colors
static uint16_t rgb_to_ng(RGBA color) {
  // RGB source:                         7  6  5  4  3  2  1  0
  // NG equiv:                           4  3  2  1  0  d __ __
  // Target:     D R0 G0 B0 R4 R3 R2 R1 G4 G3 G2 G1 B4 B3 B2 B1
  return ((color.b >> 4) & 0xf) | (color.g & 0xf0) | ((color.r << 4) & 0xf00) |
         ((color.b << 9) & 0x1000) | ((color.g << 10) & 0x2000) |
         ((color.r << 11) & 0x4000) | (~(color.b << 13) & 0x8000);
}

// Convert palette to native NeoGeo format
static uint16_t *convert_palette(Palette *palette) {
  uint16_t *ng_palette = safe_calloc(NUM_COLORS, sizeof(uint16_t));
  for (int i = 0; i < palette->count; i++) {
    ng_palette[i] = rgb_to_ng(palette->entries[i]);
  }
  return ng_palette;
}

// Median cut helpers:

static Box *create_box(int capacity) {
  Box *box = safe_malloc(sizeof(Box));
  box->pixels = safe_malloc(capacity * sizeof(RGBA));
  box->count = 0;
  box->capacity = capacity;
  box->rmin = box->gmin = box->bmin = 0xff;
  box->rmax = box->gmax = box->bmax = 0;
  return box;
}

static void add_to_box(Box *box, RGBA pixel) {
  if (box->count < box->capacity) {
    box->pixels[box->count++] = pixel;
    box->rmin = pixel.r < box->rmin ? pixel.r : box->rmin;
    box->rmax = pixel.r > box->rmax ? pixel.r : box->rmax;
    box->gmin = pixel.g < box->gmin ? pixel.g : box->gmin;
    box->gmax = pixel.g > box->gmax ? pixel.g : box->gmax;
    box->bmin = pixel.b < box->bmin ? pixel.b : box->bmin;
    box->bmax = pixel.b > box->bmax ? pixel.b : box->bmax;
  }
}

static RGBA get_box_average(Box *box) {
    RGBA avg = {0, 0, 0, 0xff};
    if (box->count == 0) return avg;
    unsigned long r = 0, g = 0, b = 0;
    for (int i = 0; i < box->count; i++) {
        r += box->pixels[i].r;
        g += box->pixels[i].g;
        b += box->pixels[i].b;
    }
    avg.r = (uint8_t)(r / box->count);
    avg.g = (uint8_t)(g / box->count);
    avg.b = (uint8_t)(b / box->count);
    return avg;
}

static void free_box(Box *box) {
  free(box->pixels);
  free(box);
}

// Comparison functions for sorting
int compare_red(const void *a, const void *b) {
  return ((RGBA *)a)->r - ((RGBA *)b)->r;
}
int compare_green(const void *a, const void *b) {
  return ((RGBA *)a)->g - ((RGBA *)b)->g;
}
int compare_blue(const void *a, const void *b) {
  return ((RGBA *)a)->b - ((RGBA *)b)->b;
}

// Get optimal <=16 color palette for tile
static Palette *create_palette_from_tile(RGBA tile_pixels[], int index) {
  Palette *palette = create_palette(index);

  // Initialize first box with all pixels
  Box **boxes = safe_malloc(NUM_BOXES * sizeof(Box *));
  boxes[0] = create_box(TILE_SIZE_EXP * TILE_SIZE_EXP);
  int err = 0;
  for (int i = 0; i < TILE_SIZE_EXP * TILE_SIZE_EXP; i++) {
    add_to_box(boxes[0], tile_pixels[i]);
    // Also try to build a single palette, and track whether it's full
    if (!err && tile_pixels[i].a > 0) {
      RGBA quantized = quantize_rgb(tile_pixels[i]);
      if (!palette_contains(palette, &quantized)) {
        err = add_to_palette(palette, &quantized);
      }
    }
  }

  // If we were able to successfully add all the colours to the palette, just return it
  if (!err) {
    if (verbose) {
      verbose_log("original palette: %d colors\n", palette->count);
      print_palette(palette);
    }
    return palette;
  }

  // Create optimal palette for tile using median cut

  int num_boxes = 1;
  palette->count = 1; // reset count

  // Split boxes until we have desired number of colors
  while (num_boxes < NUM_BOXES) {
    // Find box with largest dimension
    int box_to_split = 0;
    int max_range = 0;

    for (int i = 0; i < num_boxes; i++) {
      Box *box = boxes[i];
      int rrange = box->rmax - box->rmin;
      int grange = box->gmax - box->gmin;
      int brange = box->bmax - box->bmin;
      int largest_range = rrange > grange ? (rrange > brange ? rrange : brange)
                                          : (grange > brange ? grange : brange);

      if (largest_range > max_range) {
        max_range = largest_range;
        box_to_split = i;
      }
    }

    if (max_range == 0)
      break; // No more unique colors to split

    Box *box = boxes[box_to_split];
    int rrange = box->rmax - box->rmin;
    int grange = box->gmax - box->gmin;
    int brange = box->bmax - box->bmin;

    // Sort by longest dimension
    if (rrange >= grange && rrange >= brange) {
      qsort(box->pixels, box->count, sizeof(RGBA), compare_red);
    } else if (grange >= rrange && grange >= brange) {
      qsort(box->pixels, box->count, sizeof(RGBA), compare_green);
    } else {
      qsort(box->pixels, box->count, sizeof(RGBA), compare_blue);
    }

    // Create two new boxes
    int median = box->count / 2;
    Box *box1 = create_box(median);
    Box *box2 = create_box(box->count - median);

    // Distribute pixels
    for (int i = 0; i < median; i++) {
      add_to_box(box1, box->pixels[i]);
    }
    for (int i = median; i < box->count; i++) {
      add_to_box(box2, box->pixels[i]);
    }

    // Replace original box with first half
    free_box(boxes[box_to_split]);
    boxes[box_to_split] = box1;

    // Add second half to end
    boxes[num_boxes++] = box2;
  }

  for (int i = 0; i < num_boxes; i++) {
    RGBA c = quantize_rgb(get_box_average(boxes[i]));
    if (!palette_contains(palette, &c)) {
      add_to_palette(palette, &c);
    }
    free_box(boxes[i]);
  }

  if (verbose) {
    verbose_log("custom palette %d colors:\n", palette->count);
    print_palette(palette);
  }
  free(boxes);

  return palette;
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
static inline float color_distance(RGBA a, RGBA b) {
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
static inline float color_distance(RGBA a, RGBA b) {
  float dr = a.r - b.r;
  float dg = a.g - b.g;
  float db = a.b - b.b;
  return dr * dr + dg * dg + db * db;
}
#endif

static int find_closest_palette_color(RGBA *pixel, Palette *palette) {
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

// Read png image file
static Image *load_png(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    error_log("Failed to load image: %s: %s\n", filename, strerror(errno));
    return NULL;
  }

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(fp);
    return NULL;
  }
  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(fp);
    return NULL;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return NULL;
  }
  png_init_io(png, fp);
  png_read_info(png, info);

  int width = png_get_image_width(png, info);
  int height = png_get_image_height(png, info);
  png_byte color_type = png_get_color_type(png, info);
  png_byte bit_depth = png_get_bit_depth(png, info);

  verbose_log("%dx%d, color_type: %d, bit_depth: %d\n", width, height, color_type, bit_depth);

  // Convert indexed and grayscale images to RGB
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    if (bit_depth < 8)
      png_set_expand_gray_1_2_4_to_8(png);
    png_set_gray_to_rgb(png);
  }

  // Ensure all images have an alpha channel
  if (!(color_type & PNG_COLOR_MASK_ALPHA))
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER); // Add full alpha if absent

  // Convert transparency chunks to alpha
  if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);

  // Convert all images to RGBA (8 bits per channel)
  if (bit_depth == 16)
    png_set_strip_16(png); // Convert 16-bit to 8-bit

  png_read_update_info(png, info);

  Image *image = create_image(width, height);

  png_bytep *row_pointers = safe_malloc(sizeof(png_bytep) * height);
  for (int y = 0; y < height; y++) {
    row_pointers[y] = safe_malloc(png_get_rowbytes(png, info));
  }

  png_read_image(png, row_pointers);

  // Copy data to our image structure
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      png_byte *ptr = &(row_pointers[y][x * 4]);
      image->pixels[y * width + x].r = ptr[0];
      image->pixels[y * width + x].g = ptr[1];
      image->pixels[y * width + x].b = ptr[2];
      image->pixels[y * width + x].a = ptr[3];
    }
    free(row_pointers[y]);
  }
  free(row_pointers);

  png_destroy_read_struct(&png, &info, NULL);
  fclose(fp);
  return image;
}

// Save png of tiles recombined for debugging purposes
static int save_preview(const char *filename, Image *image) {
  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    error_log("Failed to create preview file %s: %s\n", filename, strerror(errno));
    return errno;
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(fp);
    return 1;
  }
  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, NULL);
    fclose(fp);
    return 1;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 1;
  }
  png_init_io(png, fp);

  // Write header
  png_set_IHDR(png, info, image->width, image->height, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  // Write as RGB
  png_write_info(png, info);
  png_bytep row = safe_malloc(image->width * 4);
  for (int y = 0; y < image->height; y++) {
    for (int x = 0; x < image->width; x++) {
      RGBA pixel = image->pixels[y * image->width + x];
      row[x * 4] = pixel.r;
      row[x * 4 + 1] = pixel.g;
      row[x * 4 + 2] = pixel.b;
      row[x * 4 + 3] = pixel.a;
    }
    png_write_row(png, row);
  }

  free(row);
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  fclose(fp);
  return 0;
}

// Clamp function for color values (0-255)
static inline uint8_t clamp(int value) {
  return (value < 0) ? 0 : (value > 255) ? 255 : (uint8_t)value;
}

// Apply error diffusion to tile
// This updates the surrounding pixel RGB values in the current tile data
// TODO:
// this would be better if it used a separate error buffer that covers the whole image
// This way, errors would be carried over between tiles.
static void apply_dithering(RGBA pixels[], int x, int y, RGBA *pixel, RGBA *best_color) {
  // 1/16 | - # 7 |
  //      | 1 3 5 |
  const float right_weight = 7.0f/16.0f;
  const float bottom_left_weight = 1.0f/16.0f;
  const float bottom_weight = 3.0f/16.0f;
  const float bottom_right_weight = 5.0f/16.0f;

  int error_r = pixel->r - best_color->r;
  int error_g = pixel->g - best_color->g;
  int error_b = pixel->b - best_color->b;

  // Distribute error to right pixel
  if (x + 1 < TILE_SIZE) {
    RGBA *right = &pixels[y * TILE_SIZE + (x + 1)];
    right->r = clamp(right->r + (int)(error_r * right_weight));
    right->g = clamp(right->g + (int)(error_g * right_weight));
    right->b = clamp(right->b + (int)(error_b * right_weight));
  }

  if (y + 1 < TILE_SIZE) {
    // Distribute error to bottom-left pixel
    if (x > 0) {
      RGBA *bottom_left = &pixels[(y + 1) * TILE_SIZE + (x - 1)];
      bottom_left->r = clamp(bottom_left->r + (int)(error_r * bottom_left_weight));
      bottom_left->g = clamp(bottom_left->g + (int)(error_g * bottom_left_weight));
      bottom_left->b = clamp(bottom_left->b + (int)(error_b * bottom_left_weight));
    }

    // Distribute error to bottom pixel
    RGBA *bottom = &pixels[(y + 1) * TILE_SIZE + x];
    bottom->r = clamp(bottom->r + (int)(error_r * bottom_weight));
    bottom->g = clamp(bottom->g + (int)(error_g * bottom_weight));
    bottom->b = clamp(bottom->b + (int)(error_b * bottom_weight));

    // Distribute error to bottom-right pixel
    if (x + 1 < TILE_SIZE) {
      RGBA *bottom_right = &pixels[(y + 1) * TILE_SIZE + (x + 1)];
      bottom_right->r = clamp(bottom_right->r + (int)(error_r * bottom_right_weight));
      bottom_right->g = clamp(bottom_right->g + (int)(error_g * bottom_right_weight));
      bottom_right->b = clamp(bottom_right->b + (int)(error_b * bottom_right_weight));
    }
  }
}

// Get closest palette index for each pixel in image
static void index_tile_pixels(RGBA pixels[], Palette *palette, int dither, uint8_t indexed_pixels[]) {
  for (int y = 0; y < TILE_SIZE; y++) {
    for (int x = 0; x < TILE_SIZE; x++) {
      RGBA *pixel = &pixels[y * TILE_SIZE + x];

      // Fixed index for transparent pixels
      if (pixel->a == 0) {
        indexed_pixels[y * TILE_SIZE + x] = 0;
        continue;
      }

      // Find closest palette color
      int best_index = find_closest_palette_color(pixel, palette);
      RGBA best_color = palette->entries[best_index];

      // Add additional dither colors if we have free slots
      if (dither && palette->count < NUM_COLORS) {
        RGBA actual_color = quantize_rgb(*pixel);
        if (!same_color(&best_color, &actual_color)) {
          best_color = actual_color;
          best_index = palette->count;
          add_to_palette(palette, &actual_color);
        }
      }

      indexed_pixels[y * TILE_SIZE + x] = best_index;

      if (dither) {
        apply_dithering(pixels, x, y, pixel, &best_color);
      }
    }
  }
}

// Extracts a tile from the image with a slight border
// This is used for palette generation, and reduces visible borders between tiles
static void extract_tile_pixels_with_neighbors(Image *source, int tile_x, int tile_y, RGBA pixels[]) {
  for (int y = -BORDER_SIZE; y < TILE_SIZE + BORDER_SIZE; y++) {
    for (int x = -BORDER_SIZE; x < TILE_SIZE + BORDER_SIZE; x++) {
      int sx = tile_x * TILE_SIZE + x;
      int sy = tile_y * TILE_SIZE + y;
      int offset = (y + BORDER_SIZE) * TILE_SIZE_EXP + (x + BORDER_SIZE);

      // Ensure within bounds
      if (sx >= 0 && sx < source->width && sy >= 0 && sy < source->height) {
        pixels[offset] = source->pixels[sy * source->width + sx];
      } else {
        // Out of bounds
        pixels[offset] = (RGBA){0, 0, 0, 0};
      }
    }
  }
}

// Extract 16x16 tile pixels from the source image
static void extract_tile_pixels(Image *source, int x, int y, RGBA pixels[]) {
  for (int ty = 0; ty < TILE_SIZE; ty++) {
    for (int tx = 0; tx < TILE_SIZE; tx++) {
      int sx = x * TILE_SIZE + tx;
      int sy = y * TILE_SIZE + ty;
      if (sx < source->width && sy < source->height) {
        pixels[ty * TILE_SIZE + tx] = source->pixels[sy * source->width + sx];
      } else {
        // Fill empty space with transparent px if image size is not a multiple of 16
        pixels[ty * TILE_SIZE + tx] = (RGBA){0, 0 ,0, 0};
      }
    }
  }
}

// Compare function for sorting (max-heap)
int compare_merges(const void *x, const void *y) {
  return ((MergeCandidate *)y)->score - ((MergeCandidate *)x)->score;
}

// Merge palette a into palette b (in place, avoiding duplicates)
static void merge_palettes(Palette *a, Palette *b) {
  for (int i = 0; i < a->count; i++) {
    if (!color_in_palette(b, a->hash_set[i])) {
      b->entries[b->count] = a->entries[i];
      b->hash_set[b->count] = a->hash_set[i];
      b->count++;
    }
  }
}

// Reduce palettes efficiently using a priority queue
static void reduce_palettes(Palette *palettes[], int palette_count, int *merged) {
  // Max heap for best merge candidates
  MergeCandidate *queue = safe_malloc(palette_count * palette_count * sizeof(MergeCandidate));
  int queue_size = 0;

  // Precompute all valid merges
  for (int a = 0; a < palette_count; a++) {
    if (merged[a] >= 0) continue;
    for (int b = a + 1; b < palette_count; b++) {
      if (merged[b] >= 0) continue;
      int overlap = shared_colors(palettes[a], palettes[b]);
      int size = palettes[a]->count + palettes[b]->count - overlap;
      if (size <= NUM_COLORS) {
        queue[queue_size++] = (MergeCandidate){a, b, overlap};
      }
    }
  }

  // Sort by best score (descending order)
  qsort(queue, queue_size, sizeof(MergeCandidate), compare_merges);

  // Process merges using max-heap
  while (queue_size > 0) {
    int best_a = queue[0].a;
    int best_b = queue[0].b;

    // Remove best merge from queue
    queue_size--;
    for (int i = 0; i < queue_size; i++) queue[i] = queue[i + 1];

    if (merged[best_a] >= 0 || merged[best_b] >= 0 || queue[0].score == 0) continue; // Skip already merged palettes

    verbose_log("Best merge: %d + %d (score: %d)\n", best_a, best_b, queue[0].score);
    merged[best_a] = best_b;  // Track merging
    merge_palettes(palettes[best_a], palettes[best_b]);

    // Update affected pairs **only** (avoid recomputing everything)
    // This change can only have made merges including best_b change score, or no longer valid
    // merges inlcuding best_a will be ignored
    for (int i = 0; i < queue_size; i++) {
      if (queue[i].a == best_b || queue[i].b == best_b) {
        int new_overlap = shared_colors(palettes[queue[i].a], palettes[queue[i].b]);
        int new_size = palettes[queue[i].a]->count + palettes[queue[i].b]->count - new_overlap;
        if (new_size <= NUM_COLORS) {
          // Still valid - update score
          queue[i].score = new_overlap;
        } else {
          // No longer a valid merge
          // set zero score to be ignored
          queue[i].score = 0;
        }
      }
    }

    // Resort queue after updating affected pairs
    qsort(queue, queue_size, sizeof(MergeCandidate), compare_merges);
  }
  free(queue);

  // Resolve final mappings
  for (int i = 0; i < palette_count; i++) {
    int index = i;
    while (merged[index] > 0 && merged[index] != index) {
      index = merged[index];
    }
    merged[i] = index;
  }
}

// Convert source image to NgImage structure
NgImage *convert_image(Image *source) {
  // Calculate number of tiles
  int tiles_x = (source->width + TILE_SIZE - 1) / TILE_SIZE;
  int tiles_y = (source->height + TILE_SIZE - 1) / TILE_SIZE;
  verbose_log("Splitting into %dx%d tiles\n", tiles_x, tiles_y);

  Image *preview = create_image(source->width, source->height);

  NgImage *ng_image = create_ng_image(tiles_x, tiles_y);
  ng_image->preview = preview;


  // First generate specific palettes for each tile:
  Palette *palettes[MAX_TILES];
  RGBA expanded_pixels[TILE_SIZE_EXP * TILE_SIZE_EXP];
  int palette_count = 0;
  for (int ty = 0; ty < tiles_y; ty++) {
    for (int tx = 0; tx < tiles_x; tx++) {
      verbose_log("Processing tile [%d,%d]\n", tx, ty);
      extract_tile_pixels_with_neighbors(source, tx, ty, expanded_pixels);
      palettes[palette_count] = create_palette_from_tile(expanded_pixels, palette_count);
      palette_count++;
    }
  }

  // Next reduce number of palettes:
  int merged_map[MAX_TILES] = {0};
  for (int i = 0; i < palette_count; i++) {
    merged_map[i] = -1;
  }
  reduce_palettes(palettes, palette_count, merged_map);

  // Add unique palettes to ng image and track index mapping
  int ng_palette_map[MAX_TILES];
  for (int i = 0; i < MAX_TILES; i++) {
    // Only add palettes which have not been merged
    if (merged_map[i] == i) {
      ng_palette_map[i] = ng_image->palette_count;
      ng_image->palettes[ng_image->palette_count++] = convert_palette(palettes[i]);
    }
  }

  // Now process each tile:
  int tile_index = 0;
  RGBA tile_pixels[TILE_SIZE * TILE_SIZE];
  uint8_t indexed_pixels[TILE_SIZE * TILE_SIZE];
  for (int ty = 0; ty < tiles_y; ty++) {
    for (int tx = 0; tx < tiles_x; tx++) {
      // Extract only the actual tile pixels now
      extract_tile_pixels(source, tx, ty, tile_pixels);
      // Map palette indexes to pixels
      int palette_index = merged_map[tile_index];
      Palette *palette = palettes[palette_index];
      index_tile_pixels(tile_pixels, palette, DITHER, indexed_pixels);

      // Add sprite data and add to map, currently sequential 1:1
      // TODO: depdupe
      ng_image->sprites[ng_image->sprite_count] = convert_sprite(indexed_pixels);
      ng_image->sprite_map[ng_image->sprite_count] = ng_image->sprite_count;
      ng_image->sprite_count++;

      // Add mapped palette index to map
      ng_image->palette_map[tile_index] = ng_palette_map[palette_index];

      // Copy indexed pixels to preview
      for (int y = 0; y < TILE_SIZE; y++) {
        for (int x = 0; x < TILE_SIZE; x++) {
          int dst_x = tx * TILE_SIZE + x;
          int dst_y = ty * TILE_SIZE + y;
          if (dst_x < preview->width && dst_y < preview->height) {
            preview->pixels[dst_y * preview->width + dst_x] = palette->entries[indexed_pixels[y * TILE_SIZE + x]];
          }
        }
      }
      tile_index++;
    }
  }

  for (int i = 0; i < tile_index; i++) {
    free(palettes[i]);
  }
  return ng_image;
}

// Create filenames for tile data and preview based on source file
void generate_filenames(const char *source_file, const char *output_dir, char *tiles_file, char *preview_file) {
    char base[MAX_FILENAME_LEN];
    char *filename = strrchr(source_file, '/'); // Find last '/' for basename extraction

    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = (char *)source_file; // No directory in source_file
    }

    char *dot = strrchr(filename, '.'); // Find last '.'
    if (dot) {
        size_t base_len = dot - filename;
        strncpy(base, filename, base_len);
        base[base_len] = '\0';  // Null-terminate
    } else {
        strncpy(base, filename, MAX_FILENAME_LEN - 1);
        base[MAX_FILENAME_LEN - 1] = '\0';
    }

    // Use output_dir if provided, otherwise use current directory
    if (output_dir && strlen(output_dir) > 0) {
        snprintf(tiles_file, MAX_FILENAME_LEN, "%s/%s.tiles", output_dir, base);
        snprintf(preview_file, MAX_FILENAME_LEN, "%s/%s-preview.png", output_dir, base);
    } else {
        snprintf(tiles_file, MAX_FILENAME_LEN, "%s.tiles", base);
        snprintf(preview_file, MAX_FILENAME_LEN, "%s-preview.png", base);
    }
}

// Helper to wtite ROM with file pattern
static int write_rom(char *pattern, char *rom_dir, void *data) {
    char filename[MAX_FILENAME_LEN];
    snprintf(filename, MAX_FILENAME_LEN, pattern, rom_dir);
    printf("  %s\n", filename);
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
      error_log("Error writing to file %s: %s\n", filename, strerror(errno));
      return errno;
    }
    fwrite(data, 1, 0x800000, fp);
    return 0;
}

// Save combined sprite graphics data to roms directory
static int save_roms(uint8_t rom_data[], char *rom_dir) {
  // Split into odd/even ROMs
  uint8_t *char1 = safe_malloc(ROM_SIZE/2);
  uint8_t *char2 = safe_malloc(ROM_SIZE/2);
  int i = 0;
  for (int bindex = 0; bindex < ROM_SIZE; bindex += 2) {
    char1[i] = rom_data[bindex];
    char2[i++] = rom_data[bindex + 1];
  }

  int err =
    write_rom("%s/241-c1.c1", rom_dir, char1) ||
    write_rom("%s/241-c2.c2", rom_dir, char2) ||
    write_rom("%s/241-c3.c3", rom_dir, char1) ||
    write_rom("%s/241-c4.c4", rom_dir, char2);

  free(char1);
  free(char2);
  return err;
}

void print_usage(const char *prog_name) {
  printf("Usage: %s [options] <source.png>...\n", prog_name);
  printf("Options:\n");
  printf("  -r, --rom-dir               ROM directory\n");
  printf("  -o, --output-dir=<dir>      Tiles output directory\n");
  printf("  -v, --verbose               Enable verbose output\n");
  printf("  -h, --help                  Display this help message\n");
}

int main(int argc, char *argv[]) {
  int opt;
  char *output_dir = "";
  char *rom_dir = "";

  // Define long options
  static struct option long_options[] = {
      {"output-dir", required_argument, 0, 'o'},
      {"verbose", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "o:r:vh", long_options, NULL)) != -1) {
    switch (opt) {
    case 'r':
      rom_dir = optarg;
      break;
    case 'o':
      output_dir = optarg;
      break;
    case 'v':
      verbose = 1;
      break;
    case 'h':
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    case '?':
      error_log("Invalid option. Use -h for help.\n");
      return EXIT_FAILURE;
    }
  }

  // Ensure we have at least one positional argument
  if (argc <= optind) {
    error_log("Error: Incorrect number of arguments.\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  uint8_t *rom_data = safe_calloc(ROM_SIZE, 1);
  int tile_id = 1; // first tile should always be blank

  // Process file list in positonal args
  for (int i = optind; i < argc; i++) {
    char *source_file = argv[i];
    printf("Processing file %s\n", source_file);
    Image *source = load_png(source_file);
    if (!source) return EXIT_FAILURE;

    // Convert png data to NgImage
    NgImage *ng_image = convert_image(source);
    printf("  %dx%d: %d tiles, %d sprites, %d palettes\n",
        source->width, source->height,
        ng_image->tile_count, ng_image->sprite_count, ng_image->palette_count);
    free_image(source);

    char tiles_file[MAX_FILENAME_LEN];
    char preview_file[MAX_FILENAME_LEN];
    generate_filenames(source_file, output_dir, tiles_file, preview_file);

    // Write tiles data
    printf("  Saving tiles data to %s\n", tiles_file);
    if (write_tiles(tiles_file, ng_image, tile_id) != 0) {
      return EXIT_FAILURE;
    }

    // Copy sprites to ROM data
    for (int j=0; j < ng_image->sprite_count; j++) {
      memcpy(rom_data + tile_id * SPRITE_SIZE, ng_image->sprites[j], SPRITE_SIZE);
      tile_id++;
    }

    // Save preview image
    printf("  Saving preview to %s\n", preview_file);
    if (save_preview(preview_file, ng_image->preview) != 0) {
      return EXIT_FAILURE;
    }
    free_ng_image(ng_image);
    printf("\n");
  }

  // Save combined sprite graphics data to roms directory
  if (rom_dir && strlen(rom_dir) > 0) {
    printf("Saving ROMs:\n");
    if (save_roms(rom_data, rom_dir) != 0) {
      return EXIT_FAILURE;
    }
  }

  free(rom_data);
  return EXIT_SUCCESS;
}
