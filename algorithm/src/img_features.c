/*
 * img_features.c
 * --------------
 * This file takes a loaded image and turns it into a list of numbers
 * that describe what it looks like — called a "feature vector".
 *
 * Why? Because you can't compare two images directly pixel-by-pixel
 * (that's too slow and too sensitive to tiny differences). Instead we
 * boil each image down to ~534 floats that capture its color distribution,
 * brightness, and spatial layout. Then comparing two images is just
 * comparing two arrays of numbers, which is fast.
 *
 * We also handle saving and loading these feature vectors to a binary
 * cache file so we don't have to re-extract them every time the program runs.
 */

/* Our own header — gives us the Image and FeatureVector types */
#include "img_features.h"

/* Standard headers for file I/O, memory, strings, and fixed-width integers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>     /* for int32_t — a 32-bit integer that's the same size everywhere */

/*
 * extract_features — the core of this file.
 *
 * Loops over every pixel in the image exactly once and computes three
 * kinds of features simultaneously:
 *
 *  1. RGB histogram: counts how many pixels fall into each of 512 color buckets.
 *     We then divide by total pixels to get a probability distribution (sums to 1).
 *
 *  2. Per-channel mean and variance: the average and spread of pixel brightness
 *     for the red, green, and blue channels independently.
 *
 *  3. Block-mean grid: divides the image into a 4×4 spatial grid and records
 *     the average luminance (perceived brightness) in each of the 16 cells.
 *     This captures rough spatial structure — e.g. "bright on top, dark below".
 *
 * img — the loaded image to analyze
 * fv  — the FeatureVector struct we'll write all the computed numbers into
 */
