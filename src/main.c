// TODO:
// separate path for indexed mode
// lossy palette reduction
// pre-dither?

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include "consts.h"
#include "log.h"
#include "safe_mem.h"
#include "colors.h"
#include "image.h"
#include "ng.h"
#include "output.h"
#include "color_reduction.h"
#include "palette_merging.h"

int verbose;

// Extracts a tile from the image with a slight border
// This is used for palette generation, and reduces visible borders between tiles
static void extract_tile_pixels(Image *source, int tile_x, int tile_y, RGBA pixels[], int border_size) {
  for (int y = -border_size; y < TILE_SIZE + border_size; y++) {
    for (int x = -border_size; x < TILE_SIZE + border_size; x++) {
      int sx = tile_x * TILE_SIZE + x;
      int sy = tile_y * TILE_SIZE + y;
      int offset = (y + border_size) * (TILE_SIZE+2*border_size) + (x + border_size);

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
      extract_tile_pixels(source, tx, ty, expanded_pixels, BORDER_SIZE);
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
      extract_tile_pixels(source, tx, ty, tile_pixels, 0);
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
    Image *source = load_image(source_file);
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
    if (save_tiles(tiles_file, ng_image, tile_id) != 0) {
      return EXIT_FAILURE;
    }

    // Copy sprites to ROM data
    for (int j=0; j < ng_image->sprite_count; j++) {
      memcpy(rom_data + tile_id * SPRITE_SIZE, ng_image->sprites[j], SPRITE_SIZE);
      tile_id++;
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
    if (save_roms(rom_data, rom_dir) != 0) {
      return EXIT_FAILURE;
    }
  }

  free(rom_data);
  return EXIT_SUCCESS;
}
