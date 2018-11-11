#include <stdlib.h>

#include "../vendor/unity/unity.h"

#include "../src/url.h"


void test_minimal_valid()
{
    int rval;
    url_t url;

    rval = url_init(&url, "google.com");

    TEST_ASSERT_EQUAL_INT(rval, 0);
    TEST_ASSERT_EQUAL_STRING(url.full, "google.com");
    TEST_ASSERT_EQUAL_STRING(url.scheme, "http");
    TEST_ASSERT_EQUAL_STRING(url.host, "google.com");
    TEST_ASSERT_NULL(url.ip);
    TEST_ASSERT_EQUAL_UINT(url.port, 80);
    TEST_ASSERT_EQUAL_STRING(url.path, "/");
    TEST_ASSERT_NULL(url.error);

    url_destroy(&url);
}


int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_minimal_valid);

    return UNITY_END();
}
