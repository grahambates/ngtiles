#include <string.h>
#include <errno.h>

#include "output.h"
#include "consts.h"
#include "safe_mem.h"
#include "log.h"

// Swap byte order
static inline uint16_t swap16(uint16_t val) { return (val >> 8) | (val << 8); }

// Write NgImage tiles data (palette + mappings) to disk
int save_tiles(const char *filename, const NgImage *image) {
  // UWORD palette_count;                     Number of palette entries
  // UWORD palette_entries[palette_count*16]; Color values in NG format
  // UWORD tile_width;                        Width of image in tiles
  // UWORD tile_height;                       Height of image in tiles
  // struct {
  //     UWORD tile_index;                    Index of the tile in the ROM data
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
    data[index++] = swap16(image->tile_map[i]);
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

// Helper to write ROM with file pattern
static int write_rom_file(const char *pattern, const char *rom_dir, void *data) {
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
int save_roms(const uint8_t rom_data[], const char *rom_dir) {
  // Split into odd/even ROMs
  uint8_t *char1 = safe_malloc(ROM_SIZE/2);
  uint8_t *char2 = safe_malloc(ROM_SIZE/2);
  int i = 0;
  for (int bindex = 0; bindex < ROM_SIZE; bindex += 2) {
    char1[i] = rom_data[bindex];
    char2[i++] = rom_data[bindex + 1];
  }

  int err =
    write_rom_file("%s/241-c1.c1", rom_dir, char1) ||
    write_rom_file("%s/241-c2.c2", rom_dir, char2) ||
    write_rom_file("%s/241-c3.c3", rom_dir, char1) ||
    write_rom_file("%s/241-c4.c4", rom_dir, char2);

  free(char1);
  free(char2);
  return err;
}
