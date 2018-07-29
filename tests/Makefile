SRCS=encode_packet.c decode_packet.c connect_publish.c connect_subscribe.c
OBJS=$(SRCS:%.c=%.o)
COVERAGE_FILES=$(SRCS:%.c=%.gcno) $(SRCS:%.c=%.gcda)
TARGETS=$(SRCS:%.c=%.test)

COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage

CC=gcc
CFLAGS=-g -Os -Wall -I.. -I../src -I../platform -DDEBUG=1 $(COVERAGE_FLAGS)
# -DTIMETRIAL
LDFLAGS=
LIBS=-L.. -lmqtt-debug -lpthread

all: $(TARGETS)

%.test: %.o cputime.o
	$(CC) $(COVERAGE_FLAGS) $(LDFLAGS) -o $@ cputime.o $< $(LIBS)
	# ./$@
	# rm $@

%.o: %.c test.h
	$(CC) $(CFLAGS) -o $@ -c $<

%.e: %.c test.h
	$(CC) $(CFLAGS) -E -o $@ -c $<
	less $@
	rm $@

clean:
	rm -f $(TARGETS)
	rm -f $(OBJS)
	rm -f $(COVERAGE_FILES)
	rm -f *.e
