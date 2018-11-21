# Webproxy - Simple HTTP Web Proxy

A simple HTTP static file webproxy supporting HTTP/1.1 keep-alive.

## Quickstart

```bash
$ mkdir build
$ cd build
$ cmake ../
$ make && make test
$ ./src/webproxy 10000
```

Use `--debug` flag to enable debug output.

## Implementation and file layout

Webproxy is a multithreaded HTTP proxy that implements a subset of HTTP/1.1. It includes a custom written url parser and hashmap caching implementation with unit tests. Request and response parsing is also unit tested. It implements multiple simultaneous requests, user-definable cache timeout, page and hostname caching, and URL and IP blacklisting. It does not implement link prefetching.

Directories:

 - [src](src) - The webproxy source files
 - [vendor](vendor) - Files for Unity, a small C unit testing framework
 - [tests](tests) - Unit tests for several of the fundamental data structures and parsing routines

Implementation Files:

 - [webproxy.c](webproxy.c) - "Configuration" defines (`CACHE_ROOT`, `BLACKLIST_FILE`, `KEEPALIVE_TIMEOUT`, ...), `main` function, proxy main loop, socket connection handling, etc
 - [url.h](url.h) - Url struct and related functions header
 - [url.c](url.c) - Url struct and related functions implementation
 - [hashmap.h](hashmap.h) - Hashmap struct and related functions header
 - [hashmap.c](hashmap.c) - Hashmap struct and related functions implementation
 - [request.h](request.h) - Request struct and related functions header
 - [request.c](request.c) - Request struct and related functions implementation
 - [response.h](response.h) - Response struct and related functions header
 - [response.c](response.c) - Response struct and related functions implementation
 - [printl.h](printl.h) - Printk-like logging function header
 - [printl.c](printl.c) - Printk-like logging function implementation
 - [queue.h](queue.h) - Thread-safe FIFO queue header (not currently used)
 - [queue.c](queue.c) - Thread-safe FIFO queue implementation (not currently used)

## Known Issues

 - [ ] HTTP/1.1 Keep-Alive is implemented, but HTTP/1.1 chunked transfer is not, therefore we sometimes send a request that we can't parse the response of
