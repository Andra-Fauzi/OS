#include "something.h"

extern "C" void printf(const char *str, ...);

class A {
public:
    char nama[255];
    void print() {
        printf("dari c++\n");
    }
    void setnama(char *nama) {
        int i = 0;
        while(nama[i]) {
            this->nama[i] = nama[i];
            i++;
        }
        this->nama[i] = '\0';
    }
    void print_nama() {
        printf("%s\n", this->nama);
    }
};

void something() {
    A a;
    a.print();
    a.setnama("Andra");
    a.print_nama();
}