void extract_features(const Image *img, FeatureVector *fv) {
    /* Clear every byte of the FeatureVector to zero before we start.
     * This means if we exit early, the caller gets zeros rather than garbage. */
    memset(fv, 0, sizeof(*fv));

    /* Copy the image's filename into the feature vector so we always know
     * which image this fingerprint belongs to. We use memcpy instead of
     * strncpy to avoid a compiler warning about potential truncation. */
    size_t flen = strlen(img->filename);
    if (flen >= MAX_FILENAME_LEN) flen = MAX_FILENAME_LEN - 1;  /* clamp to buffer size */
    memcpy(fv->filename, img->filename, flen);
    fv->filename[flen] = '\0';  /* always null-terminate the string */

    /* Convenience variables so we don't type img->width everywhere */
    int W = img->width;
    int H = img->height;
    long total_pixels = (long)W * (long)H;  /* cast to long to avoid overflow on large images */

    /* Nothing to do for a zero-size image */
    if (total_pixels <= 0) {
        return;
    }

    /* --- Set up the histogram counter array ---
     * We have 512 buckets (8×8×8). Each pixel votes for exactly one bucket.
     * We start with all counts at zero. */
    int hist_counts[HIST_TOTAL_BINS];
    memset(hist_counts, 0, sizeof(hist_counts));

    /* --- Accumulators for computing mean and variance ---
     * sum_r/g/b:   running total of channel values (used to compute mean)
     * sumsq_r/g/b: running total of channel values squared (used for variance)
     * Using double for precision — float would accumulate rounding errors. */
    double sum_r = 0.0, sum_g = 0.0, sum_b = 0.0;
    double sumsq_r = 0.0, sumsq_g = 0.0, sumsq_b = 0.0;

    /* --- Accumulators for the 4×4 spatial block grid ---
     * block_sum[i]   = total luminance of all pixels in block i
     * block_count[i] = how many pixels fell into block i
     * We need the count separately because edge blocks might have fewer pixels. */
    double block_sum[BLOCK_FEATURES];
    long block_count[BLOCK_FEATURES];
    memset(block_sum, 0, sizeof(block_sum));
    memset(block_count, 0, sizeof(block_count));

    /* --- Main pixel loop — visits every pixel exactly once --- */
    for (int y = 0; y < H; y++) {
        /* Figure out which row of blocks this row of pixels belongs to.
         * Scaling y by BLOCK_GRID and dividing by H maps it into 0..3. */
        int by = (y * BLOCK_GRID) / H;
        if (by >= BLOCK_GRID) by = BLOCK_GRID - 1;  /* clamp just in case of rounding */

        for (int x = 0; x < W; x++) {
            /* Same thing for the column of blocks */
            int bx = (x * BLOCK_GRID) / W;
            if (bx >= BLOCK_GRID) bx = BLOCK_GRID - 1;

            /* Each pixel occupies 3 consecutive bytes in the pixel array:
             * byte 0 = red, byte 1 = green, byte 2 = blue.
             * We use long arithmetic to avoid overflow on wide images. */
            long idx = ((long)y * W + x) * 3;
            unsigned char r = img->pixels[idx];        /* red channel, 0..255 */
            unsigned char g = img->pixels[idx + 1];    /* green channel */
            unsigned char b = img->pixels[idx + 2];    /* blue channel */

            /* --- Histogram contribution ---
             * Take the top 3 bits of each channel (r >> 5 gives 0..7).
             * Combine them into a single bucket index in the 8×8×8 cube. */
            int rb = r >> 5;   /* 0..7 for red */
            int gb = g >> 5;   /* 0..7 for green */
            int bb = b >> 5;   /* 0..7 for blue */
            int bin = (rb * HIST_BINS_PER_CHANNEL * HIST_BINS_PER_CHANNEL)
                    + (gb * HIST_BINS_PER_CHANNEL)
                    + bb;
            hist_counts[bin]++;  /* this pixel votes for that bucket */

            /* --- Running sums for mean and variance ---
             * We'll turn these into actual mean/variance after the loop. */
            sum_r += r;  sumsq_r += (double)r * r;
            sum_g += g;  sumsq_g += (double)g * g;
            sum_b += b;  sumsq_b += (double)b * b;

            /* --- Block grid contribution ---
             * Compute perceived brightness (luminance) using the standard
             * formula: 0.299*R + 0.587*G + 0.114*B. Dividing by 255 keeps
             * the value in [0, 1]. Add it to the correct block's bucket. */
            double intensity = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
            int block_idx = by * BLOCK_GRID + bx;  /* row-major index into the 4×4 grid */
            block_sum[block_idx] += intensity;
            block_count[block_idx]++;
        }
    }

    /* --- Normalize the histogram ---
     * Divide each raw count by the total pixel count so the histogram becomes
     * a probability distribution. Now all 512 values sum to 1.0, which makes
     * it fair to compare images of different sizes. */
    for (int i = 0; i < HIST_TOTAL_BINS; i++) {
        fv->histogram[i] = (float)((double)hist_counts[i] / (double)total_pixels);
    }

    /* --- Compute per-channel means ---
     * mean = sum / count, then divided by 255 to normalize to [0, 1] */
    fv->mean[0] = (float)(sum_r / total_pixels / 255.0);  /* red */
    fv->mean[1] = (float)(sum_g / total_pixels / 255.0);  /* green */
    fv->mean[2] = (float)(sum_b / total_pixels / 255.0);  /* blue */

    /* --- Compute per-channel variances ---
     * Variance formula: E[X²] - (E[X])²  (expectation of square minus square of expectation)
     * We compute it in 0..255 space and then divide by 255² to normalize to [0, 1]. */
    double var_r = (sumsq_r / total_pixels) - (sum_r / total_pixels) * (sum_r / total_pixels);
    double var_g = (sumsq_g / total_pixels) - (sum_g / total_pixels) * (sum_g / total_pixels);
    double var_b = (sumsq_b / total_pixels) - (sum_b / total_pixels) * (sum_b / total_pixels);
    fv->variance[0] = (float)(var_r / (255.0 * 255.0));
    fv->variance[1] = (float)(var_g / (255.0 * 255.0));
    fv->variance[2] = (float)(var_b / (255.0 * 255.0));

    /* --- Compute block mean brightness ---
     * For each of the 16 spatial blocks, divide total luminance by pixel count
     * to get the average brightness of that region. */
    for (int i = 0; i < BLOCK_FEATURES; i++) {
        if (block_count[i] > 0) {
            fv->block_means[i] = (float)(block_sum[i] / (double)block_count[i]);
        } else {
            /* Shouldn't happen with a valid image, but be safe */
            fv->block_means[i] = 0.0f;
        }
    }
}

