/*
 * image_io.c
 * ----------
 * This file handles reading PPM images from disk into memory.
 *
 * PPM (Portable Pixmap, P6 binary variant) is a dead-simple image format.
 * The file starts with a small text header, then the raw RGB bytes follow
 * immediately after. No compression, no metadata — just plain pixels.
 * We chose it because we can read it in C without any external libraries.
 *
 * The Python layer (bridge.py) converts PNG/JPG uploads to PPM using Pillow
 * before handing them to us, so the C code never has to deal with anything
 * more complicated than this format.
 */

/* We need our own Image struct and function declarations */
#include "image_io.h"

/* Standard C headers for file I/O, memory allocation, strings, and
 * character classification (isspace) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * skip_whitespace_and_comments — helper used while parsing the PPM header.
 *
 * PPM headers can have whitespace (spaces, newlines) and comment lines
 * starting with '#' between the fields. This function skips all of that
 * and leaves the file cursor positioned at the next meaningful character.
 *
 * fp — the open file we're reading from
 *
 * Returns 0 if we found a non-whitespace character (put back into the stream),
 * or -1 if we hit the end of file before finding anything.
 */
static int skip_whitespace_and_comments(FILE *fp) {
    int c;
    /* Keep reading characters one at a time until we find something real */
    while ((c = fgetc(fp)) != EOF) {
        if (c == '#') {
            /* This is a comment — skip every character until end of line */
            while ((c = fgetc(fp)) != EOF && c != '\n') {
                /* just discarding comment characters */
            }
        } else if (!isspace(c)) {
            /* Found a real character — push it back so the caller can read it */
            ungetc(c, fp);
            return 0;  /* success */
        }
        /* If it's whitespace we just loop and keep going */
    }
    return -1;  /* hit EOF before finding anything useful */
}

/*
 * load_ppm — opens a PPM (P6) file and reads it into an Image struct.
 *
 * The PPM P6 format looks like this in the file:
 *   P6\n
 *   <width> <height>\n
 *   255\n
 *   <raw RGB bytes ...>
 *
 * path — file path to open
 * img  — Image struct to fill in; we'll malloc() the pixel buffer inside
 *
 * Returns 0 on success. On failure, returns a non-zero code and prints a
 * message to stderr so the user knows what went wrong.
 */
int load_ppm(const char *path, Image *img) {
    /* Sanity check — don't even try if we got NULL pointers */
    if (path == NULL || img == NULL) {
        return 1;
    }

    /* Open the file in binary mode ("rb") — essential for P6 PPM because
     * the pixel data is raw bytes and we can't let the OS mangle line endings */
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        /* fopen returns NULL if the file doesn't exist or we can't read it */
        fprintf(stderr, "load_ppm: cannot open %s\n", path);
        return 2;
    }

    /* Read the first two bytes — they must be "P6" for a binary PPM file.
     * Other PPM variants (P3 = text, P5 = grayscale) aren't supported here. */
    char magic[3] = {0};
    if (fread(magic, 1, 2, fp) != 2 || magic[0] != 'P' || magic[1] != '6') {
        fprintf(stderr, "load_ppm: not a P6 PPM: %s\n", path);
        fclose(fp);
        return 3;
    }

    /* Now read the three header numbers: width, height, and maxval.
     * skip_whitespace_and_comments handles any spaces or '#' comments
     * that might appear between them. */
    int width = 0, height = 0, maxval = 0;
    if (skip_whitespace_and_comments(fp) != 0 ||
        fscanf(fp, "%d", &width) != 1 ||
        skip_whitespace_and_comments(fp) != 0 ||
        fscanf(fp, "%d", &height) != 1 ||
        skip_whitespace_and_comments(fp) != 0 ||
        fscanf(fp, "%d", &maxval) != 1) {
        fprintf(stderr, "load_ppm: bad header in %s\n", path);
        fclose(fp);
        return 4;
    }

    /* The PPM spec says exactly one whitespace character separates the header
     * from the pixel data. We must consume it before reading raw bytes. */
    fgetc(fp);

    /* We only support standard 8-bit-per-channel images (maxval == 255).
     * Unusual maxvals would mean we'd need to scale the values, which we skip. */
    if (width <= 0 || height <= 0 || maxval != 255) {
        fprintf(stderr, "load_ppm: unsupported dimensions/maxval in %s\n", path);
        fclose(fp);
        return 5;
    }

    /* Allocate memory for the pixel data.
     * Every pixel is 3 bytes: one each for red, green, and blue. */
    size_t pixel_count = (size_t)width * (size_t)height * 3;
    unsigned char *pixels = (unsigned char *)malloc(pixel_count);
    if (pixels == NULL) {
        /* malloc returns NULL if the system is out of memory */
        fprintf(stderr, "load_ppm: out of memory for %s\n", path);
        fclose(fp);
        return 6;
    }

    /* Read all the pixel bytes in one shot — fread is efficient for this */
    if (fread(pixels, 1, pixel_count, fp) != pixel_count) {
        /* If we didn't get as many bytes as expected, the file is truncated */
        fprintf(stderr, "load_ppm: truncated pixel data in %s\n", path);
        free(pixels);   /* clean up before returning — no leaks */
        fclose(fp);
        return 7;
    }

    /* Done reading — close the file */
    fclose(fp);

    /* Fill in the Image struct with everything we just loaded */
    img->width = width;
    img->height = height;
    img->pixels = pixels;   /* hand over the malloc'd buffer */

    /* Store just the filename part (no directory path) so output stays clean.
     * strrchr finds the last '/' in the path; everything after it is the name. */
    const char *slash = strrchr(path, '/');
    const char *base = (slash != NULL) ? slash + 1 : path;
    /* strncpy copies at most MAX_FILENAME_LEN-1 characters, then we force a
     * null terminator in the last slot to guarantee the string is terminated */
    strncpy(img->filename, base, MAX_FILENAME_LEN - 1);
    img->filename[MAX_FILENAME_LEN - 1] = '\0';

    return 0;  /* success */
}

/*
 * free_image — releases all the memory held by an Image struct.
 *
 * We only need to free the pixel buffer because that's the only thing
 * we malloc'd. The rest of the struct fields are plain values.
 *
 * After freeing we zero out the fields so that accidental re-use
 * of the struct gives obviously wrong values rather than silent corruption.
 */
void free_image(Image *img) {
    /* Don't crash if someone accidentally passes us NULL */
    if (img == NULL) {
        return;
    }
    if (img->pixels != NULL) {
        free(img->pixels);      /* release the pixel buffer back to the OS */
        img->pixels = NULL;     /* set to NULL so we don't accidentally free it twice */
    }
    /* Zero out dimensions so the struct is clearly "empty" */
    img->width = 0;
    img->height = 0;
    img->filename[0] = '\0';    /* empty string */
}
