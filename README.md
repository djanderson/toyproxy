# Webproxy - Simple HTTP Web Proxy

A simple HTTP static file webproxy supporting HTTP\1.1 keep-alive.

## Quickstart

```bash
$ make
$ ./webproxy 8888
```

Use `--debug` flag to enable debug output.

## Implementation and file layout

Webproxy uses a simple thread pool implementation. By default the thread pool is the number of cores (the same as the output of the `nproc` command on linux), but not more than `MAX_THREADS`, which is currently set at 15. Each thread is passed a thread-safe FIFO queue reference. The main thread takes incoming connection requests and places them on the queue, and the worker threads get connections from the queue when available in the producer-consumer model. If the request specifies `Connection: keep-alive` or if it is a HTTP\1.1 request and does _not_ specify `Connection: close`, then the worker thread waits `TIMEOUT` seconds before closing the connection. At the lowest level, the worker thread uses `pselect` with a timeout of 1 second and checks the status of the `exit_requested` flag, which may be set to `true` by the SIGINT signal handler; looping `TIMEOUT` times before closing the socket.

Directories:

 - [www](www) - The static file root directory

Files:

 - [webproxy.c](webproxy.c) - "Configuration" defines (`PROXY_ROOT`, `MAX_THREADS`, `TIMEOUT`, ...), `main` function, serve main loop, socket connection handling, thread pool handling, etc
 - [connectionq.h](connectionq.h) - Thread-safe FIFO queue header
 - [connectionq.c](connectionq.c) - Thread-safe FIFO queue implementation
 - [request.h](request.h) - Request struct and related functions header
 - [request.c](request.c) - Request struct and related functions implementation
 - [response.h](response.h) - Response struct and related functions header
 - [response.c](response.c) - Response struct and related functions implementation
 - [printl.h](printl.h) - Printk-like logging function header
 - [printl.c](printl.c) - Printk-like logging function implementation

## TODO

 - [X] Don't allow '..' in request uri               (security)
 - [X] Properly handle ctrl-c                        (required)
 - [X] If request is HTTP/1.0, respond with HTTP/1.0 (required)
 - [X] Add HTTP/1.1 keep-alive support               (10 pts bonus)
 - [ ] Add POST support for html files               (10 pts bonus)
