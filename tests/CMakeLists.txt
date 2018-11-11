set(TEST_SOURCES
  ../src/url.c
  test_url.c
)

add_executable(test_url ${TEST_SOURCES})

target_link_libraries(test_url unity)

add_test(test_url test_url)
