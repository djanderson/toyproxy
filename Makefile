CC=gcc
CFLAGS=-Wall -Wextra -g -std=gnu11
LDFLAGS=-pthread

src = $(wildcard *.c)
obj = $(src:.c=.o)

webproxy: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) webproxy
