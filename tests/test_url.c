#include <stdlib.h>

#include "../vendor/unity/unity.h"

#include "../src/url.h"


url_t url;


void setUp()
{
    /* Nothing to do */
}


void tearDown()
{
    url_destroy(&url);
}


void test_minimal_valid_url()
{
    int rval = url_init(&url, "example.com");

    TEST_ASSERT_EQUAL_INT(0, rval);
    TEST_ASSERT_EQUAL_STRING("example.com", url.full);
    TEST_ASSERT_EQUAL_STRING("http", url.scheme);
    TEST_ASSERT_EQUAL_STRING("example.com", url.host);
    TEST_ASSERT_NULL(url.ip);
    TEST_ASSERT_EQUAL_UINT(80, url.port);
    TEST_ASSERT_EQUAL_STRING("/", url.path);
    TEST_ASSERT_NULL(url.error);
}


void test_minimal_url_with_port()
{
    int rval = url_init(&url, "example.com:8000");

    TEST_ASSERT_EQUAL_INT(rval, 0);
    TEST_ASSERT_EQUAL_STRING("example.com:8000", url.full);
    TEST_ASSERT_EQUAL_STRING("http", url.scheme);
    TEST_ASSERT_EQUAL_STRING("example.com", url.host);
    TEST_ASSERT_NULL(url.ip);
    TEST_ASSERT_EQUAL_UINT(8000, url.port);
    TEST_ASSERT_EQUAL_STRING("/", url.path);
    TEST_ASSERT_NULL(url.error);
}


void test_full_url()
{
    const char full[] = "http://www.example.com:8080/path/to/resource.html";

    int rval = url_init(&url, full);

    TEST_ASSERT_EQUAL_INT(0, rval);
    TEST_ASSERT_EQUAL_STRING(full, url.full);
    TEST_ASSERT_EQUAL_STRING("http", url.scheme);
    TEST_ASSERT_EQUAL_STRING("www.example.com", url.host);
    TEST_ASSERT_NULL(url.ip);
    TEST_ASSERT_EQUAL_UINT(8080, url.port);
    TEST_ASSERT_EQUAL_STRING("/path/to/resource.html", url.path);
    TEST_ASSERT_NULL(url.error);
}


void test_url_invalid_port()
{
    int rval = url_init(&url, "example.com:abc");

    TEST_ASSERT_EQUAL_INT(-1, rval);
    TEST_ASSERT_EQUAL_STRING("Invalid port `abc'", url.error);
}


void test_url_invalid_path()
{
    int rval = url_init(&url, "example.com/../secrets");

    TEST_ASSERT_EQUAL_INT(-1, rval);
    TEST_ASSERT_EQUAL_STRING("Invalid path includes `/../'", url.error);
}


/* NOTE: toyproxy only supports http, but this test can be removed later */
void test_url_scheme_not_http()
{
    int rval = url_init(&url, "https://example.com");

    TEST_ASSERT_EQUAL_INT(-1, rval);
    TEST_ASSERT_EQUAL_STRING("Invalid scheme `https' - use http", url.error);
}


int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_minimal_valid_url);
    RUN_TEST(test_minimal_url_with_port);
    RUN_TEST(test_full_url);
    RUN_TEST(test_url_invalid_port);
    RUN_TEST(test_url_invalid_path);
    RUN_TEST(test_url_scheme_not_http);

    return UNITY_END();
}
