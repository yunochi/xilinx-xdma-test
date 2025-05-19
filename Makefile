CC=gcc
CFLAGS=-O2 -Wall -lrt -lpthread
TARGET=dma_test config_bar
OBJS=dma_simple_test.o


all: dma_test config_bar

dma_test: dma_simple_test.o
	$(CC) -o $@ $(OBJS) $(CFLAGS)

config_bar: config_bar_test.o
	$(CC) -o $@ config_bar_test.o $(CFLAGS)

dma_simple_test.o: dma_simple_test.c user_config.h
config_bar_test.o: config_bar_test.c user_config.h

clean:
	rm -f *.o
	rm -f $(TARGET)
