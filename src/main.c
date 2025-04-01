#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "log.h"
#include "output.h"
#include "process.h"

static void print_usage(const char *prog_name) {
  printf("Usage: %s [options] <source.png>...\n", prog_name);
  printf("   or: %s [options] <list.txt>\n", prog_name);
  printf("   Where each line of <list.txt> contains\n");
  printf("   <source.png> [options]\n");
  printf("   These options will be file specific overrides for the defaults passed in program args.\n\n");
  printf("Options:\n");
  printf("  -r, --rom-dir               ROM directory\n");
  printf("  -v, --verbose               Enable verbose output\n");
  printf("  -h, --help                  Display this help message\n");
  printf("can be overridden per file:\n");
  printf("  -o, --output-dir=<dir>      Tiles output directory\n");
  printf("  -a, --align=<size>          Align start tile index to size\n");
  printf("  -d, --allow_dupes           Don't de-dupe tiles. All tiles are included sequentially\n");
  printf("  -p, --preview               Output preview png\n");
}

static const char *get_file_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename) return "";
  return dot + 1;
}

static uint8_t tile_rom_data[ROM_SIZE] = {0};

int main(int argc, char *argv[]) {
  int opt;
  char *rom_dir = "";

  ImageOpts default_opts = {
    .output_dir = "",
    .align = 0,
    .allow_dupes = false,
    .preview = false,
  };

  struct option long_options[] = {
    {"output-dir", required_argument, 0, 'o'},
    {"rom-dir", required_argument, 0, 'r'},
    {"align", required_argument, 0, 'a'},
    {"allow-dupes", no_argument, 0, 'd'},
    {"preview", no_argument, 0, 'p'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "o:r:vhdp", long_options, NULL)) != -1) {
    switch (opt) {
    case 'r':
      rom_dir = optarg;
      break;
    case 'o':
      default_opts.output_dir = optarg;
      break;
    case 'a':
      default_opts.align = atoi(optarg);
      break;
    case 'd':
      default_opts.allow_dupes = true;
      break;
    case 'p':
      default_opts.preview = true;
      break;
    case 'v':
      verbose = true;
      break;
    case 'h':
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    case '?':
      error_log("Invalid option. Use -h for help.\n");
      return EXIT_FAILURE;
    }
  }

  // Ensure we have at least one positional argument
  if (argc <= optind) {
    error_log("Error: Incorrect number of arguments.\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  // Process file list in positonal args
  for (int i = optind; i < argc; i++) {
    const char *source_file = argv[i];
    const char *ext = get_file_extension(source_file);

    if (strcasecmp(ext, "txt") == 0) {
      if (process_list(source_file, &default_opts, tile_rom_data) != 0) {
        return EXIT_FAILURE;
      }
    } else if (strcasecmp(ext, "png") == 0) {
      if (process_image(source_file, &default_opts, tile_rom_data) != 0) {
        return EXIT_FAILURE;
      }
    } else {
      error_log("unsupported extension %s\n", ext);
      return EXIT_FAILURE;
    }
  }

  // Save combined sprite graphics data to roms directory
  if (rom_dir && strlen(rom_dir) > 0) {
    printf("Saving ROMs:\n");
    if (save_roms(tile_rom_data, rom_dir) != 0) {
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
