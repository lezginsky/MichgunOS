struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int num;
    unsigned int size;
    unsigned int addr;
    unsigned int shndx;
    unsigned int mmap_length;
    unsigned int mmap_addr;
    unsigned int drives_length;
    unsigned int drives_addr;
    unsigned int config_table;
    unsigned int boot_loader_name;
    unsigned int apm_table;
    unsigned int vbe_control_info;
    unsigned int vbe_mode_info;
    unsigned short vbe_mode;
    unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off;
    unsigned short vbe_interface_len;
    unsigned int framebuffer_addr_low;
    unsigned int framebuffer_addr_high;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
} __attribute__((packed));

#include "malloc.h"
#include "shell.h"
#include "unicode.h"
#include "image.h"
#include "terminal_tga.h"

void draw_pixel(int x, int y, unsigned int color);

struct raw_image *terminal_icon = 0;
struct raw_image *files_icon = 0;

volatile unsigned int timer_ticks = 0;
void timer_callback() {
    timer_ticks++;
}

struct raw_image *create_files_icon() {
    struct raw_image *img = (struct raw_image *)malloc(sizeof(struct raw_image));
    img->width = 16;
    img->height = 16;
    img->bpp = 32;
    unsigned int *pixels = (unsigned int *)malloc(16 * 16 * sizeof(unsigned int));
    img->data = (unsigned char *)pixels;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            unsigned int color = 0x00000000;
            if (y >= 3 && y <= 13 && x >= 1 && x <= 14) {
                int is_border = (y == 3 && x >= 1 && x <= 6) ||
                                (y == 4 && x >= 7 && x <= 14) ||
                                (y == 3 && x == 6) ||
                                (y == 13) ||
                                (x == 1) ||
                                (x == 14);
                if (y == 2 && x >= 2 && x <= 6) {
                    color = 0xFFD89000;
                } else if (y == 3 && x >= 2 && x <= 5) {
                    color = 0xFFFFC83B;
                } else if (is_border) {
                    color = 0xFFD89000;
                } else {
                    color = 0xFFFFC83B;
                }
            }
            pixels[y * 16 + x] = color;
        }
    }
    return img;
}

void draw_image(int x, int y, struct raw_image *img) {
    if (!img || !img->data) return;
    unsigned int *pixels = (unsigned int *)img->data;
    for (unsigned int i = 0; i < img->height; i++) {
        for (unsigned int j = 0; j < img->width; j++) {
            unsigned int color = pixels[i * img->width + j];
            unsigned char alpha = (color >> 24) & 0xFF;
            if (alpha == 0) continue;
            draw_pixel(x + j, y + i, color & 0x00FFFFFF);
        }
    }
}

#define figma_bg       0x000d0e11
#define figma_window   0x00000000
#define figma_topbar   0x00000000
#define figma_line     0x00222222
#define figma_taskbar  0x00111216
#define white_color    0x00ffffff
#define gray_color     0x007f8c8d
#define dark_gray      0x001e1f26
#define red_button     0x00e74c3c

extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

const char keymap_en[58] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

const unsigned short keymap_ru[58] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 0x0439, 0x0446, 0x0443, 0x043a, 0x0435, 0x043d, 0x0433, 0x0448, 0x0449, 0x0437, 0x0445, 0x044a, '\n',
    0, 0x0444, 0x044b, 0x0432, 0x0430, 0x043f, 0x0440, 0x043e, 0x043b, 0x0434, 0x0436, 0x044d, 0x0435,
    0, '\\', 0x044f, 0x0447, 0x0441, 0x043c, 0x0438, 0x0442, 0x044c, 0x0431, 0x044e, '.', 0,
    '*', 0, ' '
};

volatile int win_pressed = 0;
volatile int current_layout = 0; // 0 for en, 1 for ru

extern void init_shell(void);
extern void process_command(const char *cmd_line);
extern const char *get_last_output(void);
extern const char *get_current_dir(void);

unsigned int *framebuffer = (unsigned int *)0xfd000000;
int screen_width = 1024;
int screen_height = 768;

unsigned int virtual_buffer[1024 * 768];

volatile int ctrl_pressed = 0;
volatile int alt_pressed = 0;

volatile int mouse_x = 512;
volatile int mouse_y = 384;
volatile int old_mouse_x = 512;
volatile int old_mouse_y = 384;
volatile unsigned char mouse_cycle = 0;
volatile unsigned char mouse_packet[3]; 

volatile int is_terminal_open = 0;
volatile int is_terminal_maximized = 0;
volatile int is_terminal_minimized = 0;
volatile int is_dragging = 0;

int drag_offset_x = 0;
int drag_offset_y = 0;
int saved_term_x = 200;
int saved_term_y = 150;
int saved_term_w = 600;
int saved_term_h = 400;

int term_x = 200;
int term_y = 150;
int term_w = 600;
int term_h = 400;

char term_buffer[256]; 
volatile int term_index = 0;

volatile int is_files_open = 0;
volatile int is_files_maximized = 0;
volatile int is_files_minimized = 0;
int files_x = 240;
int files_y = 100;
int files_w = 480;
int files_h = 360;
int saved_files_x = 240;
int saved_files_y = 100;
int saved_files_w = 480;
int saved_files_h = 360;

char opened_file_name[32] = "";
char opened_file_content[128] = "";

char files_history[10][64] = { "/shadow/user" };
int files_history_ptr = 0;
int files_history_max = 1;

volatile int is_editor_open = 0;
volatile int is_editor_maximized = 0;
volatile int is_editor_minimized = 0;
int editor_x = 280;
int editor_y = 120;
int editor_w = 480;
int editor_h = 360;
int saved_editor_x = 280;
int saved_editor_y = 120;
int saved_editor_w = 480;
int saved_editor_h = 360;

char editor_file_name[64] = "";
char editor_file_content[1024] = "";
int editor_cursor_index = 0;
int editor_unsaved_changes = 0;
struct raw_image *editor_icon = 0;

volatile int active_focus = 0;

int strcmp(const char *s1, const char *s2);
void strcpy(char *dest, const char *src);

void append_codepoint_to_term(unsigned int cp) {
    if (cp < 0x80) {
        if (term_index < 110) {
            term_buffer[term_index++] = (char)cp;
        }
    } else if (cp <= 0x7FF) {
        if (term_index < 109) {
            term_buffer[term_index++] = (char)(0xC0 | ((cp >> 6) & 0x1F));
            term_buffer[term_index++] = (char)(0x80 | (cp & 0x3F));
        }
    }
}

