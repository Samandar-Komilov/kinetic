#include "ktc/http/response.h"
#include "test_harness.h"

#include <string.h>

static void test_response_format_200(void) {
    KTC_TEST_CASE("RFC 9112 §4 (H11-STATUS-001)", "Format 200 OK status line and headers");
    char dest[1024];
    size_t len = ktc_response_format_empty(dest, sizeof(dest), 200, "OK");

    KTC_ASSERT(len > 0, "response formatted successfully");
    KTC_ASSERT(strstr(dest, "HTTP/1.1 200 OK\r\n") == dest, "starts with HTTP/1.1 200 OK");
    KTC_ASSERT(strstr(dest, "Content-Length: 0\r\n") != NULL, "contains Content-Length: 0");
    KTC_ASSERT(strstr(dest, "Server: kinetic\r\n") != NULL, "contains Server: kinetic");
    KTC_ASSERT(strstr(dest, "Date: ") != NULL, "contains Date header");
    KTC_ASSERT(strstr(dest, "\r\n\r\n") != NULL, "contains terminating double CRLF");
}

static void test_response_connection_close(void) {
    KTC_TEST_CASE("RFC 9112 §9.3 (H11-CONN-002)",
                  "Send Connection: close in single request/response cycle");
    char dest[1024];
    size_t len = ktc_response_format_empty(dest, sizeof(dest), 200, "OK");

    KTC_ASSERT(len > 0, "response formatted");
    KTC_ASSERT(strstr(dest, "Connection: close\r\n") != NULL, "contains Connection: close header");
}

static void test_response_no_bare_cr(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-002)", "No bare CR in generated protocol elements");
    char dest[1024];
    size_t len = ktc_response_format_empty(dest, sizeof(dest), 400, "Bad Request");

    KTC_ASSERT(len > 0, "response formatted");

    bool has_bare_cr = false;
    for (size_t i = 0; i < len; i++) {
        if (dest[i] == '\r') {
            if (i + 1 >= len || dest[i + 1] != '\n') {
                has_bare_cr = true;
                break;
            }
        }
    }
    KTC_ASSERT(!has_bare_cr, "generated response contains no bare CR");

    bool has_bare_lf = false;
    for (size_t i = 0; i < len; i++) {
        if (dest[i] == '\n') {
            if (i == 0 || dest[i - 1] != '\r') {
                has_bare_lf = true;
                break;
            }
        }
    }
    KTC_ASSERT(!has_bare_lf, "generated response contains no bare LF");
}

static void test_response_buffer_too_small(void) {
    KTC_TEST_CASE("RFC 9112 §4 (Defensive)", "Buffer overflow safety when dest_len is too small");
    char dest[16];
    size_t len = ktc_response_format_empty(dest, sizeof(dest), 200, "OK");

    KTC_ASSERT(len == 0, "returns 0 safely when buffer cannot fit response");
}

static void test_response_various_statuses(void) {
    KTC_TEST_CASE("RFC 9112 §4 (Status Lines)", "Format various standard HTTP status codes");
    char dest[1024];

    size_t len = ktc_response_format_empty(dest, sizeof(dest), 414, "URI Too Long");
    KTC_ASSERT(len > 0 && strstr(dest, "HTTP/1.1 414 URI Too Long\r\n") == dest,
               "formats 414 URI Too Long");

    len = ktc_response_format_empty(dest, sizeof(dest), 431, "Request Header Fields Too Large");
    KTC_ASSERT(len > 0 && strstr(dest, "HTTP/1.1 431 Request Header Fields Too Large\r\n") == dest,
               "formats 431 Fields Too Large");

    len = ktc_response_format_empty(dest, sizeof(dest), 501, "Not Implemented");
    KTC_ASSERT(len > 0 && strstr(dest, "HTTP/1.1 501 Not Implemented\r\n") == dest,
               "formats 501 Not Implemented");

    len = ktc_response_format_empty(dest, sizeof(dest), 505, "HTTP Version Not Supported");
    KTC_ASSERT(len > 0 && strstr(dest, "HTTP/1.1 505 HTTP Version Not Supported\r\n") == dest,
               "formats 505 Version Not Supported");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 1.1 & 2.1 & Phase 3: Response Generation & Line Endings");
    test_response_format_200();
    test_response_connection_close();
    test_response_no_bare_cr();
    test_response_buffer_too_small();
    test_response_various_statuses();
    KTC_TEST_SUITE_END();
}
