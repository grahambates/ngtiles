// TODO:
// lossy palette reduction if > 256 palettes
// make PNG preview optional?
// Parallelise?

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include "consts.h"
#include "log.h"
#include "colors.h"
#include "image.h"
#include "ng.h"
#include "output.h"
#include "color_reduction.h"
#include "palette_merging.h"
#include "xxhash.h"

// Extracts RGBA pixels of a tile from the image with optional border
// Border is used for palette generation, and reduces visible seams between tiles
static void extract_tile_pixels(const Image *source, int tile_x, int tile_y, RGBA pixels[], int border_size) {
  for (int y = -border_size; y < TILE_SPAN + border_size; y++) {
    for (int x = -border_size; x < TILE_SPAN + border_size; x++) {
      int sx = tile_x * TILE_SPAN + x;
      int sy = tile_y * TILE_SPAN + y;
      int offset = (y + border_size) * (TILE_SPAN+2*border_size) + (x + border_size);

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

// Extracts indices for a tile using image's fixed palette
static void extract_tile_indices(const Image *source, int tile_x, int tile_y, uint8_t indices[]) {
  for (int y = 0; y < TILE_SPAN; y++) {
    for (int x = 0; x < TILE_SPAN; x++) {
      int sx = tile_x * TILE_SPAN + x;
      int sy = tile_y * TILE_SPAN + y;
      int offset = y * TILE_SPAN + x;
      // Ensure within bounds
      if (sx >= 0 && sx < source->width && sy >= 0 && sy < source->height) {
        indices[offset] = source->pixel_indices[sy * source->width + sx];
      } else {
        // Out of bounds
        indices[offset] = 0;
      }
    }
  }
}

static uint8_t tile_rom_data[ROM_SIZE] = {0};
static int tile_hashes[MAX_TILES];
static int tile_count = 1; // first tile should always be blank

// Gets the index of an identical tile, if one exists
static int existing_tile_index(int hash, int tile_count) {
  for (int i = 0; i < tile_count; i++) {
    if (tile_hashes[i] == hash) {
      return i;
    }
  }
  return -1;
}

// Used to track indexes of merged palettes in reduce_palettes,
// and eventually becomes a map of tile index -> palettes index
static int merged_palettes_map[MAX_TILES];
// Maps index in palettes (which contains duplicates) to unique palette indexes in ng_image
static int ng_palette_map[MAX_TILES];
// Palettes for the current image
static Palette *palettes[MAX_TILES];
// Pixels for the extended tile, used for palette generation
static RGBA expanded_pixels[TILE_PX_EXP];
// Pixels for the current tile being extracted
static RGBA tile_pixels[TILE_PX];
// Palette index per pixel in the current tile
static uint8_t indexed_pixels[TILE_PX];

// Convert source image to NgImage structure
NgImage *convert_image(Image *source) {
  // Calculate number of tiles
  int tiles_x = (source->width + TILE_SPAN - 1) / TILE_SPAN;
  int tiles_y = (source->height + TILE_SPAN - 1) / TILE_SPAN;
  verbose_log("Splitting into %dx%d tiles\n", tiles_x, tiles_y);

  Image *preview = create_image(source->width, source->height, false);
  NgImage *ng_image = create_ng_image(tiles_x, tiles_y);
  ng_image->preview = preview;

  if (source->fixed_palette) {
    ng_image->palettes[ng_image->palette_count++] = convert_palette(source->palette);

  } else {
    // Dither RGB images *before* palette generation
    // We don't do this for indexed PNGs, even if we since converted them to RGB
    if (!source->indexed)
      apply_dithering(source);

    // First generate specific palettes for each tile:
    int palette_count = 0;
    for (int tx = 0; tx < tiles_x; tx++) {
      for (int ty = 0; ty < tiles_y; ty++) {
        verbose_log("Processing tile [%d,%d]\n", tx, ty);
        extract_tile_pixels(source, tx, ty, expanded_pixels, BORDER_SIZE);
        palettes[palette_count] = create_palette_from_tile(expanded_pixels, palette_count);
        palette_count++;
      }
    }

    // Next reduce number of palettes:
    for (int i = 0; i < palette_count; i++) {
      merged_palettes_map[i] = -1;
    }
    reduce_palettes(palettes, palette_count, merged_palettes_map);

    // Add unique palettes to ng image and track index mapping
    for (int i = 0; i < palette_count; i++) {
      // Only add palettes which have not been merged
      if (merged_palettes_map[i] == i) {
        ng_palette_map[i] = ng_image->palette_count;
        ng_image->palettes[ng_image->palette_count++] = convert_palette(palettes[i]);
      }
    }
  }

  // now process each tile:
  int tile_index = 0;
  for (int tx = 0; tx < tiles_x; tx++) {
    for (int ty = 0; ty < tiles_y; ty++) {
      Palette *palette;

      if (source->fixed_palette) {
        // Use single fixed palette
        palette = source->palette;
        extract_tile_indices(source, tx, ty, indexed_pixels);
        ng_image->palette_map[tile_index] = 0;
      } else {
        // Extract only the actual tile pixels now
        extract_tile_pixels(source, tx, ty, tile_pixels, 0);
        // Map palette indices to pixels
        int palette_index = merged_palettes_map[tile_index];
        palette = palettes[palette_index];
        index_tile_pixels(tile_pixels, palette, indexed_pixels);

        // Add mapped palette index to map
        ng_image->palette_map[tile_index] = ng_palette_map[palette_index];
      }

      // Add tile data and add indices to map
      // Check unqiueness
      int hash = XXH64(indexed_pixels, TILE_PX, 0);
      int existing_index = existing_tile_index(hash, tile_count);
      if (existing_index >= 0) {
        // Reuse existing tile
        ng_image->tile_map[tile_count] = existing_index;
      } else {
        // Store new unique tile
        uint8_t *tile_ptr = &tile_rom_data[tile_count * TILE_SIZE];
        convert_tile(indexed_pixels, tile_ptr);
        tile_hashes[tile_count] = hash;
        ng_image->tile_map[tile_index] = tile_count;
        tile_count++;
      }

      // Copy indexed pixels to preview
      for (int y = 0; y < TILE_SPAN; y++) {
        for (int x = 0; x < TILE_SPAN; x++) {
          int dst_x = tx * TILE_SPAN + x;
          int dst_y = ty * TILE_SPAN + y;
          if (dst_x < preview->width && dst_y < preview->height) {
            preview->pixels[dst_y * preview->width + dst_x] = palette->entries[indexed_pixels[y * TILE_SPAN + x]];
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
static void generate_filenames(const char *source_file, const char *output_dir, char *tiles_file, char *preview_file) {
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

  // count new tiles added per image for logging
  int prev_tile_count = tile_count;

  // Process file list in positonal args
  for (int i = optind; i < argc; i++) {
    char *source_file = argv[i];
    printf("Processing file %s\n", source_file);
    Image *source = load_image(source_file);
    if (!source) return EXIT_FAILURE;

    // Convert png data to NgImage
    NgImage *ng_image = convert_image(source);
    printf("  %dx%d: %d tiles, %d new sprites, %d palettes\n",
        source->width, source->height,
        ng_image->tile_count, tile_count - prev_tile_count, ng_image->palette_count);
    prev_tile_count = tile_count;
    free_image(source);

    char tiles_file[MAX_FILENAME_LEN];
    char preview_file[MAX_FILENAME_LEN];
    generate_filenames(source_file, output_dir, tiles_file, preview_file);

    // Write tiles data
    printf("  Saving tiles data to %s\n", tiles_file);
    if (save_tiles(tiles_file, ng_image) != 0) {
      return EXIT_FAILURE;
    }

    // Save preview image
    printf("  Saving preview to %s\n", preview_file);
    if (save_image(preview_file, ng_image->preview) != 0) {
      return EXIT_FAILURE;
    }
    free_ng_image(ng_image);
    printf("\n");
  }

  // Save combined sprite graphics data to roms directory
  if (rom_dir && strlen(rom_dir) > 0) {
    printf("Saving ROMs:\n");
    if (save_roms(tile_rom_data, rom_dir) != 0) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