void append_codepoint_to_editor(unsigned int cp) {
    int len = 0;
    while (editor_file_content[len]) len++;
    if (cp < 0x80) {
        if (len < 1022) {
            editor_file_content[len++] = (char)cp;
            editor_file_content[len] = '\0';
            editor_unsaved_changes = 1;
        }
    } else if (cp <= 0x7FF) {
        if (len < 1021) {
            editor_file_content[len++] = (char)(0xC0 | ((cp >> 6) & 0x1F));
            editor_file_content[len++] = (char)(0x80 | (cp & 0x3F));
            editor_file_content[len] = '\0';
            editor_unsaved_changes = 1;
        }
    }
}

volatile int cmd_output_active = 0;
const char *last_command_output = "";
const char *current_path = "/shadow/user";
const unsigned char font[128][8] = {
    [0x20] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 
    [0x2f] = {0x00,0x10,0x10,0x08,0x04,0x04,0x02,0x00}, 
    [0x30] = {0x18,0x24,0x24,0x24,0x24,0x24,0x18,0x00}, 
    [0x31] = {0x08,0x18,0x08,0x08,0x08,0x08,0x1c,0x00}, 
    [0x32] = {0x3c,0x04,0x04,0x18,0x20,0x40,0x7e,0x00}, 
    [0x33] = {0x3c,0x04,0x04,0x1c,0x04,0x04,0x3c,0x00}, 
    [0x34] = {0x24,0x24,0x24,0x3e,0x04,0x04,0x04,0x00}, 
    [0x35] = {0x7e,0x40,0x7c,0x04,0x04,0x04,0x38,0x00}, 
    [0x36] = {0x38,0x40,0x7c,0x44,0x44,0x44,0x38,0x00}, 
    [0x37] = {0x7e,0x04,0x08,0x10,0x20,0x20,0x20,0x00}, 
    [0x38] = {0x38,0x44,0x44,0x38,0x44,0x44,0x38,0x00}, 
    [0x39] = {0x38,0x44,0x44,0x3e,0x04,0x04,0x38,0x00}, 
    [0x3a] = {0x00,0x10,0x10,0x00,0x10,0x10,0x00,0x00}, 
    [0x2e] = {0x00,0x00,0x00,0x00,0x00,0x10,0x10,0x00}, 
    [0x3c] = {0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00}, 
    [0x46] = {0xfc,0x80,0x80,0xf8,0x80,0x80,0x80,0x00}, 
    [0x5f] = {0x00,0x00,0x00,0x00,0x00,0x00,0x7e,0x00}, 
    [0x5b] = {0x1c,0x10,0x10,0x10,0x10,0x10,0x1c,0x00}, 
    [0x5d] = {0x38,0x08,0x08,0x08,0x08,0x08,0x38,0x00}, 
    [0x3e] = {0x40,0x20,0x10,0x08,0x10,0x20,0x40,0x00}, 
    [0x2d] = {0x00,0x00,0x00,0x3c,0x00,0x00,0x00,0x00},
    [0x54] = {0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, 
    [0x61] = {0x00,0x00,0x38,0x04,0x3c,0x44,0x3c,0x00}, 
    [0x62] = {0x40,0x40,0x58,0x64,0x64,0x64,0x58,0x00}, 
    [0x63] = {0x00,0x00,0x38,0x44,0x40,0x44,0x38,0x00}, 
    [0x64] = {0x04,0x04,0x38,0x44,0x44,0x44,0x3c,0x00}, 
    [0x65] = {0x00,0x00,0x38,0x44,0x7c,0x40,0x38,0x00}, 
    [0x66] = {0x18,0x24,0x78,0x20,0x20,0x20,0x70,0x00}, 
    [0x67] = {0x00,0x00,0x3c,0x44,0x44,0x3c,0x04,0x38}, 
    [0x68] = {0x40,0x40,0x58,0x64,0x64,0x64,0x64,0x00}, 
    [0x69] = {0x10,0x00,0x30,0x10,0x10,0x10,0x38,0x00}, 
    [0x6a] = {0x08,0x00,0x18,0x08,0x08,0x08,0x48,0x30}, 
    [0x6b] = {0x40,0x44,0x48,0x70,0x48,0x44,0x44,0x00}, 
    [0x6c] = {0x30,0x10,0x10,0x10,0x10,0x10,0x38,0x00}, 
    [0x6d] = {0x00,0x00,0x54,0x6c,0x6c,0x44,0x44,0x00}, 
    [0x6e] = {0x00,0x00,0x58,0x64,0x64,0x64,0x64,0x00}, 
    [0x6f] = {0x00,0x00,0x38,0x44,0x44,0x44,0x38,0x00}, 
    [0x70] = {0x00,0x00,0x58,0x64,0x64,0x58,0x40,0x40}, 
    [0x71] = {0x00,0x00,0x3c,0x44,0x44,0x3c,0x04,0x04}, 
    [0x72] = {0x00,0x00,0x2c,0x32,0x20,0x20,0x20,0x00}, 
    [0x73] = {0x00,0x00,0x3c,0x40,0x38,0x04,0x78,0x00}, 
    [0x74] = {0x20,0x20,0x70,0x20,0x20,0x24,0x18,0x00}, 
    [0x75] = {0x00,0x00,0x44,0x44,0x44,0x44,0x38,0x00}, 
    [0x76] = {0x00,0x00,0x44,0x44,0x28,0x28,0x10,0x00}, 
    [0x77] = {0x00,0x00,0x44,0x44,0x54,0x54,0x28,0x00}, 
    [0x78] = {0x00,0x00,0x44,0x28,0x10,0x28,0x44,0x00}, 
    [0x79] = {0x00,0x00,0x44,0x44,0x44,0x3c,0x04,0x38}, 
    [0x7a] = {0x00,0x00,0x7c,0x08,0x10,0x20,0x7c,0x00}  
};

const unsigned char mouse_arrow[16] = {
    0b10000000, 0b11000000, 0b11100000, 0b11110000,
    0b11111000, 0b11111100, 0b11111110, 0b11111111,
    0b11111111, 0b11111000, 0b11011000, 0b10001100,
    0b00001100, 0b00000110, 0b00000110, 0b00000000
};

void outw(unsigned short port, unsigned short val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

void draw_pixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
        virtual_buffer[y * screen_width + x] = color;
    }
}

void swap_buffers_dirty(int x1, int y1, int x2, int y2) {
    int start_x = (x1 < x2) ? x1 : x2;
    int end_x = (x1 > x2) ? x1 : x2;
    int start_y = (y1 < y2) ? y1 : y2;
    int end_y = (y1 > y2) ? y1 : y2;
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;
    if (end_x >= screen_width) end_x = screen_width - 1;
    if (end_y >= screen_height) end_y = screen_height - 1;
    for (int y = start_y; y <= end_y; y++) {
        int offset = y * screen_width;
        for (int x = start_x; x <= end_x; x++) {
            framebuffer[offset + x] = virtual_buffer[offset + x];
        }
    }
}

