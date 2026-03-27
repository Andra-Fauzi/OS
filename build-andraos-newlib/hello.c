#include <unistd.h>

int main() {
    char buf[100];
    write(1, "Masukkan teks: ", 14);
    int n = read(0, buf, 100); // syscall read
    write(1, "Kamu memasukkan: ", 17);
    write(1, buf, n);          // syscall write
    return 0;
}