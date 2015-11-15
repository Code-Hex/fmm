CC=gcc
CFLAGS=-O

snake: fmm.c fmm.h
	$(CC) $(CFLAGS) -o fmm fmm.c -lncurses
