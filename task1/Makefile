CC      = gcc
CFLAGS  = -Wall -Wextra -g
TARGET  = file_copier
OBJS    = main.o ipc.o worker.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET) *.copy *.in *.out

.PHONY: all clean