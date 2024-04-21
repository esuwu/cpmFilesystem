# Define compiler
CC = gcc
# Define compiler flags, -I for include directories
CFLAGS = -Iinclude

all: cpmRun

cpmRun: diskSimulator.o  cpmfsys.o fsysdriver.o
	$(CC) -o cpmRun diskSimulator.o cpmfsys.o fsysdriver.o

diskSimulator.o: diskSimulator.c diskSimulator.h
	$(CC) -c diskSimulator.c

cpmfsys.o: cpmfsys.h cpmfsys.c 
	$(CC) -c cpmfsys.c

fsysdriver.o: fsysdriver.c
	$(CC) -c fsysdriver.c

clean: 
	rm *.o cpmRun

