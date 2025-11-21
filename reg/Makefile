# Basira Daqiq
# Steven Correa
# Makefile for P2P Networking Project

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -g

# Target executable
PEER_TARGET = peer

# Source file
PEER_SRC = peer.c

# Default target
all: $(PEER_TARGET)

# Build peer executable
$(PEER_TARGET): $(PEER_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Clean build artifacts
clean:
	rm -f $(PEER_TARGET)

# Phony targets
.PHONY: all clean
