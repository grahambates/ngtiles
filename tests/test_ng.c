#include <stdio.h>
#include <stdlib.h>
#include "unity.h"
#include "ng.h"
#include "colors.h"
#include "safe_mem.h"

// Test function for create_ng_image
void test_create_ng_image(void) {
    int width = 4;
    int height = 4;
    NgImage *ng_image = create_ng_image(width, height);
    TEST_ASSERT_NOT_NULL(ng_image);
    TEST_ASSERT_EQUAL(width, ng_image->tile_width);
    TEST_ASSERT_EQUAL(height, ng_image->tile_height);
    TEST_ASSERT_EQUAL(width * height, ng_image->tile_count);
    TEST_ASSERT_EQUAL(0, ng_image->sprite_count);
    TEST_ASSERT_EQUAL(0, ng_image->palette_count);
    free_ng_image(ng_image);
}

// Test function for free_ng_image
void test_free_ng_image(void) {
    int width = 4;
    int height = 4;
    NgImage *ng_image = create_ng_image(width, height);
    free_ng_image(ng_image);
    // No assertions needed, just ensure no crash
}

// Test function for convert_sprite
void test_convert_sprite(void) {
    uint8_t indexed_pixels[16 * 16] = {0};
    uint8_t *sprite = convert_sprite(indexed_pixels);
    TEST_ASSERT_NOT_NULL(sprite);
    free(sprite);
}

// Test function for convert_palette
void test_convert_palette(void) {
    Palette *palette = create_palette(0);
    RGBA color = {255, 0, 0, 255}; // Red
    add_to_palette(palette, &color);
    uint16_t *ng_palette = convert_palette(palette);
    TEST_ASSERT_NOT_NULL(ng_palette);
    TEST_ASSERT_EQUAL_HEX16(0xcf00, ng_palette[1]);
    free(ng_palette);
    free(palette);
}