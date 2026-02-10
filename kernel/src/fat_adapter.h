#pragma once
#include "vfs.h"
#include "fat.h"

// Initialize FAT adapter and return operations struct
fs_operations_t *fat_get_operations();
