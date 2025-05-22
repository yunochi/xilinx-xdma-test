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
#include <getopt.h>

#define COUNTER_NORMAL 0b00
#define COUNTER_DOWN 0b01
#define CONTER_STOP 0b10
#define RANDOM_DATA 0b11

const char MODES[][30] = {"up", "down", "stop", "random"};

int main(int argc, char **argv)
{
    int fd;
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

    char c;
    while ((c = getopt(argc, argv, "m:s:h")) != -1)
    {
        switch (c)
        {
        case 'm':
            for(int i=0;;i++) {
                if(i == sizeof(MODES)/sizeof(MODES[0])) {
                    printf("Invalid Data Mode!\n"); 
                    exit(-1);
                }
                else if(strcmp(optarg, MODES[i]) == 0) {
                    uint8_t data_mode;
                    data_mode = i;
                    *(uint8_t*)map = data_mode;
                    printf("SET Data Mode to %s\n", MODES[i]);
                    break;
                }
            }
            break;
        case 's':
            uint64_t counter_step;
            counter_step = strtoull(optarg, NULL, 10);
            memcpy(map+4, &counter_step, 8);
            printf("Set Counter Step to %lu\n", counter_step);
            break;
        case 'h':
        case '?':
            printf(" -m Data Mode ('up', 'down', 'stop', 'random')\n -s Counter Step \n");
            exit(0);
            break;
        }
    }

    uint64_t step;
    memcpy(&step, map+4, 8);
    uint8_t mode = *(uint8_t*)map;
    printf("Data Mode '%s' \n", MODES[mode]);
    printf("Counter Step %lu. \n", step);
}