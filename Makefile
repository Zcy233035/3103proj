CC=gcc
CFLAGS=-g -lm

problem1 : problem1.o helpers.o
	$(CC) -o problem1 helpers.o problem1.o -pthread

.PHONY : clean

clean : 
	rm *.o $(objects) problem1