void swap_full_buffer() {
    for (int i = 0; i < screen_width * screen_height; i++) {
        framebuffer[i] = virtual_buffer[i];
    }
}
void draw_rect(int x, int y, int w, int h, unsigned int color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void draw_char_raw(int x, int y, char c, unsigned int color) {
    for (int i = 0; i < 8; i++) {
        unsigned char row = font[(unsigned char)c][i];
        for (int j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                draw_pixel(x + j, y + i, color);
            }
        }
    }
}

void draw_string_raw(int x, int y, const char *str, unsigned int color) {
    while (*str) {
        draw_char_raw(x, y, *str++, color);
        x += 8; 
    }
}

unsigned char read_cmos_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

unsigned char bcd_to_binary(unsigned char bcd) {
    return ((bcd / 16) * 10) + (bcd % 16);
}

void int_to_string(int num, char *str) {
    int val = num;
    str[0] = (val / 10) + '0';
    str[1] = (val % 10) + '0';
    str[2] = '\0';
}

void draw_char_utf8(int x, int y, unsigned int codepoint, unsigned int color) {
    const unsigned char *bitmap = get_unicode_font_bitmap(codepoint);
    if (bitmap) {
        for (int i = 0; i < 8; i++) {
            unsigned char row = bitmap[i];
            for (int j = 0; j < 8; j++) {
                if (row & (0x80 >> j)) {
                    draw_pixel(x + j, y + i, color);
                }
            }
        }
    } else if (codepoint < 128) {
        draw_char_raw(x, y, (char)codepoint, color);
    }
}

void draw_string_utf8(int x, int y, const char *str, unsigned int color) {
    const unsigned char *ptr = (const unsigned char *)str;
    while (*ptr) {
        int consumed = 0;
        unsigned int cp = utf8_to_codepoint(ptr, &consumed);
        if (consumed <= 0) break;
        draw_char_utf8(x, y, cp, color);
        x += 8;
        ptr += consumed;
    }
}

void update_system_tray() {
    static unsigned int last_tick_read = 0xFFFFFFFF;
    static unsigned char hour = 0;
    static unsigned char min = 0;
    if (last_tick_read == 0xFFFFFFFF || (timer_ticks - last_tick_read) >= 100) {
        hour = bcd_to_binary(read_cmos_register(4));
        min = bcd_to_binary(read_cmos_register(2));
        last_tick_read = timer_ticks;
    }
    char time_str[3], min_str[3];
    int_to_string(hour, time_str);
    int_to_string(min, min_str);
    draw_rect(screen_width - 150, screen_height - 32, 140, 32, figma_taskbar);
    if (current_layout == 1) {
        draw_string_raw(screen_width - 130, screen_height - 20, "[ru]", gray_color);
    } else {
        draw_string_raw(screen_width - 130, screen_height - 20, "[en]", gray_color);
    }
    draw_string_raw(screen_width - 80, screen_height - 20, time_str, white_color);
    draw_string_raw(screen_width - 62, screen_height - 20, ":", white_color);
    draw_string_raw(screen_width - 54, screen_height - 20, min_str, white_color);
}

void draw_terminal_text() {
    draw_rect(term_x + 4, term_y + 44, term_w - 8, term_h - 48, figma_window);
    const char *path = get_current_dir();
    int path_len = 0;
    while (path[path_len]) path_len++;
    draw_string_utf8(term_x + 20, term_y + 60, path, white_color);
    draw_string_utf8(term_x + 20 + path_len * 8, term_y + 60, " > ", white_color);
    int start_x = term_x + 20 + (path_len + 3) * 8;
    int start_y = term_y + 60;
    
    const unsigned char *ptr = (const unsigned char *)term_buffer;
    int col = 0;
    int row = 0;
    while (*ptr && (ptr - (const unsigned char *)term_buffer) < term_index) {
        int consumed = 0;
        unsigned int cp = utf8_to_codepoint(ptr, &consumed);
        if (consumed <= 0) break;
        
        int px = start_x + col * 8;
        int py = start_y + row * 20;
        
        if (px >= term_x + term_w - 16) {
            col = 0;
            row++;
            px = start_x + col * 8;
            py = start_y + row * 20;
        }
        
        draw_char_utf8(px, py, cp, white_color);
        col++;
        ptr += consumed;
    }
    
    if (cmd_output_active) {
        draw_string_utf8(term_x + 20, term_y + 110, get_last_output(), gray_color);
    }
}

void draw_mouse_cursor(int x, int y) {
    for (int i = 0; i < 16; i++) {
        unsigned char row = mouse_arrow[i];
        for (int j = 0; j < 8; j++) {
            if (row & (0x80 >> j)) {
                draw_pixel(x + j, y + i, white_color);
            }
        }
    }
}

void draw_files_window() {
    if (!is_files_open || is_files_minimized) return;
    draw_rect(files_x, files_y, files_w, files_h, figma_window);
    draw_rect(files_x, files_y, files_w, 40, figma_topbar);
    draw_string_raw(files_x + 40, files_y + 16, "Files", white_color);
    if (files_icon) {
        draw_image(files_x + 15, files_y + 12, files_icon);
    } else {
        draw_rect(files_x + 15, files_y + 14, 12, 12, white_color);
    }
    draw_rect(files_x, files_y + 40, files_w, 1, figma_line);
    draw_char_raw(files_x + files_w - 64, files_y + 16, '_', white_color);
    draw_rect(files_x + files_w - 46, files_y + 14, 10, 10, figma_window);
    draw_rect(files_x + files_w - 45, files_y + 15, 8, 8, white_color);
    draw_char_raw(files_x + files_w - 24, files_y + 16, 'x', white_color);

    draw_rect(files_x, files_y + 41, files_w, 31, dark_gray);
    if (files_history_ptr > 0) {
        draw_string_raw(files_x + 15, files_y + 50, "<-", white_color);
    } else {
        draw_string_raw(files_x + 15, files_y + 50, "<-", gray_color);
    }
    if (files_history_ptr < files_history_max - 1) {
        draw_string_raw(files_x + 45, files_y + 50, "->", white_color);
    } else {
        draw_string_raw(files_x + 45, files_y + 50, "->", gray_color);
    }
    draw_string_raw(files_x + 80, files_y + 50, files_history[files_history_ptr], white_color);
    draw_rect(files_x, files_y + 72, files_w, 1, figma_line);

    struct v_file *curr = get_root_files();
    int i = 0;
    char display_name[64];
    while (curr) {
        if (get_file_in_dir(curr->name, files_history[files_history_ptr], display_name)) {
            int item_y = files_y + 85 + i * 24;
            draw_string_raw(files_x + 20, item_y, "[ ", gray_color);
            char num_str[4];
            int_to_string(i + 1, num_str);
            draw_string_raw(files_x + 36, item_y, num_str, white_color);
            draw_string_raw(files_x + 52, item_y, " ]", gray_color);
            
            if (curr->is_dir) {
                char dir_disp[68];
                dir_disp[0] = '[';
                int k = 0;
                while (display_name[k]) { dir_disp[1 + k] = display_name[k]; k++; }
                dir_disp[1 + k] = ']';
                dir_disp[2 + k] = '\0';
                draw_string_raw(files_x + 76, item_y, dir_disp, 0x003498db);
            } else {
                draw_string_raw(files_x + 76, item_y, display_name, white_color);
            }
            i++;
        }
        curr = curr->next;
    }
}

