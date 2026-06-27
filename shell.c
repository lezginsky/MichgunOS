#include "shell.h"
#include "malloc.h"

static struct v_file *root_dir = 0;
static char current_dir[64] = "/shadow/user";
static char output_buffer[512];
static int output_active = 0;

struct v_file *get_root_files(void) {
    return root_dir;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

void init_shell(void) {
    init_heap();
    root_dir = (struct v_file *)malloc(sizeof(struct v_file));
    strcpy(root_dir->name, "readme.txt");
    strcpy(root_dir->content, "welcome to shadow zone of michgun os");
    root_dir->is_dir = 0;
    root_dir->next = (struct v_file *)malloc(sizeof(struct v_file));
    
    strcpy(root_dir->next->name, "secret.txt");
    strcpy(root_dir->next->content, "encrypted security lock: active");
    root_dir->next->is_dir = 0;
    root_dir->next->next = 0;
    
    output_buffer[0] = '\0';
}

const char *get_last_output(void) { return output_buffer; }
const char *get_current_dir(void) { return current_dir; }
int is_output_active(void) { return output_active; }
void clear_shell_output(void) { output_active = 0; output_buffer[0] = '\0'; }

void process_command(const char *cmd_line) {
    char cmd[32];
    char arg[64];
    int i = 0, j = 0;
    output_active = 1;
    while (cmd_line[i] && cmd_line[i] != ' ' && i < 31) { cmd[i] = cmd_line[i]; i++; }
    cmd[i] = '\0';
    if (cmd_line[i] == ' ') {
        i++;
        while (cmd_line[i] && j < 63) { arg[j] = cmd_line[i]; i++; j++; }
    }
    arg[j] = '\0';

    if (strcmp(cmd, "help") == 0) {
        strcpy(output_buffer, "available: help, ls, clear, cd, cat, mkdir, touch");
    } else if (strcmp(cmd, "clear") == 0) {
        clear_shell_output();
    } else if (strcmp(cmd, "ls") == 0) {
        struct v_file *curr = root_dir;
        output_buffer[0] = '\0';
        int pos = 0;
        while (curr) {
            j = 0;
            if (curr->is_dir) { output_buffer[pos++] = '['; }
            while (curr->name[j]) { output_buffer[pos++] = curr->name[j++]; }
            if (curr->is_dir) { output_buffer[pos++] = ']'; }
            output_buffer[pos++] = ' '; output_buffer[pos++] = ' ';
            curr = curr->next;
        }
        output_buffer[pos] = '\0';
    } else if (strcmp(cmd, "mkdir") == 0) {
        if (arg[0] == '\0') { strcpy(output_buffer, "mkdir: missing operand"); return; }
        struct v_file *new_dir = (struct v_file *)malloc(sizeof(struct v_file));
        strcpy(new_dir->name, arg);
        new_dir->is_dir = 1;
        new_dir->content[0] = '\0';
        new_dir->next = root_dir;
        root_dir = new_dir;
        strcpy(output_buffer, "directory created successfully");
    } else if (strcmp(cmd, "touch") == 0) {
        if (arg[0] == '\0') { strcpy(output_buffer, "touch: missing operand"); return; }
        struct v_file *new_file = (struct v_file *)malloc(sizeof(struct v_file));
        strcpy(new_file->name, arg);
        new_file->is_dir = 0;
        strcpy(new_file->content, "empty shadow file");
        new_file->next = root_dir;
        root_dir = new_file;
        strcpy(output_buffer, "file created successfully");
    } else if (strcmp(cmd, "cat") == 0) {
        if (arg[0] == '\0') { strcpy(output_buffer, "cat: missing filename"); return; }
        struct v_file *curr = root_dir;
        while (curr) {
            if (strcmp(curr->name, arg) == 0) {
                if (curr->is_dir) { strcpy(output_buffer, "cat: is a directory"); return; }
                strcpy(output_buffer, curr->content);
                return;
            }
            curr = curr->next;
        }
        strcpy(output_buffer, "cat: file not found");
    } else if (strcmp(cmd, "cd") == 0) {
        if (arg[0] == '\0') { strcpy(output_buffer, "cd: missing argument"); return; }
        struct v_file *curr = root_dir;
        while (curr) {
            if (strcmp(curr->name, arg) == 0 && curr->is_dir) {
                int len = 0;
                while (current_dir[len]) len++;
                current_dir[len++] = '/';
                j = 0;
                while (arg[j]) current_dir[len++] = arg[j++];
                current_dir[len] = '\0';
                output_active = 0;
                return;
            }
            curr = curr->next;
        }
        strcpy(output_buffer, "cd: no such directory");
    } else if (cmd[0] != '\0') {
        strcpy(output_buffer, "shadow-shell: command not found");
    } else {
        output_active = 0;
    }
}
