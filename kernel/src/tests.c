#include "tests.h"
#include "vfs.h"
#include "util.h"
#include "ahci.h"
#include "terminal.h"
#include "paging.h"
#include "elf.h"

void test_vfs_fat() {
    printf("Testing VFS (FAT)...\n");
    int fd = vfs_open("/", O_RDWR);
    if (fd >= 0) {
        printf("VFS Open Success: fd=%d\n", (int64_t)fd);
        char *msg = "Hello VFS World!";
        vfs_write(fd, msg, 16);
        vfs_close(fd);
        
        // Read back
        fd = vfs_open("/", O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            memset(buf, 0, 32);
            vfs_read(fd, buf, 32);
            printf("VFS Read Result: %s\n", buf);
            vfs_close(fd);
        }
    } else {
        printf("VFS Open Failed\n");
    }
}

void test_vfs_terminal() {
    printf("Testing VFS (Terminal)...\n");
    int fd = vfs_open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        printf("VFS Open Success: fd=%d\n", fd);
        char *msg = "Hello VFS Terminal!\n";
        vfs_write(fd, msg, 21);
        vfs_close(fd);

        printf("Testing reading...\n");
        fd = vfs_open("/dev/tty", O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            memset(buf, 0, 32);
            vfs_read(fd, buf, 32);
            printf("VFS Read Result: %s\n", buf);
            vfs_close(fd);
        }
    } else {
        printf("VFS Open Failed\n");
    }
}

void test_ahci_read() {
    if (sataport) {
        printf("Testing AHCI Read...\n");
        uint64_t buf_phys = allocate_frame();
        uint8_t *buf = (uint8_t*)PHYS_TO_VIRT(buf_phys);
        
        bool success = ahci_read(sataport, 0, 0, 1, (uint16_t*)buf);
        MBR_t *mbr = (MBR_t*)buf;
        partition_entry_t *partition = (partition_entry_t*)mbr->partition_table;
        
        if(success) {
            printf("AHCI Read Success! Data:\n");
            for(int i=0; i<32; i++) { // Limit to 32 bytes for clean output
                printf("%c", buf[i]);
            }
            printf("\n");
            for(int i=0; i<4; i++) {
                printf("Partition %d Type: %x, Start: %d\n", i+1, partition[i].type, partition[i].LBA_start_sector);
            }
        } else {
            printf("AHCI Read Failed\n");
        }
    }
}

void test_elf() {
    ELF_HEADER_t *elf_header = load_elf("/ELF_TEST/T_ELF.ELF");
    run_elf(elf_header);
}

void test_vfs_readdir() {
    printf("Testing VFS Readdir (root)...\n");
    int fd = vfs_open("/", O_RDONLY);
    if (fd >= 0) {
        printf("Opened root directory (fd=%d)\n", fd);
        vfs_dirent_t dirent;
        while (vfs_readdir(fd, &dirent) == 0) {
            printf("Found: %s (Type: %s, Ino: %d)\n", dirent.name, dirent.type == 1 ? "DIR" : "FILE", dirent.ino);
        }
        vfs_close(fd);
    } else {
        printf("Failed to open root directory\n");
    }
    printf("Testing VFS Readdir (ELF_TEST/)...\n");
    fd = vfs_open("/ELF_TEST", O_RDONLY);
    if (fd >= 0) {
        printf("Opened root directory (fd=%d)\n", fd);
        vfs_dirent_t dirent;
        while (vfs_readdir(fd, &dirent) == 0) {
            printf("Found: %s (Type: %s, Ino: %d, Size: %d)\n", dirent.name, dirent.type == 1 ? "DIR" : "FILE", dirent.ino, dirent.size);
        }
        vfs_close(fd);
    } else {
        printf("Failed to open root directory\n");
    }
}

void test_vfs_mkdir() {
    printf("Testing VFS mkdir (root)...\n");
    int fd = vfs_open("/", O_RDONLY);
    if (fd >= 0) {
        int result = vfs_mkdir(fd, "jawanibkkjkjos");
        if(result == -1) {
            printf("failed to mkdir\n");
        }
        vfs_close(fd);
    } else {
        printf("Failed to open root directory\n");
    }
    vfs_close(fd);
}

void test_vfs_create_file() {
    printf("Testing VFS create file (root)...\n");
    int fd = vfs_open("/ANDRA.TXT", O_RDONLY | O_CREAT);
    printf("fd is %d\n", fd);
    if (fd >= 0) {
        int result = vfs_write(fd, "ANDRA", 5);
        if(result == -1) {
            printf("failed to write\n");
        }
        vfs_close(fd);
    } else {
        printf("Failed to create file\n");
    }
    vfs_close(fd);
}

void run_diagnostic_tests() {
    // test_ahci_read();
    // test_vfs_fat();
    // test_vfs_terminal();
    // test_vfs_create_file();
    // test_vfs_mkdir();
    // test_vfs_readdir();
}