void draw_editor_text() {
    draw_rect(editor_x + 4, editor_y + 44, editor_w - 8, editor_h - 48, figma_window);
    
    int start_x = editor_x + 20;
    int start_y = editor_y + 60;
    
    const unsigned char *ptr = (const unsigned char *)editor_file_content;
    int col = 0;
    int row = 0;
    
    int i = 0;
    while (ptr[i]) {
        int consumed = 0;
        unsigned int cp = utf8_to_codepoint(ptr + i, &consumed);
        if (consumed <= 0) break;
        
        if (cp == '\n') {
            col = 0;
            row++;
            i += consumed;
            continue;
        }
        
        int px = start_x + col * 8;
        int py = start_y + row * 16;
        
        if (px >= editor_x + editor_w - 24) {
            col = 0;
            row++;
            px = start_x + col * 8;
            py = start_y + row * 16;
        }
        
        draw_char_utf8(px, py, cp, white_color);
        col++;
        i += consumed;
    }
    
    int cursor_px = start_x + col * 8;
    int cursor_py = start_y + row * 16;
    if (cursor_px >= editor_x + editor_w - 24) {
        cursor_px = start_x;
        cursor_py += 16;
    }
    
    draw_rect(cursor_px, cursor_py, 8, 14, 0x0033ff33);
}

void draw_editor_window() {
    if (!is_editor_open || is_editor_minimized) return;
    draw_rect(editor_x, editor_y, editor_w, editor_h, figma_window);
    draw_rect(editor_x, editor_y, editor_w, 40, figma_topbar);
    
    char title[128] = "Text Redactor - ";
    int len = 16;
    for (int k = 0; editor_file_name[k] && len < 100; k++) {
        title[len++] = editor_file_name[k];
    }
    if (editor_unsaved_changes) {
        title[len++] = '*';
    }
    title[len] = '\0';
    
    draw_string_raw(editor_x + 40, editor_y + 16, title, white_color);
    
    int icon_x = editor_x + 15;
    int icon_y = editor_y + 12;
    draw_rect(icon_x, icon_y, 12, 14, 0x003498db);
    draw_rect(icon_x + 2, icon_y + 1, 10, 12, white_color);
    draw_rect(icon_x + 4, icon_y + 3, 6, 1, gray_color);
    draw_rect(icon_x + 4, icon_y + 6, 6, 1, gray_color);
    draw_rect(icon_x + 4, icon_y + 9, 6, 1, gray_color);
    
    draw_rect(editor_x, editor_y + 40, editor_w, 1, figma_line);
    
    draw_char_raw(editor_x + editor_w - 64, editor_y + 16, '_', white_color);
    draw_rect(editor_x + editor_w - 46, editor_y + 14, 10, 10, figma_window);
    draw_rect(editor_x + editor_w - 45, editor_y + 15, 8, 8, white_color);
    draw_char_raw(editor_x + editor_w - 24, editor_y + 16, 'x', white_color);
    
    draw_editor_text();
}

static unsigned int terminal_preview[134 * 70];
static unsigned int files_preview[134 * 70];
static unsigned int editor_preview[134 * 70];

void capture_preview_to_buffer(int src_x, int src_y, int src_w, int src_h, unsigned int *dst_buf, int dst_w, int dst_h) {
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    for (int dy = 0; dy < dst_h; dy++) {
        int sy = src_y + (dy * src_h) / dst_h;
        if (sy < 0 || sy >= screen_height) continue;
        for (int dx = 0; dx < dst_w; dx++) {
            int sx = src_x + (dx * src_w) / dst_w;
            if (sx < 0 || sx >= screen_width) continue;
            dst_buf[dy * dst_w + dx] = virtual_buffer[sy * screen_width + sx];
        }
    }
}

void draw_preview_from_buffer(int dst_x, int dst_y, unsigned int *src_buf, int src_w, int src_h) {
    for (int y = 0; y < src_h; y++) {
        for (int x = 0; x < src_w; x++) {
            draw_pixel(dst_x + x, dst_y + y, src_buf[y * src_w + x]);
        }
    }
}

