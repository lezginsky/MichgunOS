#include "image.h"

static unsigned int read_be32(const unsigned char *buf) {
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static unsigned short read_le16(const unsigned char *buf) {
    return buf[0] | (buf[1] << 8);
}

static unsigned int read_le32(const unsigned char *buf) {
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

struct raw_image *decode_tga(const unsigned char *buffer, unsigned int size) {
    if (!buffer || size < 18) return 0;

    unsigned char id_length = buffer[0];
    unsigned char image_type = buffer[2];
    unsigned short width = read_le16(&buffer[12]);
    unsigned short height = read_le16(&buffer[14]);
    unsigned char bpp = buffer[16];

    if (image_type != 1 && image_type != 2 && image_type != 10) {
        return 0; 
    }

    unsigned int *decoded_pixels = (unsigned int *)malloc(width * height * sizeof(unsigned int));
    if (!decoded_pixels) return 0;

    if (image_type == 1) {
        unsigned char colormap_type = buffer[1];
        if (colormap_type != 1) {
            free(decoded_pixels);
            return 0;
        }
        unsigned short colormap_length = read_le16(&buffer[5]);
        unsigned char colormap_entry_size = buffer[7];
        unsigned int bytes_per_entry = (colormap_entry_size + 7) / 8;
        
        unsigned int colormap_offset = 18 + id_length;
        unsigned int pixel_data_offset = colormap_offset + colormap_length * bytes_per_entry;
        
        if (pixel_data_offset + width * height > size) {
            free(decoded_pixels);
            return 0;
        }
        
        unsigned int *palette = (unsigned int *)malloc(colormap_length * sizeof(unsigned int));
        if (!palette) {
            free(decoded_pixels);
            return 0;
        }
        
        for (int i = 0; i < colormap_length; i++) {
            unsigned int entry_offset = colormap_offset + i * bytes_per_entry;
            if (colormap_entry_size == 24) {
                unsigned char b = buffer[entry_offset];
                unsigned char g = buffer[entry_offset + 1];
                unsigned char r = buffer[entry_offset + 2];
                palette[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
            } else if (colormap_entry_size == 32) {
                unsigned char b = buffer[entry_offset];
                unsigned char g = buffer[entry_offset + 1];
                unsigned char r = buffer[entry_offset + 2];
                unsigned char a = buffer[entry_offset + 3];
                palette[i] = (a << 24) | (r << 16) | (g << 8) | b;
            } else if (colormap_entry_size == 16) {
                unsigned short val = read_le16(&buffer[entry_offset]);
                unsigned char r = ((val >> 10) & 0x1F) << 3;
                unsigned char g = ((val >> 5) & 0x1F) << 3;
                unsigned char b = (val & 0x1F) << 3;
                palette[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
            } else {
                palette[i] = 0;
            }
        }
        
        unsigned char descriptor = buffer[17];
        int is_top_down = (descriptor & 0x20) != 0;
        
        for (int y = 0; y < height; y++) {
            int actual_y = is_top_down ? y : (height - 1 - y);
            unsigned int row_offset = pixel_data_offset + actual_y * width;
            for (int x = 0; x < width; x++) {
                unsigned char index = buffer[row_offset + x];
                if (index < colormap_length) {
                    decoded_pixels[y * width + x] = palette[index];
                } else {
                    decoded_pixels[y * width + x] = 0;
                }
            }
        }
        free(palette);
    }
    else if (image_type == 2) {
        unsigned int pixel_data_offset = 18 + id_length;
        unsigned int bytes_per_pixel = bpp / 8;
        if (pixel_data_offset + width * height * bytes_per_pixel > size) {
            free(decoded_pixels);
            return 0;
        }
        
        unsigned char descriptor = buffer[17];
        int is_top_down = (descriptor & 0x20) != 0;
        
        for (int y = 0; y < height; y++) {
            int actual_y = is_top_down ? y : (height - 1 - y);
            unsigned int row_offset = pixel_data_offset + actual_y * width * bytes_per_pixel;
            for (int x = 0; x < width; x++) {
                unsigned int pixel_offset = row_offset + x * bytes_per_pixel;
                if (bpp == 32) {
                    unsigned char b = buffer[pixel_offset];
                    unsigned char g = buffer[pixel_offset + 1];
                    unsigned char r = buffer[pixel_offset + 2];
                    unsigned char a = buffer[pixel_offset + 3];
                    decoded_pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
                } else if (bpp == 24) {
                    unsigned char b = buffer[pixel_offset];
                    unsigned char g = buffer[pixel_offset + 1];
                    unsigned char r = buffer[pixel_offset + 2];
                    decoded_pixels[y * width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else {
                    decoded_pixels[y * width + x] = 0;
                }
            }
        }
    }
    else if (image_type == 10) {
        unsigned int pixel_data_offset = 18 + id_length;
        unsigned int bytes_per_pixel = bpp / 8;
        unsigned int ptr = pixel_data_offset;
        
        unsigned int *temp_pixels = (unsigned int *)malloc(width * height * sizeof(unsigned int));
        if (!temp_pixels) {
            free(decoded_pixels);
            return 0;
        }
        
        unsigned int count_pixels = 0;
        while (count_pixels < width * height && ptr < size) {
            unsigned char chunk_header = buffer[ptr++];
            int is_rle = (chunk_header & 0x80) != 0;
            int count = (chunk_header & 0x7F) + 1;
            
            if (is_rle) {
                if (ptr + bytes_per_pixel > size) break;
                unsigned char b = buffer[ptr];
                unsigned char g = buffer[ptr + 1];
                unsigned char r = buffer[ptr + 2];
                unsigned char a = (bytes_per_pixel == 4) ? buffer[ptr + 3] : 0xFF;
                unsigned int color = (a << 24) | (r << 16) | (g << 8) | b;
                ptr += bytes_per_pixel;
                
                for (int c = 0; c < count && count_pixels < width * height; c++) {
                    temp_pixels[count_pixels++] = color;
                }
            } else {
                for (int c = 0; c < count && count_pixels < width * height; c++) {
                    if (ptr + bytes_per_pixel > size) break;
                    unsigned char b = buffer[ptr];
                    unsigned char g = buffer[ptr + 1];
                    unsigned char r = buffer[ptr + 2];
                    unsigned char a = (bytes_per_pixel == 4) ? buffer[ptr + 3] : 0xFF;
                    unsigned int color = (a << 24) | (r << 16) | (g << 8) | b;
                    ptr += bytes_per_pixel;
                    temp_pixels[count_pixels++] = color;
                }
            }
        }
        
        unsigned char descriptor = buffer[17];
        int is_top_down = (descriptor & 0x20) != 0;
        for (int y = 0; y < height; y++) {
            int actual_y = is_top_down ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                decoded_pixels[y * width + x] = temp_pixels[actual_y * width + x];
            }
        }
        free(temp_pixels);
    }

    struct raw_image *img = (struct raw_image *)malloc(sizeof(struct raw_image));
    if (!img) {
        free(decoded_pixels);
        return 0;
    }

    img->width = width;
    img->height = height;
    img->bpp = 32;
    img->data = (unsigned char *)decoded_pixels;

    return img;
}

struct raw_image *decode_bmp(const unsigned char *buffer, unsigned int size) {
    if (!buffer || size < 54) return 0;

    if (buffer[0] != 'B' || buffer[1] != 'M') return 0;

    unsigned int data_offset = read_le32(&buffer[10]);
    unsigned int width = read_le32(&buffer[18]);
    unsigned int height = read_le32(&buffer[22]);
    unsigned short planes = read_le16(&buffer[26]);
    unsigned short bpp = read_le16(&buffer[28]);
    unsigned int compression = read_le32(&buffer[30]);

    if (planes != 1 || (bpp != 24 && bpp != 32) || compression != 0) {
        return 0;
    }

    unsigned int pixel_data_size = width * height * (bpp / 8);
    if (data_offset + pixel_data_size > size) {
        return 0;
    }

    struct raw_image *img = (struct raw_image *)malloc(sizeof(struct raw_image));
    if (!img) return 0;

    img->width = width;
    img->height = height;
    img->bpp = (unsigned char)bpp;
    img->data = (unsigned char *)malloc(pixel_data_size);

    if (!img->data) {
        free(img);
        return 0;
    }

    unsigned int bytes_per_pixel = bpp / 8;
    unsigned int row_size = width * bytes_per_pixel;

    for (unsigned int y = 0; y < height; y++) {
        unsigned int src_row = (height - 1 - y) * row_size;
        unsigned int dest_row = y * row_size;
        for (unsigned int x = 0; x < row_size; x++) {
            img->data[dest_row + x] = buffer[data_offset + src_row + x];
        }
    }

    return img;
}

int parse_png_header(const unsigned char *buffer, unsigned int size, unsigned int *width, unsigned int *height) {
    if (!buffer || size < 24) return 0;

    if (buffer[0] != 0x89 || buffer[1] != 0x50 || buffer[2] != 0x4E || buffer[3] != 0x47 ||
        buffer[4] != 0x0D || buffer[5] != 0x0A || buffer[6] != 0x1A || buffer[7] != 0x0A) {
        return 0;
    }

    if (buffer[12] != 'I' || buffer[13] != 'H' || buffer[14] != 'D' || buffer[15] != 'R') {
        return 0;
    }

    if (width) *width = read_be32(&buffer[16]);
    if (height) *height = read_be32(&buffer[20]);

    return 1;
}

int parse_jpg_header(const unsigned char *buffer, unsigned int size, unsigned int *width, unsigned int *height) {
    if (!buffer || size < 4) return 0;

    if (buffer[0] != 0xFF || buffer[1] != 0xD8) return 0;

    unsigned int i = 2;
    while (i < size - 8) {
        if (buffer[i] != 0xFF) {
            i++;
            continue;
        }

        unsigned char marker = buffer[i + 1];
        if (marker == 0xFF) {
            i++;
            continue;
        }

        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2 || marker == 0xC3) {
            if (i + 8 < size) {
                if (height) *height = (buffer[i + 5] << 8) | buffer[i + 6];
                if (width) *width = (buffer[i + 7] << 8) | buffer[i + 8];
                return 1;
            }
        }

        unsigned int length = (buffer[i + 2] << 8) | buffer[i + 3];
        i += 2 + length;
    }

    return 0;
}
