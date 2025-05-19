#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "user_config.h"


#define COUNTER_STOP 1 << 0
#define COUNTER_DOWN 1 << 1

int main(int argc, char **argv)
{
    int fd;
    bool counter_stop;
    bool counter_down_mode;
    uint64_t counter_step;

    printf("AXI-ST Config BAR \nAXI-Stream Data generator Config: \n");
    fd = open(USER_CONFIG_BAR, O_RDWR | O_SYNC);
    if (fd == -1)
    {
        perror("open");
        exit(errno);
    }
    void *map = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x00);
    if (map == (void *)-1)
    {
        perror("Mamory Map");
        exit(-1);
    }
    if (argc == 4) {
        uint32_t reg0, reg1, reg2;
        counter_stop = strcmp(argv[1], "1") == 0;
        counter_down_mode = strcmp(argv[2], "1") == 0;
        counter_step = strtoull(argv[3], NULL, 10);
        reg0 = counter_down_mode << 1 | counter_stop;
        reg1 = counter_step >> 32;
        reg2 = counter_step;
        memcpy(map, &reg0, 4);
        memcpy(map+4, &reg1, 4);
        memcpy(map+8, &reg2, 4);

    }

    counter_stop = (*(uint8_t *)map & COUNTER_STOP) != 0;
    counter_down_mode = (*(uint8_t *)map & COUNTER_DOWN) != 0;
    counter_step = *(uint32_t *)((char *)map + 4);
    counter_step =  counter_step << 32 | *(uint32_t *)((char *)map + 8);

    printf("Counter Stop? %d.  ", counter_stop);
    printf("Counter Down? %d.  ", counter_down_mode);
    printf("Counter Step %lu + 1. \n", counter_step);
}