void draw_hover_previews(void) {
    int is_hover_term = 0;
    if (is_terminal_open) {
        if (mouse_x >= 40 && mouse_x < 68 && mouse_y >= screen_height - 32) {
            is_hover_term = 1;
        }
        int prev_x = 10;
        int prev_y = screen_height - 32 - 130;
        int prev_w = 150;
        int prev_h = 110;
        if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h + 32) {
            is_hover_term = 1;
        }
    }

    if (is_hover_term) {
        int prev_x = 10;
        int prev_y = screen_height - 32 - 130;
        int prev_w = 150;
        int prev_h = 110;

        draw_rect(prev_x, prev_y, prev_w, prev_h, figma_taskbar);
        draw_rect(prev_x, prev_y, prev_w, 24, dark_gray);
        draw_rect(prev_x, prev_y, prev_w, 1, figma_line);
        draw_rect(prev_x, prev_y + 24, prev_w, 1, figma_line);
        draw_rect(prev_x, prev_y, 1, prev_h, figma_line);
        draw_rect(prev_x + prev_w - 1, prev_y, 1, prev_h, figma_line);
        draw_rect(prev_x, prev_y + prev_h - 1, prev_w, 1, figma_line);

        draw_string_raw(prev_x + 8, prev_y + 8, "Terminal", white_color);
        draw_char_raw(prev_x + prev_w - 18, prev_y + 8, 'x', white_color);

        int mini_x = prev_x + 8;
        int mini_y = prev_y + 32;
        draw_preview_from_buffer(mini_x, mini_y, terminal_preview, 134, 70);
    }

    int is_hover_files = 0;
    if (is_files_open) {
        if (mouse_x >= 68 && mouse_x < 96 && mouse_y >= screen_height - 32) {
            is_hover_files = 1;
        }
        int prev_x = 44;
        int prev_y = screen_height - 32 - 130;
        int prev_w = 150;
        int prev_h = 110;
        if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h + 32) {
            is_hover_files = 1;
        }
    }

    if (is_hover_files) {
        int prev_x = 44;
        int prev_y = screen_height - 32 - 130;
        int prev_w = 150;
        int prev_h = 110;

        draw_rect(prev_x, prev_y, prev_w, prev_h, figma_taskbar);
        draw_rect(prev_x, prev_y, prev_w, 24, dark_gray);
        draw_rect(prev_x, prev_y, prev_w, 1, figma_line);
        draw_rect(prev_x, prev_y + 24, prev_w, 1, figma_line);
        draw_rect(prev_x, prev_y, 1, prev_h, figma_line);
        draw_rect(prev_x + prev_w - 1, prev_y, 1, prev_h, figma_line);
        draw_rect(prev_x, prev_y + prev_h - 1, prev_w, 1, figma_line);

        draw_string_raw(prev_x + 8, prev_y + 8, "Files", white_color);
        draw_char_raw(prev_x + prev_w - 18, prev_y + 8, 'x', white_color);

        int mini_x = prev_x + 8;
        int mini_y = prev_y + 32;
        draw_preview_from_buffer(mini_x, mini_y, files_preview, 134, 70);
    }
}

void redraw_desktop() {
    draw_rect(0, 0, screen_width, screen_height - 32, figma_bg);
    draw_rect(0, screen_height - 32, screen_width, 32, figma_taskbar);
    
    draw_string_raw(20, screen_height - 20, "mg", gray_color);
    if (terminal_icon) {
        draw_image(44, screen_height - 24, terminal_icon);
        if (is_terminal_open) {
            draw_rect(44, screen_height - 32, 16, 2, white_color);
        }
    } else {
        draw_string_raw(44, screen_height - 20, "[terminal]", gray_color);
        if (is_terminal_open) {
            draw_rect(44, screen_height - 32, 80, 2, white_color);
        }
    }
    if (files_icon) {
        draw_image(72, screen_height - 24, files_icon);
        if (is_files_open) {
            draw_rect(72, screen_height - 32, 16, 2, white_color);
        }
    } else {
        draw_string_raw(72, screen_height - 20, "[files]", gray_color);
        if (is_files_open) {
            draw_rect(72, screen_height - 32, 56, 2, white_color);
        }
    }
    
    // Editor icon on taskbar (notepad look)
    int icon_x = 100;
    int icon_y = screen_height - 22;
    draw_rect(icon_x, icon_y, 12, 14, 0x003498db);
    draw_rect(icon_x + 2, icon_y + 1, 10, 12, white_color);
    draw_rect(icon_x + 4, icon_y + 3, 6, 1, gray_color);
    draw_rect(icon_x + 4, icon_y + 6, 6, 1, gray_color);
    draw_rect(icon_x + 4, icon_y + 9, 6, 1, gray_color);
    if (is_editor_open) {
        draw_rect(100, screen_height - 32, 16, 2, white_color);
    }

    if (active_focus == 0) {
        if (is_files_open && !is_files_minimized) {
            draw_files_window();
        }
        if (is_editor_open && !is_editor_minimized) {
            draw_editor_window();
        }
        if (is_terminal_open && !is_terminal_minimized) {
            draw_rect(term_x, term_y, term_w, term_h, figma_window);
            draw_rect(term_x, term_y, term_w, 40, figma_topbar);
            draw_string_raw(term_x + 40, term_y + 16, "Terminal", white_color);
            if (terminal_icon) {
                draw_image(term_x + 15, term_y + 12, terminal_icon);
            } else {
                draw_rect(term_x + 15, term_y + 14, 12, 12, white_color);
            }
            draw_rect(term_x, term_y + 40, term_w, 1, figma_line);
            draw_char_raw(term_x + term_w - 64, term_y + 16, '_', white_color);
            draw_rect(term_x + term_w - 46, term_y + 14, 10, 10, figma_window);
            draw_rect(term_x + term_w - 45, term_y + 15, 8, 8, white_color);
            draw_char_raw(term_x + term_w - 24, term_y + 16, 'x', white_color);
            draw_terminal_text();
            capture_preview_to_buffer(term_x, term_y, term_w, term_h, terminal_preview, 134, 70);
        }
    } else if (active_focus == 1) {
        if (is_terminal_open && !is_terminal_minimized) {
            draw_rect(term_x, term_y, term_w, term_h, figma_window);
            draw_rect(term_x, term_y, term_w, 40, figma_topbar);
            draw_string_raw(term_x + 40, term_y + 16, "Terminal", white_color);
            if (terminal_icon) {
                draw_image(term_x + 15, term_y + 12, terminal_icon);
            } else {
                draw_rect(term_x + 15, term_y + 14, 12, 12, white_color);
            }
            draw_rect(term_x, term_y + 40, term_w, 1, figma_line);
            draw_char_raw(term_x + term_w - 64, term_y + 16, '_', white_color);
            draw_rect(term_x + term_w - 46, term_y + 14, 10, 10, figma_window);
            draw_rect(term_x + term_w - 45, term_y + 15, 8, 8, white_color);
            draw_char_raw(term_x + term_w - 24, term_y + 16, 'x', white_color);
            draw_terminal_text();
            capture_preview_to_buffer(term_x, term_y, term_w, term_h, terminal_preview, 134, 70);
        }
        if (is_editor_open && !is_editor_minimized) {
            draw_editor_window();
        }
        if (is_files_open && !is_files_minimized) {
            draw_files_window();
            capture_preview_to_buffer(files_x, files_y, files_w, files_h, files_preview, 134, 70);
        }
    } else if (active_focus == 2) {
        if (is_terminal_open && !is_terminal_minimized) {
            draw_rect(term_x, term_y, term_w, term_h, figma_window);
            draw_rect(term_x, term_y, term_w, 40, figma_topbar);
            draw_string_raw(term_x + 40, term_y + 16, "Terminal", white_color);
            if (terminal_icon) {
                draw_image(term_x + 15, term_y + 12, terminal_icon);
            } else {
                draw_rect(term_x + 15, term_y + 14, 12, 12, white_color);
            }
            draw_rect(term_x, term_y + 40, term_w, 1, figma_line);
            draw_char_raw(term_x + term_w - 64, term_y + 16, '_', white_color);
            draw_rect(term_x + term_w - 46, term_y + 14, 10, 10, figma_window);
            draw_rect(term_x + term_w - 45, term_y + 15, 8, 8, white_color);
            draw_char_raw(term_x + term_w - 24, term_y + 16, 'x', white_color);
            draw_terminal_text();
            capture_preview_to_buffer(term_x, term_y, term_w, term_h, terminal_preview, 134, 70);
        }
        if (is_files_open && !is_files_minimized) {
            draw_files_window();
            capture_preview_to_buffer(files_x, files_y, files_w, files_h, files_preview, 134, 70);
        }
        if (is_editor_open && !is_editor_minimized) {
            draw_editor_window();
            capture_preview_to_buffer(editor_x, editor_y, editor_w, editor_h, editor_preview, 134, 70);
        }
    }

    update_system_tray();
    draw_hover_previews();
    draw_mouse_cursor(mouse_x, mouse_y);
}

