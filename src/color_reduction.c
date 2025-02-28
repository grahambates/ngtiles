#include "color_reduction.h"
#include "colors.h"
#include "safe_mem.h"
#include "log.h"

// Pixel grouping for median cut
typedef struct Box {
  RGBA *pixels;
  int count;
  int capacity;
  uint8_t rmin, rmax;
  uint8_t gmin, gmax;
  uint8_t bmin, bmax;
} Box;

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
Palette *create_palette_from_tile(RGBA tile_pixels[], int index) {
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

// Clamp function for color values (0-255)
static inline uint8_t clamp(int value) {
  return (value < 0) ? 0 : (value > 255) ? 255 : (uint8_t)value;
}

// Apply error diffusion to image
void apply_dithering(Image *source) {
  // 1/16 | - # 7 |
  //      | 1 3 5 |
  const float right_weight = 7.0f/16.0f;
  const float bottom_left_weight = 1.0f/16.0f;
  const float bottom_weight = 3.0f/16.0f;
  const float bottom_right_weight = 5.0f/16.0f;

  for (int y = 0; y < source->height; y++) {
    for (int x = 0; x < source->width; x++) {
      int offset = y * source->width + x;
      RGBA pixel = source->pixels[offset];
      RGBA quantized = quantize_rgb(pixel);

      int error_r = pixel.r - quantized.r;
      int error_g = pixel.g - quantized.g;
      int error_b = pixel.b - quantized.b;

      source->pixels[offset] = quantized;

      // Distribute error to right pixel
      if (x + 1 < source->width) {
        RGBA *right = &source->pixels[offset + 1];
        right->r = clamp(right->r + (int)(error_r * right_weight));
        right->g = clamp(right->g + (int)(error_g * right_weight));
        right->b = clamp(right->b + (int)(error_b * right_weight));
      }

      if (y + 1 < source->height) {
        // Distribute error to bottom-left pixel
        if (x > 0) {
          RGBA *bottom_left = &source->pixels[offset + source->width - 1];
          bottom_left->r = clamp(bottom_left->r + (int)(error_r * bottom_left_weight));
          bottom_left->g = clamp(bottom_left->g + (int)(error_g * bottom_left_weight));
          bottom_left->b = clamp(bottom_left->b + (int)(error_b * bottom_left_weight));
        }

        // Distribute error to bottom pixel
        RGBA *bottom = &source->pixels[offset + source->width];
        bottom->r = clamp(bottom->r + (int)(error_r * bottom_weight));
        bottom->g = clamp(bottom->g + (int)(error_g * bottom_weight));
        bottom->b = clamp(bottom->b + (int)(error_b * bottom_weight));

        // Distribute error to bottom-right pixel
        if (x + 1 < source->width) {
          RGBA *bottom_right = &source->pixels[offset + source->width + 1];
          bottom_right->r = clamp(bottom_right->r + (int)(error_r * bottom_right_weight));
          bottom_right->g = clamp(bottom_right->g + (int)(error_g * bottom_right_weight));
          bottom_right->b = clamp(bottom_right->b + (int)(error_b * bottom_right_weight));
        }
      }
    }
  }
}

// Get closest palette index for each pixel in image
void index_tile_pixels(RGBA pixels[], Palette *palette, uint8_t indexed_pixels[]) {
  for (int y = 0; y < TILE_SIZE; y++) {
    for (int x = 0; x < TILE_SIZE; x++) {
      RGBA *pixel = &pixels[y * TILE_SIZE + x];

      // Fixed index for transparent pixels
      if (pixel->a == 0) {
        indexed_pixels[y * TILE_SIZE + x] = 0;
        continue;
      }

      // Find closest palette color
      indexed_pixels[y * TILE_SIZE + x] = find_closest_palette_color(pixel, palette);
    }
  }
}
