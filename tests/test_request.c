#include <arpa/inet.h>          /* inet_addr */

#include "../vendor/unity/unity.h"

#include "../src/request.h"

#define TEST_IP "127.0.0.1"
#define TEST_FD 5

request_t req;
char test_raw_request[306];

struct sockaddr_in addr;

const int request_length = 305;
const char raw_request[] =
    "GET http://ecee.colorado.edu/~mathys/ecen4242/ HTTP/1.1\r\n"
    "Host: ecee.colorado.edu\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;\r\n"
    "Accept-Language: en-US,en;q=0.5\r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "DNT: 1\r\n"
    "Connection: keep-alive\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "Cache-Control: max-age=0\r\n"
    "\r\n";


void setUp()
{
    addr.sin_addr.s_addr = inet_addr(TEST_IP);
    memset(test_raw_request, 0, 306);
    request_init(&req, TEST_FD, &addr);
}


void tearDown()
{
    request_destroy(&req);
}


void verify_request()
{
    TEST_ASSERT_TRUE_MESSAGE(req.complete, "Request not complete");

    TEST_ASSERT_EQUAL_STRING(TEST_IP, req.ip);
    TEST_ASSERT_EQUAL_STRING("GET", req.method);
    TEST_ASSERT_EQUAL_STRING("HTTP/1.1", req.http_version);
    TEST_ASSERT_EQUAL_STRING("keep-alive", req.connection);
    TEST_ASSERT_EQUAL_STRING(raw_request, req.raw);
    TEST_ASSERT_EQUAL_INT(request_length, req.raw_len);
}


/* Test that all information is parsed when full message is passed at once. */
void test_request_deserialize_whole()
{
    int nunparsed;

    strcpy(test_raw_request, raw_request);
    nunparsed = request_deserialize(&req, test_raw_request, request_length);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);
    verify_request();
}


void test_split_request_partial_header_line()
{
    int nunparsed;

    /* Copy up to "...Accept-Encod" */
    strncpy(test_raw_request, raw_request, 195);
    nunparsed = request_deserialize(&req, test_raw_request, 195);
    TEST_ASSERT_EQUAL_INT(12, nunparsed);
    TEST_ASSERT_EQUAL_STRING("Accept-Encod", test_raw_request);

    /* Place the reqt of the request in the buffer */
    strcat(test_raw_request, &raw_request[195]);
    nunparsed = request_deserialize(&req, test_raw_request, 110 + nunparsed);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    verify_request();
}


void test_split_request_full_header_line()
{
    int nunparsed;

    /* Copy up to "...Accept-Encoding: gzip, deflate\r\n" */
    strncpy(test_raw_request, raw_request, 215);
    nunparsed = request_deserialize(&req, test_raw_request, 215);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);
    TEST_ASSERT_EQUAL_STRING("", test_raw_request);

    /* Place the reqt of the request in the buffer */
    strcat(test_raw_request, &raw_request[215]);
    nunparsed = request_deserialize(&req, test_raw_request, 90);
    TEST_ASSERT_EQUAL_INT(0, nunparsed);

    verify_request();
}


int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_request_deserialize_whole);
    RUN_TEST(test_split_request_partial_header_line);
    RUN_TEST(test_split_request_full_header_line);

    return UNITY_END();
}