void keyboard_callback(unsigned char scancode) {
    if (scancode == 0x1d) { ctrl_pressed = 1; return; }
    if (scancode == 0x9d) { ctrl_pressed = 0; return; }
    if (scancode == 0x38) { alt_pressed = 1; return; }
    if (scancode == 0xb8) { alt_pressed = 0; return; }
    
    if (scancode == 0x5b || scancode == 0x5c) { win_pressed = 1; return; }
    
    if (scancode & 0x80) {
        if (scancode == 0xdb || scancode == 0xdc) { win_pressed = 0; }
        return;
    }
    
    if (scancode == 0x32 && ctrl_pressed && alt_pressed) {
        if (!is_terminal_open || is_terminal_minimized) { is_terminal_open = 1; is_terminal_minimized = 0; term_index = 0; cmd_output_active = 0; }
        active_focus = 0;
        redraw_desktop(); swap_full_buffer(); return;
    }
    
    if (scancode == 0x39 && win_pressed) {
        current_layout = !current_layout;
        redraw_desktop();
        swap_full_buffer();
        return;
    }
    
    if (active_focus == 0 && is_terminal_open && !is_terminal_minimized) {
        if (scancode == 0x0e) { 
            if (term_index > 0) {
                term_index--;
                while (term_index > 0 && ((unsigned char)term_buffer[term_index] & 0xC0) == 0x80) {
                    term_index--;
                }
                redraw_desktop(); 
                swap_full_buffer(); 
            }
            return;
        }
        if (scancode == 0x1c) { 
            term_buffer[term_index] = '\0';
            cmd_output_active = 1;
            process_command(term_buffer);
            term_index = 0;
            redraw_desktop();
            swap_full_buffer();
            return;
        }
        if (scancode < 58) {
            unsigned int cp = 0;
            if (current_layout == 1) {
                cp = keymap_ru[scancode];
                if (cp == 0) {
                    cp = keymap_en[scancode];
                }
            } else {
                cp = keymap_en[scancode];
            }
            
            if (cp >= 32 && term_index < 110) {
                append_codepoint_to_term(cp);
                redraw_desktop();
                swap_full_buffer();
            }
        }
    } else if (active_focus == 2 && is_editor_open && !is_editor_minimized) {
        if (scancode == 0x1f && ctrl_pressed) {
            char full_path[128];
            int len = 0;
            const char *active_dir = files_history[files_history_ptr];
            while (active_dir[len]) {
                full_path[len] = active_dir[len];
                len++;
            }
            if (len > 0 && full_path[len - 1] != '/') {
                full_path[len++] = '/';
            }
            int j = 0;
            while (editor_file_name[j] && len < 127) {
                full_path[len++] = editor_file_name[j++];
            }
            full_path[len] = '\0';

            struct v_file *curr = get_root_files();
            while (curr) {
                if (strcmp(curr->name, full_path) == 0) {
                    int k = 0;
                    while (editor_file_content[k] && k < 1023) {
                        curr->content[k] = editor_file_content[k];
                        k++;
                    }
                    curr->content[k] = '\0';
                    editor_unsaved_changes = 0;
                    break;
                }
                curr = curr->next;
            }
            redraw_desktop();
            swap_full_buffer();
            return;
        }
        if (scancode == 0x0e) { 
            int len = 0;
            while (editor_file_content[len]) len++;
            if (len > 0) {
                len--;
                while (len > 0 && ((unsigned char)editor_file_content[len] & 0xC0) == 0x80) {
                    len--;
                }
                editor_file_content[len] = '\0';
                editor_unsaved_changes = 1;
                redraw_desktop(); 
                swap_full_buffer(); 
            }
            return;
        }
        if (scancode == 0x1c) { 
            int len = 0;
            while (editor_file_content[len]) len++;
            if (len < 1022) {
                editor_file_content[len++] = '\n';
                editor_file_content[len] = '\0';
                editor_unsaved_changes = 1;
                redraw_desktop();
                swap_full_buffer();
            }
            return;
        }
        if (scancode < 58) {
            unsigned int cp = 0;
            if (current_layout == 1) {
                cp = keymap_ru[scancode];
                if (cp == 0) {
                    cp = keymap_en[scancode];
                }
            } else {
                cp = keymap_en[scancode];
            }
            
            if (cp >= 32) {
                append_codepoint_to_editor(cp);
                redraw_desktop();
                swap_full_buffer();
            }
        }
    }
}

