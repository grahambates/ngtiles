#ifndef CONSTS_H
#define CONSTS_H

#define DITHER 1

#define NUM_COLORS 16
#define TILE_SIZE 16
#define BORDER_SIZE 1 // Number of pixels to extend around each tile
#define TILE_SIZE_EXP (TILE_SIZE+2*BORDER_SIZE)
#define NUM_BOXES (NUM_COLORS-1) // Colour 0 is always transparent
#define MAX_PALETTES 256
#define MAX_SPRITES 1024
#define MAX_TILES 1024
#define MAX_FILENAME_LEN 1024
#define ROM_SIZE 0x1000000
#define SPRITE_SIZE (TILE_SIZE*TILE_SIZE/2) // Two px per byte

#endif // CONSTS_H
