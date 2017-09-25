#
# Students' Makefile for the Malloc Lab
#

ALLOCATOR =
STRATEGY = USE_IMPLICIT

# USE_FIRST_FIT
# USE_NEXT_FIT
# USE_BEST_FIT
FIT = USE_BEST_FIT

DEBUG = 0
DUMP = 1
PROFILE = 0

CC = clang
CFLAGS = -Wall

ifeq ($(compiler), clang)
	CC = clang
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -ggdb3 -DDEBUG

	ifeq ($(DUMP), 1)
    	CFLAGS += -DDUMP_HEAP
    endif
endif

CFLAGS += -D$(STRATEGY) -D$(FIT)

ifeq ($(PROFILE), 1)
	CFLAGS += -pg
endif

ifeq ($(STRATEGY), USE_IMPLICIT)
	ALLOCATOR=implicit
endif

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o $(ALLOCATOR).o

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