void mouse_callback(unsigned char response) {
    switch (mouse_cycle) {
        case 0: mouse_packet[0] = response; if ((response & 0x08) == 0x08) mouse_cycle++; break;
        case 1: mouse_packet[1] = response; mouse_cycle++; break;
        case 2:
            mouse_packet[2] = response; mouse_cycle = 0;
            int rel_x = (int)mouse_packet[1];
            int rel_y = (int)mouse_packet[2];
            if (mouse_packet[0] & 0x10) rel_x -= 256;
            if (mouse_packet[0] & 0x20) rel_y -= 256;
            old_mouse_x = mouse_x; old_mouse_y = mouse_y;
            
            // Adjust sensitivity (making it responsive but not too wild)
            mouse_x += rel_x / 2; mouse_y -= rel_y / 2;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= screen_width) mouse_x = screen_width - 1;
            if (mouse_y >= screen_height) mouse_y = screen_height - 1;
            
            if (mouse_packet[0] & 0x01) { 
                // Determine focus based on click coordinates first
                if (is_editor_open && !is_editor_minimized &&
                    mouse_x >= editor_x && mouse_x < editor_x + editor_w &&
                    mouse_y >= editor_y && mouse_y < editor_y + editor_h) {
                    active_focus = 2;
                } else if (is_files_open && !is_files_minimized &&
                           mouse_x >= files_x && mouse_x < files_x + files_w &&
                           mouse_y >= files_y && mouse_y < files_y + files_h) {
                    active_focus = 1;
                } else if (is_terminal_open && !is_terminal_minimized &&
                           mouse_x >= term_x && mouse_x < term_x + term_w &&
                           mouse_y >= term_y && mouse_y < term_y + term_h) {
                    active_focus = 0;
                }

                if (!is_dragging) {
                    // Start dragging active window
                    if (active_focus == 0 && is_terminal_open && !is_terminal_minimized && !is_terminal_maximized &&
                        mouse_x >= term_x && mouse_x < term_x + term_w - 75 &&
                        mouse_y >= term_y && mouse_y < term_y + 40) {
                        is_dragging = 1;
                        drag_offset_x = mouse_x - term_x;
                        drag_offset_y = mouse_y - term_y;
                    } else if (active_focus == 1 && is_files_open && !is_files_minimized && !is_files_maximized &&
                               mouse_x >= files_x && mouse_x < files_x + files_w - 75 &&
                               mouse_y >= files_y && mouse_y < files_y + 40) {
                        is_dragging = 2;
                        drag_offset_x = mouse_x - files_x;
                        drag_offset_y = mouse_y - files_y;
                    } else if (active_focus == 2 && is_editor_open && !is_editor_minimized && !is_editor_maximized &&
                               mouse_x >= editor_x && mouse_x < editor_x + editor_w - 75 &&
                               mouse_y >= editor_y && mouse_y < editor_y + 40) {
                        is_dragging = 3;
                        drag_offset_x = mouse_x - editor_x;
                        drag_offset_y = mouse_y - editor_y;
                    }
                } else {
                    // Continue dragging
                    if (is_dragging == 1) {
                        term_x = mouse_x - drag_offset_x;
                        term_y = mouse_y - drag_offset_y;
                    } else if (is_dragging == 2) {
                        files_x = mouse_x - drag_offset_x;
                        files_y = mouse_y - drag_offset_y;
                    } else if (is_dragging == 3) {
                        editor_x = mouse_x - drag_offset_x;
                        editor_y = mouse_y - drag_offset_y;
                    }
                }

                if (is_terminal_open) {
                    int is_hover_term = 0;
                    if (mouse_x >= 40 && mouse_x < 68 && mouse_y >= screen_height - 32) is_hover_term = 1;
                    int prev_x = 10;
                    int prev_y = screen_height - 32 - 130;
                    int prev_w = 150;
                    int prev_h = 110;
                    if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h + 32) is_hover_term = 1;

                    if (is_hover_term) {
                        if (mouse_x >= prev_x + prev_w - 22 && mouse_x < prev_x + prev_w - 4 &&
                            mouse_y >= prev_y + 4 && mouse_y < prev_y + 20) {
                            is_terminal_open = 0;
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h) {
                            is_terminal_minimized = 0;
                            active_focus = 0;
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                    }
                }
                
                if (is_files_open) {
                    int is_hover_files = 0;
                    if (mouse_x >= 68 && mouse_x < 96 && mouse_y >= screen_height - 32) is_hover_files = 1;
                    int prev_x = 44;
                    int prev_y = screen_height - 32 - 130;
                    int prev_w = 150;
                    int prev_h = 110;
                    if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h + 32) is_hover_files = 1;

                    if (is_hover_files) {
                        if (mouse_x >= prev_x + prev_w - 22 && mouse_x < prev_x + prev_w - 4 &&
                            mouse_y >= prev_y + 4 && mouse_y < prev_y + 20) {
                            is_files_open = 0;
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h) {
                            is_files_minimized = 0;
                            active_focus = 1;
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                    }
                }

                if (is_editor_open) {
                    int is_hover_editor = 0;
                    if (mouse_x >= 96 && mouse_x < 124 && mouse_y >= screen_height - 32) is_hover_editor = 1;
                    int prev_x = 78;
                    int prev_y = screen_height - 32 - 130;
                    int prev_w = 150;
                    int prev_h = 110;
                    if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h + 32) is_hover_editor = 1;

                    if (is_hover_editor) {
                        if (mouse_x >= prev_x + prev_w - 22 && mouse_x < prev_x + prev_w - 4 &&
                            mouse_y >= prev_y + 4 && mouse_y < prev_y + 20) {
                            is_editor_open = 0;
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= prev_x && mouse_x < prev_x + prev_w && mouse_y >= prev_y && mouse_y < prev_y + prev_h) {
                            is_editor_minimized = 0;
                            active_focus = 2;
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                    }
                }

                if (mouse_y >= screen_height - 32) {
                    if (mouse_x >= 40 && mouse_x < 68) {
                        if (is_terminal_open) {
                            is_terminal_minimized = !is_terminal_minimized;
                            if (!is_terminal_minimized) active_focus = 0;
                        } else {
                            is_terminal_open = 1;
                            is_terminal_minimized = 0;
                            active_focus = 0;
                            term_index = 0;
                            cmd_output_active = 0;
                        }
                        redraw_desktop(); swap_full_buffer(); return;
                    }
                    if (mouse_x >= 68 && mouse_x < 96) {
                        if (is_files_open) {
                            is_files_minimized = !is_files_minimized;
                            if (!is_files_minimized) active_focus = 1;
                        } else {
                            is_files_open = 1;
                            is_files_minimized = 0;
                            active_focus = 1;
                            files_history_ptr = 0;
                            files_history_max = 1;
                            strcpy(files_history[0], "/shadow/user");
                        }
                        redraw_desktop(); swap_full_buffer(); return;
                    }
                    if (mouse_x >= 96 && mouse_x < 124) {
                        if (is_editor_open) {
                            is_editor_minimized = !is_editor_minimized;
                            if (!is_editor_minimized) active_focus = 2;
                        } else {
                            is_editor_open = 1;
                            is_editor_minimized = 0;
                            active_focus = 2;
                            strcpy(editor_file_name, "untitled.txt");
                            editor_file_content[0] = '\0';
                            editor_unsaved_changes = 0;
                        }
                        redraw_desktop(); swap_full_buffer(); return;
                    }
                }
                
                if (is_terminal_open && !is_terminal_minimized && active_focus == 0) {
                    if (mouse_y >= term_y && mouse_y < (term_y + 40)) {
                        if (mouse_x >= (term_x + term_w - 30) && mouse_x < term_x + term_w) {
                            is_terminal_open = 0; redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= (term_x + term_w - 52) && mouse_x < (term_x + term_w - 34)) {
                            if (is_terminal_maximized) {
                                is_terminal_maximized = 0; term_x = saved_term_x; term_y = saved_term_y; term_w = saved_term_w; term_h = saved_term_h;
                            } else {
                                is_terminal_maximized = 1; saved_term_x = term_x; saved_term_y = term_y; saved_term_w = term_w; saved_term_h = term_h;
                                term_x = 0; term_y = 0; term_w = 1024; term_h = 736;
                            }
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= (term_x + term_w - 70) && mouse_x < (term_x + term_w - 52)) {
                            is_terminal_minimized = 1; redraw_desktop(); swap_full_buffer(); return;
                        }
                    }
                }
                
                if (is_files_open && !is_files_minimized && active_focus == 1) {
                    if (mouse_y >= files_y && mouse_y < (files_y + 40)) {
                        if (mouse_x >= (files_x + files_w - 30) && mouse_x < files_x + files_w) {
                            is_files_open = 0; redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= (files_x + files_w - 52) && mouse_x < (files_x + files_w - 34)) {
                            if (is_files_maximized) {
                                is_files_maximized = 0; files_x = saved_files_x; files_y = saved_files_y; files_w = saved_files_w; files_h = saved_files_h;
                            } else {
                                is_files_maximized = 1; saved_files_x = files_x; saved_files_y = files_y; saved_files_w = files_w; saved_files_h = files_h;
                                files_x = 0; files_y = 0; files_w = 1024; files_h = 736;
                            }
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= (files_x + files_w - 70) && mouse_x < (files_x + files_w - 52)) {
                            is_files_minimized = 1; redraw_desktop(); swap_full_buffer(); return;
                        }
                    }
                    if (mouse_y >= files_y + 41 && mouse_y < files_y + 72) {
                        if (mouse_x >= files_x + 10 && mouse_x < files_x + 35) {
                            if (files_history_ptr > 0) {
                                files_history_ptr--;
                                redraw_desktop(); swap_full_buffer(); return;
                            }
                        }
                        if (mouse_x >= files_x + 40 && mouse_x < files_x + 65) {
                            if (files_history_ptr < files_history_max - 1) {
                                files_history_ptr++;
                                redraw_desktop(); swap_full_buffer(); return;
                            }
                        }
                    }
                    if (mouse_y >= files_y + 80 && mouse_x >= files_x + 10 && mouse_x < files_x + files_w - 10) {
                        int clicked_row = (mouse_y - (files_y + 80)) / 24;
                        struct v_file *curr = get_root_files();
                        int i = 0;
                        char display_name[64];
                        while (curr) {
                            if (get_file_in_dir(curr->name, files_history[files_history_ptr], display_name)) {
                                if (i == clicked_row) {
                                    if (curr->is_dir) {
                                        if (files_history_ptr < 9) {
                                            files_history_ptr++;
                                            strcpy(files_history[files_history_ptr], curr->name);
                                            files_history_max = files_history_ptr + 1;
                                        }
                                    } else {
                                        is_editor_open = 1;
                                        is_editor_minimized = 0;
                                        active_focus = 2;
                                        strcpy(editor_file_name, display_name);
                                        
                                        int k = 0;
                                        while (curr->content[k] && k < 1023) {
                                            editor_file_content[k] = curr->content[k];
                                            k++;
                                        }
                                        editor_file_content[k] = '\0';
                                        editor_unsaved_changes = 0;
                                    }
                                    redraw_desktop();
                                    swap_full_buffer();
                                    return;
                                }
                                i++;
                            }
                            curr = curr->next;
                        }
                    }
                }

                if (is_editor_open && !is_editor_minimized && active_focus == 2) {
                    if (mouse_y >= editor_y && mouse_y < (editor_y + 40)) {
                        if (mouse_x >= (editor_x + editor_w - 30) && mouse_x < editor_x + editor_w) {
                            is_editor_open = 0; redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= (editor_x + editor_w - 52) && mouse_x < (editor_x + editor_w - 34)) {
                            if (is_editor_maximized) {
                                is_editor_maximized = 0; editor_x = saved_editor_x; editor_y = saved_editor_y; editor_w = saved_editor_w; editor_h = saved_editor_h;
                            } else {
                                is_editor_maximized = 1; saved_editor_x = editor_x; saved_editor_y = editor_y; saved_editor_w = editor_w; saved_editor_h = editor_h;
                                editor_x = 0; editor_y = 0; editor_w = 1024; editor_h = 736;
                            }
                            redraw_desktop(); swap_full_buffer(); return;
                        }
                        if (mouse_x >= (editor_x + editor_w - 70) && mouse_x < (editor_x + editor_w - 52)) {
                            is_editor_minimized = 1; redraw_desktop(); swap_full_buffer(); return;
                        }
                    }
                }
            } else {
                is_dragging = 0;
            }

            redraw_desktop();
            int min_x = (old_mouse_x < mouse_x) ? old_mouse_x : mouse_x;
            int max_x = ((old_mouse_x > mouse_x) ? old_mouse_x : mouse_x) + 16;
            int min_y = (old_mouse_y < mouse_y) ? old_mouse_y : mouse_y;
            int max_y = ((old_mouse_y > mouse_y) ? old_mouse_y : mouse_y) + 16;
            swap_buffers_dirty(min_x, min_y, max_x, max_y);
            if (is_terminal_open) {
                swap_buffers_dirty(10, screen_height - 32 - 130, 10 + 150, screen_height - 32);
            }
            if (is_files_open) {
                swap_buffers_dirty(44, screen_height - 32 - 130, 44 + 150, screen_height - 32);
            }
            if (is_editor_open) {
                swap_buffers_dirty(78, screen_height - 32 - 130, 78 + 150, screen_height - 32);
            }
            break;
    }
}

void init_ramdisk(void) {
}

void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    (void)magic; (void)mbi;
    init_heap();
    terminal_icon = decode_tga(terminal_tga_data, terminal_tga_size);
    files_icon = create_files_icon();
    init_shell();
    outw(0x1ce, 4); outw(0x1cf, 0);      
    outw(0x1ce, 1); outw(0x1cf, 1024);   
    outw(0x1ce, 2); outw(0x1cf, 768);    
    outw(0x1ce, 3); outw(0x1cf, 32);     
    outw(0x1ce, 4); outw(0x1cf, 0x41);   
    redraw_desktop();
    swap_full_buffer();
    while (1) {
        __asm__ volatile("hlt");
    }
}
