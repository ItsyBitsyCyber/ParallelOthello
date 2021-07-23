#COMPILER ?= mpicc
#-DDEBUG
COMPILER ?= mpicc

CFLAGS ?= -O2 -g -Wall -Wno-variadic-macros -pedantic $(GCC_SUPPFLAGS)
LDFLAGS ?= -g 
LDLIBS =

EXECUTABLE = player/main

SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:src/%.c=player/%.o)

all: release

release: $(OBJS)
	$(COMPILER) $(LDFLAGS) -o $(EXECUTABLE) $(OBJS) $(LDLIBS) 

player/%.o: src/%.c | player
	$(COMPILER) $(CFLAGS) -o $@ -c $<

player:
	mkdir -p $@

clean:
	rm -f player/*.o
	rm ${EXECUTABLE} 

cleandata:
	rm white*.txt
	rm black*.txt
