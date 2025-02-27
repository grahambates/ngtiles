#ifndef OUTPUT_H
#define OUTPUT_H

#include "ng.h"

int save_tiles(char *filename, NgImage *image, int offset);
int save_roms(uint8_t rom_data[], char *rom_dir);

#endif
