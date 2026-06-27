#ifndef IMAGE_H
#define IMAGE_H

#include "malloc.h"

struct raw_image {
    unsigned int width;
    unsigned int height;
    unsigned char bpp;
    unsigned char *data;
};

struct raw_image *decode_tga(const unsigned char *buffer, unsigned int size);

struct raw_image *decode_bmp(const unsigned char *buffer, unsigned int size);

int parse_png_header(const unsigned char *buffer, unsigned int size, unsigned int *width, unsigned int *height);

int parse_jpg_header(const unsigned char *buffer, unsigned int size, unsigned int *width, unsigned int *height);

#endif
