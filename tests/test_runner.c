#include "unity.h"

// Declare setUp and tearDown functions if they are used in test files
extern void setUp(void);
extern void tearDown(void);

// Declare test functions from test_colors.c
extern void test_create_palette(void);
extern void test_add_to_palette(void);
extern void test_palette_contains(void);
extern void test_shared_colors(void);
extern void test_color_distance(void);
extern void test_find_closest_palette_color(void);
extern void test_quantize_rgb(void);

// Declare test functions from test_palette_merging.c
extern void test_reduce_palettes(void);

// Declare test functions from test_color_reduction.c
extern void test_create_palette_from_tile(void);
extern void test_apply_dithering(void);
extern void test_index_tile_pixels(void);

// Declare test functions from test_ng.c
extern void test_create_ng_image(void);
extern void test_free_ng_image(void);
extern void test_convert_sprite(void);
extern void test_convert_palette(void);

// Setup and teardown functions
void setUp(void) {
// Initialize any resources needed for the tests
}

void tearDown(void) {
// Clean up any resources allocated in setUp
}

int main(void) {
    UNITY_BEGIN();
    // Run tests from test_colors.c
    RUN_TEST(test_create_palette);
    RUN_TEST(test_add_to_palette);
    RUN_TEST(test_palette_contains);
    RUN_TEST(test_shared_colors);
    RUN_TEST(test_color_distance);
    RUN_TEST(test_find_closest_palette_color);
    RUN_TEST(test_quantize_rgb);
    // Run tests from test_palette_merging.c
    RUN_TEST(test_reduce_palettes);
    // Run tests from test_color_reduction.c
    RUN_TEST(test_create_palette_from_tile);
    RUN_TEST(test_apply_dithering);
    RUN_TEST(test_index_tile_pixels);
    // Run tests from test_ng.c
    RUN_TEST(test_create_ng_image);
    RUN_TEST(test_free_ng_image);
    RUN_TEST(test_convert_sprite);
    RUN_TEST(test_convert_palette);
    return UNITY_END();
}