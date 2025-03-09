#include <png.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "colors.h"
#include "image.h"
#include "consts.h"
#include "safe_mem.h"
#include "log.h"

Image *create_image(int width, int height, bool fixed_palette) {
  Image *image = safe_malloc(sizeof(Image));
  image->width = width;
  image->height = height;
  image->fixed_palette = fixed_palette;
  if (fixed_palette) {
    image->pixels = NULL;
    image->pixel_indices = safe_malloc(width * height * sizeof(uint8_t));
  } else {
    image->pixel_indices = NULL;
    image->pixels = safe_malloc(width * height * sizeof(RGBA));
  }
  return image;
}

void free_image(Image *image) {
  if (image->pixels != NULL) {
    free(image->pixels);
  }
  if (image->pixel_indices != NULL) {
    free(image->pixel_indices);
  }
  free(image);
}

// Read png image file
Image *load_image(const char *filename) {
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    error_log("Failed to load image: %s: %s\n", filename, strerror(errno));
    return NULL;
  }

  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(fp);
    return NULL;
  }
  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(fp);
    return NULL;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return NULL;
  }
  png_init_io(png, fp);
  png_read_info(png, info);

  int width = png_get_image_width(png, info);
  int height = png_get_image_height(png, info);
  png_byte color_type = png_get_color_type(png, info);
  png_byte bit_depth = png_get_bit_depth(png, info);

  verbose_log("%dx%d, color_type: %d, bit_depth: %d\n", width, height, color_type, bit_depth);

  bool indexed = false; // Track original indexed mode to affect dither behaviour, even if we convert it to RGB
  bool fixed_palette = false;
  Palette *palette;

  // Handle different colour modes

  if (color_type == PNG_COLOR_TYPE_PALETTE) {
	// For indexed palettes, we can either use the existing fixed palette if it's small enough, or convert to RGB
	// Either way we track the original mode, as this determines dither behaviour
    indexed = true;

    // Get palette data
    png_colorp png_palette;
    int num_palette;
    if (png_get_PLTE(png, info, &png_palette, &num_palette) != PNG_INFO_PLTE) {
      error_log("Failed to get palette data for indexed PNG\n");
      png_destroy_read_struct(&png, &info, NULL);
      fclose(fp);
      return NULL;
    }

    // Check palette size
    if (num_palette <= NUM_COLORS) {
      // Can use fixed palette mode if palette size <= 16
      verbose_log("Using fixed palette with %d colors\n", num_palette);
      fixed_palette = true;
      // Discard transparency data. We assume colour 0 is transparent
      png_set_invalid(png, info, PNG_INFO_tRNS);
      // Get palette colors, assume zero transparent
      palette = create_palette(0);
      for (int i = 1; i < num_palette; i++) {
        RGBA color = quantize_rgb(
          (RGBA){png_palette[i].red, png_palette[i].green, png_palette[i].blue, 0xff}
        );
        add_to_palette(palette, &color);
      }
    } else {
      // Convert to RGB if > 16
      verbose_log("Index palette too big at %d colors, converting to RGB\n", num_palette);
      png_set_palette_to_rgb(png);
    }
  }

  // Convert greyscale to RGBA
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    if (bit_depth < 8)
      png_set_expand_gray_1_2_4_to_8(png);
    png_set_gray_to_rgb(png);
  }

  // Ensure all images have an alpha channel
  if (!(color_type & PNG_COLOR_MASK_ALPHA))
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER); // Add full alpha if absent

  // Convert transparency chunks to alpha
  if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);

  // Ensure bit depth is 8
  if (bit_depth == 16)
    png_set_strip_16(png); // Convert 16-bit to 8-bit

  png_read_update_info(png, info);

  Image *image = create_image(width, height, fixed_palette);
  image->indexed = indexed;

  // Read the pixel data - either RGBA or indexed

  png_bytep *row_pointers = safe_malloc(sizeof(png_bytep) * height);
  for (int y = 0; y < height; y++) {
    row_pointers[y] = safe_malloc(png_get_rowbytes(png, info));
  }
  png_read_image(png, row_pointers);

  // Copy data to our image structure
  if (fixed_palette) {
    image->palette = palette;
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        image->pixel_indices[y * width + x] = row_pointers[y][x];
      }
      free(row_pointers[y]);
    }
  } else {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        png_byte *ptr = &(row_pointers[y][x * 4]);
        image->pixels[y * width + x].r = ptr[0];
        image->pixels[y * width + x].g = ptr[1];
        image->pixels[y * width + x].b = ptr[2];
        image->pixels[y * width + x].a = ptr[3];
      }
      free(row_pointers[y]);
    }
  }

  free(row_pointers);
  png_destroy_read_struct(&png, &info, NULL);
  fclose(fp);
  return image;
}

// Save png file
int save_image(const char *filename, const Image *image) {
  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    error_log("Failed to create image %s: %s\n", filename, strerror(errno));
    return errno;
  }

  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) {
    fclose(fp);
    return 1;
  }
  png_infop info = png_create_info_struct(png);
  if (!info) {
    png_destroy_write_struct(&png, NULL);
    fclose(fp);
    return 1;
  }
  if (setjmp(png_jmpbuf(png))) {
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return 1;
  }
  png_init_io(png, fp);

  // Write header
  png_set_IHDR(png, info, image->width, image->height, 8, PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  // Write as RGB
  png_write_info(png, info);
  png_bytep row = safe_malloc(image->width * 4);
  for (int y = 0; y < image->height; y++) {
    for (int x = 0; x < image->width; x++) {
      RGBA pixel = image->pixels[y * image->width + x];
      row[x * 4] = pixel.r;
      row[x * 4 + 1] = pixel.g;
      row[x * 4 + 2] = pixel.b;
      row[x * 4 + 3] = pixel.a;
    }
    png_write_row(png, row);
  }

  free(row);
  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);
  fclose(fp);
  return 0;
}
