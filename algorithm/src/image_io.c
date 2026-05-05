/*handling reading ppm images from disk*/
#include "image_io.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*skipping whitespace and comments during header parsing*/
static int skip_whitespace_and_comments(FILE *fp) {
  int c;
  while ((c = fgetc(fp)) != EOF) {
    if (c == '#') {
      while ((c = fgetc(fp)) != EOF && c != '\n') {
      }
    } else if (!isspace(c)) {
      ungetc(c, fp);
      return 0;
    }
  }
  return -1;
}

/*loading ppm file into image struct*/
int load_ppm(const char *path, Image *img) {
  if (path == NULL || img == NULL) {
    return 1;
  }

  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    fprintf(stderr, "load_ppm: cannot open %s\n", path);
    return 2;
  }

  char magic[3] = {0};
  if (fread(magic, 1, 2, fp) != 2 || magic[0] != 'P' || magic[1] != '6') {
    fprintf(stderr, "load_ppm: not a P6 PPM: %s\n", path);
    fclose(fp);
    return 3;
  }

  int width = 0, height = 0, maxval = 0;
  if (skip_whitespace_and_comments(fp) != 0 || fscanf(fp, "%d", &width) != 1 ||
      skip_whitespace_and_comments(fp) != 0 || fscanf(fp, "%d", &height) != 1 ||
      skip_whitespace_and_comments(fp) != 0 || fscanf(fp, "%d", &maxval) != 1) {
    fprintf(stderr, "load_ppm: bad header in %s\n", path);
    fclose(fp);
    return 4;
  }

  fgetc(fp);

  if (width <= 0 || height <= 0 || maxval != 255) {
    fprintf(stderr, "load_ppm: unsupported dimensions/maxval in %s\n", path);
    fclose(fp);
    return 5;
  }

  size_t pixel_count = (size_t)width * (size_t)height * 3;
  unsigned char *pixels = (unsigned char *)malloc(pixel_count);
  if (pixels == NULL) {
    fprintf(stderr, "load_ppm: out of memory for %s\n", path);
    fclose(fp);
    return 6;
  }

  if (fread(pixels, 1, pixel_count, fp) != pixel_count) {
    fprintf(stderr, "load_ppm: truncated pixel data in %s\n", path);
    free(pixels);
    fclose(fp);
    return 7;
  }

  fclose(fp);

  img->width = width;
  img->height = height;
  img->pixels = pixels;

  const char *slash = strrchr(path, '/');
  const char *base = (slash != NULL) ? slash + 1 : path;
  strncpy(img->filename, base, MAX_FILENAME_LEN - 1);
  img->filename[MAX_FILENAME_LEN - 1] = '\0';

  return 0;
}

/*freeing image memory*/
void free_image(Image *img) {
  if (img == NULL) {
    return;
  }
  if (img->pixels != NULL) {
    free(img->pixels);
    img->pixels = NULL;
  }
  img->width = 0;
  img->height = 0;
  img->filename[0] = '\0';
}
