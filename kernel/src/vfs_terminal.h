#pragma once

#include "vfs.h"

// Initialize Terminal VFS if needed (optional)
void vfs_terminal_init();

// Get the FS operations for the terminal
fs_operations_t* vfs_terminal_get_ops();
