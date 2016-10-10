#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define ALIGN_DOWN(p,n) (((ptrdiff_t)(p)) & ~((n)-1))
#define ALIGN_UP(p,n) ALIGN_DOWN((ptrdiff_t)(p)+(n)-1,(n))

int main(int argc, char *argv[]) {
    int z = 854;
    //int z = 0xff;

    printf("0x%.4x, 0x%.4x\n", z, ALIGN_DOWN(z, 16));
    printf("0x%.4x, 0x%.4x\n", z, ALIGN_UP(z, 16));
    printf("0x%.4x, 0x%.4x\n", z, ALIGN_DOWN(z, 32));
    printf("0x%.4x, 0x%.4x\n", z, ALIGN_UP(z, 32));
}

