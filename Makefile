CC=gcc
CFLAGS=-Wall -Wextra -g -std=gnu11
LDFLAGS=-pthread -lssl -lcrypto

src = $(wildcard *.c)
obj = $(src:.c=.o)

webproxy: $(obj)
	$(CC) -o $@ $^ $(LDFLAGS)
	mkdir -p .cache

.PHONY: clean
clean:
	rm -f $(obj) webproxy
