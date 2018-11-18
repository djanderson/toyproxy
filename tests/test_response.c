#include "../vendor/unity/unity.h"

#include "../src/response.h"


response_t res;
char *test_raw_response, *test_raw_response_saveptr;

const int response_length = 180;
const char raw_response[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Tue, 13 Nov 2018 05:01:00 GMT\r\n"
    "Server: Apache\r\n"
    "Content-Length: 39\r\n"
    "Connection: Keep-Alive\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<html><body><h1>Test</h1></body></html>";


void setUp()
{
    response_init(&res);
    test_raw_response = strdup(raw_response);
    test_raw_response_saveptr = test_raw_response; /* hold for free */
}


void tearDown()
{
    free(test_raw_response_saveptr);
    response_destroy(&res);
}


static void verify_response()
{
    char *date, *server, *clen, *conn, *ctype;

    TEST_ASSERT_TRUE(res.header.complete);
    TEST_ASSERT_EQUAL_STRING("HTTP/1.1 200 OK", res.header.status_line);

    hashmap_get(&res.header.fields, "Date", &date);
    TEST_ASSERT_EQUAL_STRING("Tue, 13 Nov 2018 05:01:00 GMT", date);
    free(date);

    hashmap_get(&res.header.fields, "Server", &server);
    TEST_ASSERT_EQUAL_STRING("Apache", server);
    free(server);

    hashmap_get(&res.header.fields, "Content-Length", &clen);
    TEST_ASSERT_EQUAL(39, atoi(clen));
    free(clen);

    hashmap_get(&res.header.fields, "Connection", &conn);
    TEST_ASSERT_EQUAL_STRING("Keep-Alive", conn);
    free(conn);

    hashmap_get(&res.header.fields, "Content-Type", &ctype);
    TEST_ASSERT_EQUAL_STRING("text/html", ctype);
    free(ctype);

    TEST_ASSERT_TRUE(res.complete);
    TEST_ASSERT_EQUAL_STRING("<html><body><h1>Test</h1></body></html>",
                             res.content);
}


/* Test that all information is parsed when full message is passed at once. */
void test_response_deserialize_whole()
{
    int nunparsed;

    nunparsed = response_deserialize(&res, test_raw_response, response_length);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);
    verify_response();
}


/* Test the response_ok helper function. */
void test_response_ok()
{
    response_deserialize(&res, test_raw_response, response_length);
    TEST_ASSERT_TRUE(response_ok(&res));
}


/* Test that parse of response split at partial header line succeeds. */
void test_split_response_partial_header_line()
{

}


/* Test that parse of response split at full header line succeeds. */
void test_split_response_full_header_line()
{

}


/* Test that parse of response split at end of complete header succeeds. */
void test_split_response_complete_header()
{

}



/* Test that parse of response split at partial content succeeds. */
void test_split_response_partial_content()
{

}



int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_response_deserialize_whole);
    RUN_TEST(test_response_ok);

    return UNITY_END();
}
