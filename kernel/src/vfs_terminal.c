#include "vfs_terminal.h"
#include "keyboard.h"
#include "terminal.h"
#include "util.h" // for NULL

// Terminal Open: returns dummy pointer (singleton device)
void* terminal_open(const char *path, int flags) {
    (void)path; 
    (void)flags;
    return (void*)1; 
}

// Terminal Close
void terminal_close(void *fs_file) {
    (void)fs_file;
}

// Terminal Read: Line-buffered blocking read from keyboard
int terminal_read(void *fs_file, void *buf, size_t size) {
    (void)fs_file;
    char *char_buf = (char*)buf;
    size_t count = 0;

    while (count < size) {
        char c = keyboard_getchar();
        if (c != -1) { 
             write_terminal(c); // Echo to screen

             if (c == '\b') {
                 // Handle backspace: remove last char from buffer
                 if (count > 0) {
                     count--;
                 }
             } else {
                 char_buf[count++] = c;
             }

             if (c == '\n') {
                 break;
             }
        } 
        // Busy wait if no character available
    }
    return count;
}

// Terminal Write: Writes buffer directly to screen
int terminal_write(void *fs_file, const void *buf, size_t size) {
    (void)fs_file;
    const char *char_buf = (const char*)buf;
    for (size_t i = 0; i < size; i++) {
        write_terminal(char_buf[i]);
    }
    return size;
}

static fs_operations_t terminal_ops = {
    .open = terminal_open,
    .close = terminal_close,
    .read = terminal_read,
    .write = terminal_write
};

void vfs_terminal_init() {
    // Nothing special to init, keyboard is already inited in main
}

fs_operations_t* vfs_terminal_get_ops() {
    return &terminal_ops;
}
