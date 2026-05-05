/*extracting feature vectors from loaded image*/
#include "img_features.h"

#include <stdint.h> /* for int32_t — a 32-bit integer that's the same size everywhere */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*extracting features from image to feature vector*/
void extract_features(const Image *img, FeatureVector *fv) {
  memset(fv, 0, sizeof(*fv));

  size_t flen = strlen(img->filename);
  if (flen >= MAX_FILENAME_LEN)
    flen = MAX_FILENAME_LEN - 1;
  memcpy(fv->filename, img->filename, flen);
  fv->filename[flen] = '\0';

  int W = img->width;
  int H = img->height;
  long total_pixels = (long)W * (long)H;

  if (total_pixels <= 0) {
    return;
  }

  int hist_counts[HIST_TOTAL_BINS];
  memset(hist_counts, 0, sizeof(hist_counts));

  double sum_r = 0.0, sum_g = 0.0, sum_b = 0.0;
  double sumsq_r = 0.0, sumsq_g = 0.0, sumsq_b = 0.0;

  double block_sum[BLOCK_FEATURES];
  long block_count[BLOCK_FEATURES];
  memset(block_sum, 0, sizeof(block_sum));
  memset(block_count, 0, sizeof(block_count));

  for (int y = 0; y < H; y++) {
    int by = (y * BLOCK_GRID) / H;
    if (by >= BLOCK_GRID)
      by = BLOCK_GRID - 1;

    for (int x = 0; x < W; x++) {
      int bx = (x * BLOCK_GRID) / W;
      if (bx >= BLOCK_GRID)
        bx = BLOCK_GRID - 1;

      long idx = ((long)y * W + x) * 3;
      unsigned char r = img->pixels[idx];
      unsigned char g = img->pixels[idx + 1];
      unsigned char b = img->pixels[idx + 2];

      int rb = r >> 5;
      int gb = g >> 5;
      int bb = b >> 5;
      int bin = (rb * HIST_BINS_PER_CHANNEL * HIST_BINS_PER_CHANNEL) +
                (gb * HIST_BINS_PER_CHANNEL) + bb;
      hist_counts[bin]++;

      sum_r += r;
      sumsq_r += (double)r * r;
      sum_g += g;
      sumsq_g += (double)g * g;
      sum_b += b;
      sumsq_b += (double)b * b;

      double intensity = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
      int block_idx = by * BLOCK_GRID + bx;
      block_sum[block_idx] += intensity;
      block_count[block_idx]++;
    }
  }

  for (int i = 0; i < HIST_TOTAL_BINS; i++) {
    fv->histogram[i] = (float)((double)hist_counts[i] / (double)total_pixels);
  }

  fv->mean[0] = (float)(sum_r / total_pixels / 255.0);
  fv->mean[1] = (float)(sum_g / total_pixels / 255.0);
  fv->mean[2] = (float)(sum_b / total_pixels / 255.0);

  double var_r = (sumsq_r / total_pixels) -
                 (sum_r / total_pixels) * (sum_r / total_pixels);
  double var_g = (sumsq_g / total_pixels) -
                 (sum_g / total_pixels) * (sum_g / total_pixels);
  double var_b = (sumsq_b / total_pixels) -
                 (sum_b / total_pixels) * (sum_b / total_pixels);
  fv->variance[0] = (float)(var_r / (255.0 * 255.0));
  fv->variance[1] = (float)(var_g / (255.0 * 255.0));
  fv->variance[2] = (float)(var_b / (255.0 * 255.0));

  for (int i = 0; i < BLOCK_FEATURES; i++) {
    if (block_count[i] > 0) {
      fv->block_means[i] = (float)(block_sum[i] / (double)block_count[i]);
    } else {
      fv->block_means[i] = 0.0f;
    }
  }
}

/*saving feature vectors to binary cache file*/
int save_features_to_cache(const char *cache_path, const FeatureVector *fvs,
                           int count) {
  FILE *fp = fopen(cache_path, "wb");
  if (fp == NULL) {
    return 1;
  }

  int32_t n = (int32_t)count;
  if (fwrite(&n, sizeof(n), 1, fp) != 1) {
    fclose(fp);
    return 2;
  }

  if (count > 0 &&
      fwrite(fvs, sizeof(FeatureVector), count, fp) != (size_t)count) {
    fclose(fp);
    return 3;
  }

  fclose(fp);
  return 0;
}

/*loading feature vectors from binary cache file*/
int load_features_from_cache(const char *cache_path, FeatureVector **fvs_out,
                             int *count_out) {
  FILE *fp = fopen(cache_path, "rb");
  if (fp == NULL) {
    return 1;
  }

  int32_t n = 0;
  if (fread(&n, sizeof(n), 1, fp) != 1 || n < 0) {
    fclose(fp);
    return 2;
  }

  FeatureVector *buf = NULL;
  if (n > 0) {
    buf = (FeatureVector *)malloc(sizeof(FeatureVector) * (size_t)n);
    if (buf == NULL) {
      fclose(fp);
      return 3;
    }
    if (fread(buf, sizeof(FeatureVector), (size_t)n, fp) != (size_t)n) {
      free(buf);
      fclose(fp);
      return 4;
    }
  }

  fclose(fp);

  *fvs_out = buf;
  *count_out = (int)n;
  return 0;
}
