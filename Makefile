CC = gcc
CFLAGS = -std=c99 -g -Wall -fsanitize=address,undefined -pthread

all: ttt ttts

ttt: ttt.o protocol.o arraylist.o
	$(CC) $(CFLAGS) $^ -o $@

ttts: ttts.o protocol.o arraylist.o
	$(CC) $(CFLAGS) $^ -o $@

ttts.o ttt.o protocol.o arraylist.o: protocol.h arraylist.h

protocol-dev.o: protocol.c protocol.h
	$(CC) $(CFLAGS) -DSAFE -DDEBUG=2 $< -o $@
	
arraylist-dev.o: arraylist.c arraylist.h
	$(CC) $(CFLAGS) -DSAFE -DDEBUG=2 $< -o $@

clean:
	rm -rf *.o ttt ttts