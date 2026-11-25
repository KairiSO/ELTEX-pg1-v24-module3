CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99
SRCS    = main.c chat.c queue.c
OBJS    = $(SRCS:.c=.o)
TARGET  = chat

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c chat.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean