#ifndef CONSTS_H
#define CONSTS_H

#define DITHER 1

#define NUM_COLORS 16
#define TILE_SPAN 16
#define TILE_PX (TILE_SPAN*TILE_SPAN)
#define BORDER_SIZE 1 // Number of pixels to extend around each tile
#define TILE_SPAN_EXP (TILE_SPAN+2*BORDER_SIZE)
#define TILE_PX_EXP (TILE_SPAN_EXP*TILE_SPAN_EXP)
#define NUM_BOXES (NUM_COLORS-1) // Colour 0 is always transparent
#define ROM_SIZE 0x1000000
#define TILE_SIZE (TILE_PX/2) // Two px per byte
#define MAX_TILES (ROM_SIZE/TILE_SIZE)
#define MAX_PALETTES 256
#define MAX_FILENAME_LEN 1024
#define MAX_LIST_LINE_LENGTH 512
#define MAX_LIST_LINE_TOKENS 128

#endif // CONSTS_H