/*
 * save_features_to_cache — writes an array of FeatureVectors to a binary file.
 *
 * The format is intentionally simple:
 *   [int32_t count] [FeatureVector 0] [FeatureVector 1] ... [FeatureVector n-1]
 *
 * We use fwrite to dump the structs as raw bytes. This is fast and compact.
 * The downside is that the file is platform-specific (different byte order
 * on different CPUs), but for this course project that's fine.
 *
 * cache_path — where to write the file
 * fvs        — the array to save
 * count      — number of elements in the array
 *
 * Returns 0 on success, non-zero if something went wrong with the file.
 */
int save_features_to_cache(const char *cache_path,
                           const FeatureVector *fvs,
                           int count) {
    /* Open for binary write — "wb" creates the file or overwrites it */
    FILE *fp = fopen(cache_path, "wb");
    if (fp == NULL) {
        return 1;  /* couldn't create or open the file */
    }

    /* Write the count as a fixed-size 32-bit integer first.
     * This tells the loader how many structs to expect. */
    int32_t n = (int32_t)count;
    if (fwrite(&n, sizeof(n), 1, fp) != 1) {
        fclose(fp);
        return 2;  /* write failed */
    }

    /* Write all the FeatureVector structs as a blob of raw bytes.
     * fwrite returns the number of elements successfully written. */
    if (count > 0 && fwrite(fvs, sizeof(FeatureVector), count, fp) != (size_t)count) {
        fclose(fp);
        return 3;  /* partial write — file might be full */
    }

    fclose(fp);
    return 0;  /* success */
}

/*
 * load_features_from_cache — reads FeatureVectors back out of a cache file.
 *
 * This is the reverse of save_features_to_cache. We allocate a fresh array,
 * read everything into it, and hand the pointer to the caller.
 *
 * cache_path — path to the binary cache file
 * fvs_out    — we set *fvs_out to the malloc'd array on success
 * count_out  — we set *count_out to the number of entries we read
 *
 * Returns 0 if the cache loaded cleanly. Returns non-zero and leaves
 * fvs_out/count_out untouched if anything goes wrong.
 */
int load_features_from_cache(const char *cache_path,
                             FeatureVector **fvs_out,
                             int *count_out) {
    /* Try to open the file for binary reading */
    FILE *fp = fopen(cache_path, "rb");
    if (fp == NULL) {
        return 1;  /* file doesn't exist yet — that's fine, not an error */
    }

    /* Read the count that was written first by save_features_to_cache */
    int32_t n = 0;
    if (fread(&n, sizeof(n), 1, fp) != 1 || n < 0) {
        /* Either we couldn't read the count, or it's negative (corrupted file) */
        fclose(fp);
        return 2;
    }

    /* Allocate the right amount of memory for n FeatureVectors */
    FeatureVector *buf = NULL;
    if (n > 0) {
        buf = (FeatureVector *)malloc(sizeof(FeatureVector) * (size_t)n);
        if (buf == NULL) {
            fclose(fp);
            return 3;  /* out of memory */
        }
        /* Read all n structs in one call */
        if (fread(buf, sizeof(FeatureVector), (size_t)n, fp) != (size_t)n) {
            /* Didn't get as many structs as expected — file is probably truncated */
            free(buf);
            fclose(fp);
            return 4;
        }
    }

    fclose(fp);

    /* Hand the results back to the caller */
    *fvs_out = buf;
    *count_out = (int)n;
    return 0;  /* success */
}
