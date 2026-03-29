#include <stdio.h>

int main() {
    char buf[100];
    printf("Masukkan kalimat: ");
    fflush(stdout);
    char *result = fgets(buf, sizeof(buf), stdin);
    if (result) {
        printf("Kamu masukkan: %s\n", buf);
    } else {
        printf("fgets failed\n");
    }
    return 0;
}
