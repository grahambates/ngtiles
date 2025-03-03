#include <stdio.h>
#include <stdlib.h>
#include "unity.h"
#include "color_reduction.h"
#include "colors.h"
#include "safe_mem.h"

// Test function for create_palette_from_tile
void test_create_palette_from_tile(void) {
    RGBA tile_pixels[TILE_PX_EXP];
    for (int i = 0; i < TILE_PX_EXP; i++) {
        tile_pixels[i] = (RGBA){i % 256, i % 256, i % 256, 255};
    }
    Palette *palette = create_palette_from_tile(tile_pixels, 0);
    TEST_ASSERT_NOT_NULL(palette);
    TEST_ASSERT_LESS_OR_EQUAL(NUM_COLORS, palette->count);
    free(palette);
}

// Test function for apply_dithering
void test_apply_dithering(void) {
    Image *image = create_image(4, 4, false);
    for (int i = 0; i < 16; i++) {
        image->pixels[i] = (RGBA){i * 16, i * 16, i * 16, 255};
    }
    apply_dithering(image);
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_LESS_OR_EQUAL(255, image->pixels[i].r);
        TEST_ASSERT_LESS_OR_EQUAL(255, image->pixels[i].g);
        TEST_ASSERT_LESS_OR_EQUAL(255, image->pixels[i].b);
    }
    free_image(image);
}

// Test function for index_tile_pixels
void test_index_tile_pixels(void) {
    RGBA tile_pixels[TILE_PX];
    for (int i = 0; i < TILE_PX; i++) {
        tile_pixels[i] = (RGBA){i % 256, i % 256, i % 256, 255};
    }
    Palette *palette = create_palette(0);
    for (int i = 0; i < NUM_COLORS; i++) {
        RGBA color = {i, i, i, 255};
        add_to_palette(palette, &color);
    }
    uint8_t indexed_pixels[TILE_PX];
    index_tile_pixels(tile_pixels, palette, indexed_pixels);
    for (int i = 0; i < TILE_PX; i++) {
        TEST_ASSERT_LESS_THAN(NUM_COLORS, indexed_pixels[i]);
    }
    free(palette);
}