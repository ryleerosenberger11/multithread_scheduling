# Makefile for mts.c, CSC360 p2

# Default target
all: mts

# build the target
mts: mts.c
	gcc mts.c -o mts -lpthread

# Clean target
clean:
	rm -f mts
