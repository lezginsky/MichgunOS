#ifndef SHELL_H
#define SHELL_H

struct v_file {
    char name[32];
    char content[128];
    int is_dir;
    struct v_file *next;
};

void init_shell(void);
void process_command(const char *cmd_line);
const char *get_last_output(void);
const char *get_current_dir(void);
int is_output_active(void);
void clear_shell_output(void);
struct v_file *get_root_files(void);

#endif
