#ifndef UNICODE_H
#define UNICODE_H

unsigned int utf8_to_codepoint(const unsigned char *str, int *bytes_consumed);

int utf8_to_codepoints(const char *utf8_str, unsigned int *out_codepoints, int max_len);

const unsigned char *get_unicode_font_bitmap(unsigned int codepoint);

#endif
