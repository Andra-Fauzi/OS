#pragma once

#include <limine.h>

void sys_init();
void drivers_init();
void vfs_setup_mounts();
void check_limine_revision(volatile uint64_t* revision);
void get_framebuffer(struct limine_framebuffer_request* request);
