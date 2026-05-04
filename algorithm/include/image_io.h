/*
 * image_io.h
 * ----------
 * This is a header file — it tells other parts of the code what's available
 * in image_io.c without them needing to look inside that file.
 *
 * We define one struct (Image) and declare two functions: one to load a
 * PPM image from disk, and one to free the memory it used.
 */

/* Include guard — makes sure this header is never processed twice in one
 * compilation. If it was already included, the compiler just skips everything
 * between here and the #endif at the bottom. */
#ifndef IMAGE_IO_H
#define IMAGE_IO_H

/* Maximum number of characters we'll store for a filename, including the
 * null terminator at the end. 256 is way more than enough for any normal name. */
#define MAX_FILENAME_LEN 256

/*
 * This struct represents one image that has been loaded into memory.
 *
 * width    — how many pixels wide the image is
 * height   — how many pixels tall it is
 * pixels   — a pointer to the raw RGB color data. It's laid out as
 *             R, G, B, R, G, B, ... going left-to-right, top-to-bottom.
 *             The total byte count is width * height * 3.
 * filename — the short name of the file this image came from (no directory path)
 */
typedef struct {
    int width;
    int height;
    unsigned char *pixels;
    char filename[MAX_FILENAME_LEN];
} Image;

/*
 * load_ppm — reads a PPM (P6 binary format) image from disk into memory.
 *
 * path — the full file path to the .ppm file we want to open
 * img  — pointer to an Image struct; we'll fill it in with the loaded data
 *
 * This function uses malloc() to allocate the pixel buffer. When you're done
 * with the image you MUST call free_image() to give that memory back, otherwise
 * you'll leak it and valgrind will complain.
 *
 * Returns 0 on success, non-zero on any kind of failure.
 */
int load_ppm(const char *path, Image *img);

/*
 * free_image — releases the pixel memory that load_ppm() allocated.
 *
 * img — pointer to the Image we want to clean up
 *
 * Safe to call even if the Image was never properly loaded — we check for
 * NULL before doing anything, so it won't crash.
 */
void free_image(Image *img);

/* End of the include guard — everything above is now "seen" by the compiler */
#endif /* IMAGE_IO_H */
