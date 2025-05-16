CC=gcc
CFLAGS=-O2 -Wall -lrt -lpthread
TARGET=dma_test
OBJS=dma_simple_test.o

dma_test: dma_simple_test.o
	$(CC) -o $@ $(OBJS) $(CFLAGS)

dma_simple_test.o: dma_simple_test.c user_config.h

clean:
	rm -f *.o
	rm -f $(TARGET)
