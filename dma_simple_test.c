#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "user_config.h"
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>

#define RW_MAX_SIZE 1048576UL

enum RW_OPTS
{
    READ,
    WRITE
};

int qdma_rw(int dev_fd, off_t axi_base_addr, void *buffer, uint64_t size, enum RW_OPTS rw);
int memcmp_user(void *buff1, void *buff2, size_t size);
int open_dev(char **dev_path);

int main(int argc, char* argv[])
{

    printf("QDMA/XDMA simple Test \n");
    char *dev_path = DEV_PATH;
    int dev_fd = open_dev(&dev_path);

    unsigned char *read_buf = NULL;
    unsigned char *write_buf = NULL;

    if (posix_memalign((void *)&read_buf, 4096, BUFF_SIZE + 4096) != 0 || posix_memalign((void *)&write_buf, 4096, BUFF_SIZE + 4096) != 0)
    {
        perror("Fail to allocate buffer\n");
        exit(errno);
    };

    if (mlock(read_buf, BUFF_SIZE + 4096) || mlock(write_buf, BUFF_SIZE + 4096))
    {
        perror("mlock");
        exit(errno);
    }
    struct timeval tval_before, tval_after, tval_result;
    int64_t millisec = 0;

    // ---------------------------------------------   Write Test ----------------------------------------------------------------------
    if (argc > 1) {
        int data_fd = open(argv[1], O_RDONLY);
        if(data_fd == -1) {
            perror("Open input file");
            exit(errno);
        }
        int ret = read(data_fd, write_buf, WRITE_SIZE);
        printf("Load %d bytes from input file \n", ret);
    } else {
        memset(write_buf, 0x00, BUFF_SIZE);
    }

    printf("Start Write %ld bytes from AXI 0x%08lx. \n", WRITE_SIZE, AXI_BASE_ADDR);
    int64_t total_write_bytes = 0;
    gettimeofday(&tval_before, NULL);

    // DMA Write 수행
    int write_loop_count = WRITE_LOOP_COUNT;
    while (write_loop_count--)
    {
        total_write_bytes += qdma_rw(dev_fd, AXI_BASE_ADDR, write_buf, WRITE_SIZE, WRITE);
    }

    // 시간차 계산
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);
    millisec = (tval_result.tv_sec * 1000) + (tval_result.tv_usec / 1000);
    printf("Total Write %ld bytes (%.02f MB) in %ld ms, %.04f MB/s \n", total_write_bytes, total_write_bytes / 1000000.f, millisec, (total_write_bytes / 1000000.f) / (millisec / 1000.f));

    // ---------------------------------------------  Write Test END ----------------------------------------------------------------------

    // ---------------------------------------------   Read Test ----------------------------------------------------------------------
    printf("Start Read %ld bytes from AXI 0x%08lx. \n", READ_SIZE, AXI_BASE_ADDR);
    int64_t total_read_bytes = 0;
    gettimeofday(&tval_before, NULL);

    // DMA Read 수행
    int read_loop_count = READ_LOOP_COUNT;
    while (read_loop_count--)
    {
        total_read_bytes += qdma_rw(dev_fd, AXI_BASE_ADDR, read_buf, READ_SIZE, READ);
    }

    // 시간차 계산
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);
    millisec = (tval_result.tv_sec * 1000) + (tval_result.tv_usec / 1000);
    printf("Total Read %ld bytes (%.02f MB) in %ld ms, %.04f MB/s \n", total_read_bytes, total_read_bytes / 1000000.f, millisec, (total_read_bytes / 1000000.f) / (millisec / 1000.f));

    if (memcmp_user(read_buf, write_buf, READ_SIZE) != 0)
    {
        printf("TX/RX Data not match!\n");
    }
    else
    {
        printf("TX/RX Data match! \n");
    }

    printf("First 256 bytes: ");
    for (int i = 0; i < 256; i++)
    {
        if (i % 50 == 0)
        {
            printf("\n 0x%06x | ", i);
        }
        printf("%02x ", read_buf[i]);
    }
    printf("\n");

    // ---------------------------------------------   Read Test END ----------------------------------------------------------------------

    close(dev_fd);
    free(read_buf);
    free(write_buf);

    exit(EXIT_SUCCESS);
}

unsigned int no_memcpy = 1;
int open_dev(char **dev_path)
{
    int dev = open(DEV_PATH, O_RDWR | O_SYNC);

    if (dev == -1)
    {
        perror(DEV_PATH);
        exit(errno);
    }
    else
    {
        printf("Dev %s Opened\n", DEV_PATH);
    }

    if (ioctl(dev, 0, &no_memcpy) != 0)
    {
        printf("Fail to set non memcpy\n");
        exit(1);
    };
    return dev;
}

/**
 * @brief dev_fd 의 AXI address <-> buffer 간 size만큼 전송
 * @param rw 0: read, 1: write
 * @return 읽은 바이트의 수. 실패시 -1
 */
int qdma_rw(int dev_fd, off_t axi_base_addr, void *buffer, uint64_t size, enum RW_OPTS rw)
{

    int64_t buff_cursor = 0;
    while (buff_cursor < size)
    {
        // 읽거나 쓴 만큼 뒤로 탐색
        int ret = lseek(dev_fd, axi_base_addr + buff_cursor, SEEK_SET);
        if (ret == -1)
        {
            perror("dev lseek() fail!\n");
            return -1;
        }

        uint64_t bytes = size - buff_cursor;
        if (bytes > RW_MAX_SIZE)
            bytes = RW_MAX_SIZE;

        int64_t rw_ret;
        if (rw == WRITE)
        {
            rw_ret = write(dev_fd, (buffer + buff_cursor), bytes);
        }
        else
        {
            rw_ret = read(dev_fd, (buffer + buff_cursor), bytes);
        }

        // RW 실패
        if (rw_ret != bytes)
        {
            perror("rw fail!");
            break;
        }

        buff_cursor += rw_ret;
    }

    return buff_cursor;
}

int memcmp_user(void *buff1, void *buff2, size_t size)
{
    unsigned char *ptr1 = buff1;
    unsigned char *ptr2 = buff2;

    int i = 0;
    while (size--)
    {
        i++;
        if (*ptr1 != *ptr2)
        {
            printf("byte %d not match! ptr1: %02x ptr2 %02x \n", i, *ptr1, *ptr2);
            return *ptr1 - *ptr2;
        }
        ptr1++;
        ptr2++;
    }
    return 0;
}
