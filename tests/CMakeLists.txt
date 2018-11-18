set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(test_url ../src/url.c test_url.c)
add_executable(test_hashmap ../src/hashmap.c ../src/printl.c test_hashmap.c)
add_executable(test_response
  ../src/response.c
  ../src/printl.c
  ../src/hashmap.c
  test_response.c)

target_link_libraries(test_url unity)
target_link_libraries(test_hashmap unity Threads::Threads)
target_link_libraries(test_response unity Threads::Threads)

add_test(test_url test_url)
add_test(test_hashmap test_hashmap)
add_test(test_response test_response)
