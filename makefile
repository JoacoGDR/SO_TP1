CC=gcc
CFLAGS = -Wall -lpthread -lrt -g -std=c99 -D_XOPEN_SOURCE=700

all: solve view slave

solve: solve.c
	$(CC) solve.c $(CFLAGS) -o solve

view: view.c
	$(CC) view.c $(CFLAGS) -o view

slave: slave.c
	$(CC) slave.c $(CFLAGS) -o slave

.PHONY: clean

clean:
	rm slave solve view
