#include <stdio.h>
#include <stdlib.h>
#include "unity.h"
#include "palette_merging.h"
#include "colors.h"
#include "safe_mem.h"

// Test function for reduce_palettes
void test_reduce_palettes(void) {
    // Create some sample palettes
    Palette *palette1 = create_palette(0);
    Palette *palette2 = create_palette(1);
    Palette *palette3 = create_palette(2);

    // Add some colors to the palettes
    RGBA color1 = {255, 0, 0, 255}; // Red
    RGBA color2 = {0, 255, 0, 255}; // Green
    RGBA color3 = {0, 0, 255, 255}; // Blue
    RGBA color4 = {255, 255, 0, 255}; // Yellow

    add_to_palette(palette1, &color1);
    add_to_palette(palette1, &color2);
    add_to_palette(palette2, &color2);
    add_to_palette(palette2, &color3);
    add_to_palette(palette3, &color3);
    add_to_palette(palette3, &color4);

    // Create an array of palettes
    Palette *palettes[] = {palette1, palette2, palette3};
    int palette_count = 3;
    int merged[3] = {-1, -1, -1};

    // Call the reduce_palettes function
    reduce_palettes(palettes, palette_count, merged);

    // Check the results
    TEST_ASSERT_EQUAL(merged[0], merged[1]);
    TEST_ASSERT_EQUAL(merged[1], merged[2]);

    // Clean up
    free(palette1);
    free(palette2);
    free(palette3);
}