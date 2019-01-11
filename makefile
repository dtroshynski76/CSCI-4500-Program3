CFLAGS = -Wall -ansi -pedantic
.PHONY: all, clean

objects = prog3.o
program = myprog3

%.o : %.c
	   gcc -c $(CFLAGS) $<

all: $(objects)
	   gcc -o $(program) $(objects)


clean:
	   rm *.o $(program)
