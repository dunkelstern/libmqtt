SRCS=encode_packet.c
OBJS=$(SRCS:%.c=%.o)
TARGETS=$(SRCS:%.c=%.test)

CC=clang
CFLAGS=-g -Os -Wall -I.. -I../src -I../platform -DDEBUG=1
# git -DTIMETRIAL
LDFLAGS=
LIBS=-L.. -lmqtt

all: $(TARGETS)

%.test: %.o cputime.o
	$(CC) $(LDFLAGS) -o $@ cputime.o $< $(LIBS)
	./$@
	rm $@

%.o: %.c test.h
	$(CC) $(CFLAGS) -o $@ -c $<

%.e: %.c test.h
	$(CC) $(CFLAGS) -E -o $@ -c $<
	less $@
	rm $@

clean:
	rm -f $(TARGETS)
	rm -f $(OBJS)
	rm -f *.e
