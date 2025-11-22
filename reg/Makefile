CC = gcc
CFLAGS = -Wall -std=c99
TARGET = registry

all: $(TARGET)

$(TARGET): registry.c
	$(CC) $(CFLAGS) -o $(TARGET) registry.c

clean:
	rm -f $(TARGET)
