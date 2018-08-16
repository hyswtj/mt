CC=gcc

CFLAGS = -Wall -march=native -g -m64 -pthread

SRCS=\
main.c \
tests.c \
tests_compression.c \
tests_decompression.c

COVERAGE=-lstdc++ -lc -ldl -lz -Wl,-lrt -Wl,-lm -Wl,-ldl -lpthread
OBJS = $(SRCS:%.c=%.o)

TARGET = mt_perf
all: $(TARGET)

$(TARGET): $(OBJS) Makefile
	$(CC) -o $(TARGET) $(OBJS) $(COVERAGE)

clean:
	rm -f $(OBJS) $(TARGET)
