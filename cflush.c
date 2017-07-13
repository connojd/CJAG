#include <stdio.h>
#include <sys/mman.h>
#include "detection/cache.h"

const int PROT = PROT_READ | PROT_WRITE | PROT_EXEC;
const int FLAGS = MAP_ANONYMOUS | MAP_PRIVATE;

int main()
{
    cache_config_l3_t l3 = get_l3_info();

    char *addr = mmap(NULL, l3.size, PROT, FLAGS, -1, 0);
    if (addr == MAP_FAILED) {
        printf("mmap failed\n"); 
        return 1;
    }

    for (int i = 0; i < l3.size; i += l3.line_size) {
        asm volatile ("clflush %0" : "+m" (addr[i]));
    }

    munmap(addr, l3.size);
    printf("Flushed L3\n");
    return 0;
}

