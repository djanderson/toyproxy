set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(MAIN_SOURCES
  hashmap.c
  printl.c
  queue.c
  request.c
  response.c
  url.c
  webproxy.c
)

set(HEADERS
  hashmap.h
  printl.h
  queue.h
  request.h
  response.h
  url.h
)

add_executable(webproxy ${HEADERS} ${MAIN_SOURCES})
target_link_libraries(webproxy Threads::Threads)

install(TARGETS webproxy DESTINATION bin)
