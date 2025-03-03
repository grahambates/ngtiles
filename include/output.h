#ifndef OUTPUT_H
#define OUTPUT_H

#include "ng.h"

int save_tiles(const char *filename, const NgImage *image);
int save_roms(const uint8_t rom_data[], const char *rom_dir);

#endif // OUTPUT_H
