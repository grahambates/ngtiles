#include <stdio.h>
#include <stdlib.h>
#include "unity.h"
#include "colors.h"
#include "safe_mem.h"

// Test function for create_palette
void test_create_palette(void) {
    Palette *palette = create_palette(0);
    TEST_ASSERT_NOT_NULL(palette);
    TEST_ASSERT_EQUAL(1, palette->count);
    TEST_ASSERT_EQUAL(0, palette->index);
    free(palette);
}

// Test function for add_to_palette
void test_add_to_palette(void) {
    Palette *palette = create_palette(0);
    RGBA color = {255, 0, 0, 255}; // Red
    int result = add_to_palette(palette, &color);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(2, palette->count);
    TEST_ASSERT_EQUAL_MEMORY(&color, &palette->entries[1], sizeof(RGBA));
    free(palette);
}

// Test function for palette_contains
void test_palette_contains(void) {
    Palette *palette = create_palette(0);
    RGBA color = {255, 0, 0, 255}; // Red
    add_to_palette(palette, &color);
    int contains = palette_contains(palette, &color);
    TEST_ASSERT_TRUE(contains);
    free(palette);
}

// Test function for shared_colors
void test_shared_colors(void) {
    Palette *palette1 = create_palette(0);
    Palette *palette2 = create_palette(1);
    RGBA color1 = {255, 0, 0, 255}; // Red
    RGBA color2 = {0, 255, 0, 255}; // Green
    add_to_palette(palette1, &color1);
    add_to_palette(palette2, &color1);
    add_to_palette(palette2, &color2);
    int shared = shared_colors(palette1, palette2);
    TEST_ASSERT_EQUAL(2, shared); // Includes transparent color
    free(palette1);
    free(palette2);
}

// Test function for color_distance
void test_color_distance(void) {
    RGBA color1 = {255, 0, 0, 255}; // Red
    RGBA color2 = {0, 255, 0, 255}; // Green
    float distance = color_distance(color1, color2);
    TEST_ASSERT_EQUAL_FLOAT(130050.0, distance);
}

// Test function for find_closest_palette_color
void test_find_closest_palette_color(void) {
    Palette *palette = create_palette(0);
    RGBA color1 = {255, 0, 0, 255}; // Red
    RGBA color2 = {0, 255, 0, 255}; // Green
    add_to_palette(palette, &color1);
    add_to_palette(palette, &color2);
    RGBA test_color = {254, 0, 0, 255}; // Almost Red
    int index = find_closest_palette_color(&test_color, palette);
    TEST_ASSERT_EQUAL(1, index);
    free(palette);
}

// Test function for quantize_rgb
void test_quantize_rgb(void) {
    RGBA color = {255, 0, 0, 255}; // Red
    RGBA quantized = quantize_rgb(color);
    TEST_ASSERT_EQUAL(248, quantized.r);
    TEST_ASSERT_EQUAL(0, quantized.g);
    TEST_ASSERT_EQUAL(0, quantized.b);
    TEST_ASSERT_EQUAL(255, quantized.a);
}