#include "../vendor/unity/unity.h"

#include "../src/response.h"


response_t res;
char test_raw_response[206];

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

const int chunked_response_length = 205;
const char chunked_raw_response[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Tue, 13 Nov 2018 05:01:00 GMT\r\n"
    "Server: Apache\r\n"
    "Connection: Keep-Alive\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "14\r\n"
    "<html><body><h1>Test\r\n"
    "13\r\n"
    "</h1></body></html>\r\n"
    "0\r\n"
    "\r\n";


void setUp()
{
    memset(test_raw_response, 0, sizeof(test_raw_response));
    response_init(&res);
}


void tearDown()
{
    response_destroy(&res);
}


static void verify_response()
{
    char *date, *server, *clen, *conn, *ctype;

    TEST_ASSERT_TRUE_MESSAGE(res.header.complete, "Header not complete");
    TEST_ASSERT_EQUAL_STRING("HTTP/1.1 200 OK", res.header.status_line);

    hashmap_get(&res.header.fields, "Date", &date);
    TEST_ASSERT_EQUAL_STRING("Tue, 13 Nov 2018 05:01:00 GMT", date);
    free(date);

    hashmap_get(&res.header.fields, "Server", &server);
    TEST_ASSERT_EQUAL_STRING("Apache", server);
    free(server);

    hashmap_get(&res.header.fields, "Content-Length", &clen);
    TEST_ASSERT_EQUAL_INT(39, atoi(clen));
    free(clen);

    hashmap_get(&res.header.fields, "Connection", &conn);
    TEST_ASSERT_EQUAL_STRING("Keep-Alive", conn);
    free(conn);

    hashmap_get(&res.header.fields, "Content-Type", &ctype);
    TEST_ASSERT_EQUAL_STRING("text/html", ctype);
    free(ctype);

    TEST_ASSERT_TRUE_MESSAGE(res.complete, "Response not complete");
    TEST_ASSERT_EQUAL_STRING(raw_response, res.raw);
    TEST_ASSERT_EQUAL_STRING("<html><body><h1>Test</h1></body></html>",
                             res.content);
}


static void verify_chunked_response()
{
    char *date, *server, *tenc, *conn, *ctype;

    TEST_ASSERT_TRUE_MESSAGE(res.header.complete, "Header not complete");
    TEST_ASSERT_EQUAL_STRING("HTTP/1.1 200 OK", res.header.status_line);

    hashmap_get(&res.header.fields, "Date", &date);
    TEST_ASSERT_EQUAL_STRING("Tue, 13 Nov 2018 05:01:00 GMT", date);
    free(date);

    hashmap_get(&res.header.fields, "Server", &server);
    TEST_ASSERT_EQUAL_STRING("Apache", server);
    free(server);

    hashmap_get(&res.header.fields, "Transfer-Encoding", &tenc);
    TEST_ASSERT_EQUAL_STRING("chunked", tenc);
    free(tenc);

    hashmap_get(&res.header.fields, "Connection", &conn);
    TEST_ASSERT_EQUAL_STRING("Keep-Alive", conn);
    free(conn);

    hashmap_get(&res.header.fields, "Content-Type", &ctype);
    TEST_ASSERT_EQUAL_STRING("text/html", ctype);
    free(ctype);

    TEST_ASSERT_TRUE_MESSAGE(res.complete, "Response not complete");
    TEST_ASSERT_EQUAL_STRING(chunked_raw_response, res.raw);
    TEST_ASSERT_EQUAL_STRING("14\r\n<html><body><h1>Test\r\n13\r\n</h1></body>"
                             "</html>\r\n0\r\n\r\n", res.content);
}


/* Test the response_ok helper function. */
void test_response_ok()
{
    strcpy(test_raw_response, raw_response);
    response_deserialize(&res, test_raw_response, response_length);
    TEST_ASSERT_TRUE(response_ok(&res));
}


/* Test the response_chunked helper function. */
void test_response_chunked()
{
    strcpy(test_raw_response, chunked_raw_response);
    response_deserialize(&res, test_raw_response, response_length);
    TEST_ASSERT_TRUE(response_chunked(&res));
}


/* Test that all information is parsed when full message is passed at once. */
void test_response_deserialize_whole()
{
    int nunparsed;

    strcpy(test_raw_response, raw_response);
    nunparsed = response_deserialize(&res, test_raw_response, response_length);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);
    verify_response();
}


/* Test that all information is parsed when full message is passed at once. */
void test_response_deserialize_whole_chunked()
{
    int nunparsed;

    strcpy(test_raw_response, chunked_raw_response);
    nunparsed = response_deserialize(&res, test_raw_response,
                                     chunked_response_length);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);
    verify_chunked_response();
}


/* Test that parse of response split at partial header line succeeds. */
void test_split_response_partial_header_line()
{
    int nunparsed;

    /* Copy up to "...Server: Apache\r\nContent-Le" */
    strncpy(test_raw_response, raw_response, 80);
    nunparsed = response_deserialize(&res, test_raw_response, 80);
    TEST_ASSERT_EQUAL_INT(10, nunparsed);
    TEST_ASSERT_EQUAL_STRING("Content-Le", test_raw_response);

    /* Place the rest of the response in the buffer */
    strcat(test_raw_response, &raw_response[80]);
    nunparsed = response_deserialize(&res, test_raw_response, 100 + nunparsed);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    verify_response();
}


/* Test that parse of response split at full header line succeeds. */
void test_split_response_full_header_line()
{
    int nunparsed;

    /* Copy up to "...Content-Length: 39\r\n" */
    strncpy(test_raw_response, raw_response, 90);
    nunparsed = response_deserialize(&res, test_raw_response, 90);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    /* Place the rest of the response in the buffer */
    strcat(test_raw_response, &raw_response[90]);
    nunparsed = response_deserialize(&res, test_raw_response, 90);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    verify_response();
}


/* Test that parse of response split at end of complete header succeeds. */
void test_split_response_complete_header()
{
    int nunparsed;

    /* Copy up to "...Content-Type: text/html\r\n\r\n" */
    strncpy(test_raw_response, raw_response, 141);
    nunparsed = response_deserialize(&res, test_raw_response, 141);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    /* Place the rest of the response in the buffer */
    strcat(test_raw_response, &raw_response[141]);
    nunparsed = response_deserialize(&res, test_raw_response, 39);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    verify_response();
}



/* Test that parse of response split at partial content succeeds. */
void test_split_response_partial_content()
{
    int nunparsed;

    /* Copy up to "...Content-Type: text/html\r\n\r\n" */
    strncpy(test_raw_response, raw_response, 157);
    nunparsed = response_deserialize(&res, test_raw_response, 157);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    /* Place the rest of the response in the buffer */
    strcat(test_raw_response, &raw_response[157]);
    nunparsed = response_deserialize(&res, test_raw_response, 23);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    verify_response();
}


int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_response_ok);
    RUN_TEST(test_response_chunked);
    RUN_TEST(test_response_deserialize_whole);
    RUN_TEST(test_response_deserialize_whole_chunked);
    RUN_TEST(test_split_response_partial_header_line);
    RUN_TEST(test_split_response_full_header_line);
    RUN_TEST(test_split_response_complete_header);
    RUN_TEST(test_split_response_partial_content);

    return UNITY_END();
}
