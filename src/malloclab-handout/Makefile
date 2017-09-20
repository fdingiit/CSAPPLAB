#
# Students' Makefile for the Malloc Lab
#

# example:
# make strategy=USE_IMPLICIT fit=USE_FIRST_FIT

CC = clang
CFLAGS = -Wall -O2 -m32 -ggdb3 -D$(strategy) -D$(fit)

ifeq ($(strategy),USE_IMPLICIT)
	allocator=implicit
endif

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o $(allocator).o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

# allocators:
implicit.o: implicit.c implicit.h memlib.h utils.h

clean:
	rm -f *~ *.o mdriver


