# Toyproxy - Simple HTTP Web Proxy [![Travis CI Build Status][travis-badge]][travis-link]

  [travis-link]: https://travis-ci.org/djanderson/toyproxy
  [travis-badge]: https://travis-ci.org/djanderson/toyproxy.svg?branch=master

A simple caching HTTP-only webproxy supporting HTTP/1.1. This project was
written for ECEN 5273 - Network Systems at the University of Colorado -
Boulder. This is a toy project and shouldn't be used for anything serious, but
was the first C project I ever wrote that got big enough to where I needed unit
tests to keep from getting overwhelmed.

## Quickstart

```bash
$ mkdir build
$ cd build
$ cmake ../
$ make && make test
$ ./src/toyproxy 10000  # start on port 10000
```

Use `--debug` flag to enable debug output.

## Implementation and file layout

Toyproxy is a multithreaded HTTP proxy that implements a subset of HTTP/1.1. It
includes a custom written url parser and hashmap caching implementation with
unit tests. Request and response parsing is also unit tested. It implements
multiple simultaneous requests, user-definable cache timeout, page and hostname
caching, and URL and IP blacklisting.

Directories:

 - [src](src) - The toyproxy source files
 - [vendor](vendor) - Files for Unity, a small C unit testing framework
 - [tests](tests) - Unit tests for several of the fundamental data structures and parsing routines

Implementation Files:

 - [toyproxy.c](src/toyproxy.c) - "Configuration" defines (`CACHE_ROOT`, `BLACKLIST_FILE`, `KEEPALIVE_TIMEOUT`, ...), `main` function, proxy main loop, socket connection handling, etc
 - [url.h](src/url.h) - Url struct and related functions header
 - [url.c](src/url.c) - Url struct and related functions implementation
 - [hashmap.h](src/hashmap.h) - Hashmap struct and related functions header
 - [hashmap.c](src/hashmap.c) - Hashmap struct and related functions implementation
 - [request.h](src/request.h) - Request struct and related functions header
 - [request.c](src/request.c) - Request struct and related functions implementation
 - [response.h](src/response.h) - Response struct and related functions header
 - [response.c](src/response.c) - Response struct and related functions implementation
 - [printl.h](src/printl.h) - Printk-like logging function header
 - [printl.c](src/printl.c) - Printk-like logging function implementation
 - [queue.h](src/queue.h) - Thread-safe FIFO queue header (not currently used)
 - [queue.c](src/queue.c) - Thread-safe FIFO queue implementation (not currently used)


## Licence

Copyright 2018 Douglas Anderson <douglas.anderson-1@colorado.edu>. Released
under GPL 3.
