#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ImageOpts {
  char *output_dir;
  int align;
  bool allow_dupes;
  bool preview;
} ImageOpts;

int process_image(const char *source_file, ImageOpts *opts, uint8_t *tile_rom_data);
int process_list(const char *source_file, ImageOpts *default_opts, uint8_t *tile_rom_data);

#endif // PROCESS_H
