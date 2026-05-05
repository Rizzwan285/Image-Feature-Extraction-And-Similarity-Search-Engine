/*header for image input/output handling*/
#ifndef IMAGE_IO_H
#define IMAGE_IO_H

/*defining maximum filename length*/
#define MAX_FILENAME_LEN 256

/*defining loaded image structure*/
typedef struct {
  int width;
  int height;
  unsigned char *pixels;
  char filename[MAX_FILENAME_LEN];
} Image;

/*loading ppm image from disk*/
int load_ppm(const char *path, Image *img);

/*freeing image memory*/
void free_image(Image *img);

#endif /* IMAGE_IO_H */
