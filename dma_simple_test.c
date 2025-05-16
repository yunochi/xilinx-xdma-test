#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "user_config.h"
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <getopt.h>

#define RW_MAX_SIZE 1048576UL

enum RW_OPTS
{
    READ,
    WRITE
};

int dma_rw(int dev_fd, off_t axi_base_addr, void *buffer, uint64_t size, enum RW_OPTS rw, const char *test_mode);
int memcmp_user(void *buff1, void *buff2, size_t size);
int open_dev(const char *dev_path);

int main(int argc, char *argv[])
{
    const char *dev_path_h2c = DEV_PATH_H2C;
    const char *dev_path_c2h = DEV_PATH_C2H;
    const char *input_file = NULL;
    const char *output_file = NULL;
    const char *test_mode = "ST";
    int test_loop_count = 1;
    char c;
    while ((c = getopt(argc, argv, "d:D:i:o:m:c:h")) != -1)
    {
        switch (c)
        {
        case 'd':
            dev_path_h2c = optarg;
            break;
        case 'D':
            dev_path_c2h = optarg;
            break;
        case 'i':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'm':
            test_mode = optarg;
            break;
            ;
        case 'c':
            test_loop_count = strtol(optarg, NULL, 10);
            if (test_loop_count < 0 || test_loop_count > 10000)
            {
                printf("test_loop_count should 1 ~ 10000\n");
                exit(-1);
            }
            break;
        case 'h':
        case '?':
            printf(" -d H2C device\n -D C2H device\n -i Input file\n -o Output file\n -m Mode (ST/MM)\n -c Test Loop count\n");
            exit(0);
            break;
        }
    }

    printf("QDMA/XDMA simple Test \n");

    if (strcmp(test_mode, "ST") != 0 && strcmp(test_mode, "MM") != 0)
    {
        printf("Test mode should be ST/MM\n");
        exit(-1);
    }

    int dev_fd_c2h = open_dev(dev_path_c2h);
    int dev_fd_h2c = open_dev(dev_path_h2c);

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
    if (input_file)
    {
        int data_fd = open(input_file, O_RDONLY);
        if (data_fd == -1)
        {
            perror("Open input file");
            exit(errno);
        }
        int ret = read(data_fd, write_buf, WRITE_SIZE);
        printf("Load %d bytes from input file \n", ret);
    }
    else
    {
        memset(write_buf, 0x00, BUFF_SIZE);
    }

    if (strcmp(test_mode, "ST") == 0)
    {
        printf("Start Write %ld bytes to H2C ST \n", WRITE_SIZE);
    }
    else if (strcmp(test_mode, "MM") == 0)
    {
        printf("Start Write %ld bytes to AXI 0x%08lx. \n", WRITE_SIZE, AXI_BASE_ADDR);
    }

    int64_t total_write_bytes = 0;
    gettimeofday(&tval_before, NULL);

    // DMA Write 수행
    int write_loop_count = test_loop_count;
    while (write_loop_count--)
    {
        total_write_bytes += dma_rw(dev_fd_h2c, AXI_BASE_ADDR, write_buf, WRITE_SIZE, WRITE, test_mode);
    }

    // 시간차 계산
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);
    millisec = (tval_result.tv_sec * 1000) + (tval_result.tv_usec / 1000);
    printf("Total Write %ld bytes (%.02f MB) in %ld ms, %.04f MB/s \n", total_write_bytes, total_write_bytes / 1000000.f, millisec, (total_write_bytes / 1000000.f) / (millisec / 1000.f));

    // ---------------------------------------------  Write Test END ----------------------------------------------------------------------

    // ---------------------------------------------   Read Test ----------------------------------------------------------------------
    if (strcmp(test_mode, "ST") == 0)
    {
        printf("Start Read %ld bytes from C2H ST \n", WRITE_SIZE);
    }
    else if (strcmp(test_mode, "MM") == 0)
    {
        printf("Start Read %ld bytes from AXI 0x%08lx. \n", WRITE_SIZE, AXI_BASE_ADDR);
    }

    int64_t total_read_bytes = 0;
    gettimeofday(&tval_before, NULL);

    // DMA Read 수행
    int read_loop_count = test_loop_count;
    while (read_loop_count--)
    {
        total_read_bytes += dma_rw(dev_fd_c2h, AXI_BASE_ADDR, read_buf, READ_SIZE, READ, test_mode);
    }

    // 시간차 계산
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);
    millisec = (tval_result.tv_sec * 1000) + (tval_result.tv_usec / 1000);
    printf("Total Read %ld bytes (%.02f MB) in %ld ms, %.04f MB/s \n", total_read_bytes, total_read_bytes / 1000000.f, millisec, (total_read_bytes / 1000000.f) / (millisec / 1000.f));

    if (strcmp(test_mode, "MM") == 0)
    {
        if (memcmp_user(read_buf, write_buf, READ_SIZE) != 0)
        {
            printf("TX/RX Data not match!\n");
        }
        else
        {
            printf("TX/RX Data match! \n");
        }
    }

    if (output_file)
    {
        int output_file_fd = open(output_file, O_RDWR | O_TRUNC | O_CREAT, 0644);
        if (output_file_fd == -1)
        {
            perror("Output file");
        }
        else
        {
            int rv = write(output_file_fd, read_buf, READ_SIZE);
            printf("Write %d bytes to output file\n", rv);
        }
    }

    // ---------------------------------------------   Read Test END ----------------------------------------------------------------------

    close(dev_fd_c2h);
    close(dev_fd_h2c);
    free(read_buf);
    free(write_buf);

    exit(EXIT_SUCCESS);
}

unsigned int no_memcpy = 1;
int open_dev(const char *dev_path)
{
    int dev = open(dev_path, O_RDWR | O_SYNC);

    if (dev == -1)
    {
        perror(dev_path);
        exit(errno);
    }
    else
    {
        printf("Dev %s Opened\n", dev_path);
    }
    return dev;
}

/**
 * @brief dev_fd 의 AXI address <-> buffer 간 size만큼 전송
 * @param rw 0: read, 1: write
 * @return 읽은 바이트의 수. 실패시 -1
 */
int dma_rw(int dev_fd, off_t axi_base_addr, void *buffer, uint64_t size, enum RW_OPTS rw, const char *test_mode)
{

    int64_t buff_cursor = 0;
    bool isMemoryMapMode = strcmp(test_mode, "MM") == 0;
    while (buff_cursor < size)
    {
        if (isMemoryMapMode)
        {
            // 읽거나 쓴 만큼 뒤로 탐색
            int ret = lseek(dev_fd, axi_base_addr + buff_cursor, SEEK_SET);
            if (ret == -1)
            {
                perror("dev lseek() fail!\n");
                return -1;
            }
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
