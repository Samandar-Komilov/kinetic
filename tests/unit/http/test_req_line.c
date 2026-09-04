#include "ktc/http/req_line.h"
#include "test_harness.h"

#include <string.h>

static void test_parse_success(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-001)", "Valid GET request line");
    const char *raw = "GET /index.html HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser completed consuming line");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "parser state is COMPLETE");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_NONE, "parser error is NONE");

    ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
    KTC_ASSERT(ktc_str_eq_cstr(parser.method, "GET"), "method parsed as GET");
    KTC_ASSERT(ktc_str_eq_cstr(parser.target, "/index.html"), "target parsed as /index.html");
    KTC_ASSERT(ktc_str_eq_cstr(parser.version, "HTTP/1.1"), "version parsed as HTTP/1.1");
}

static void test_parse_leading_crlf(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-006)", "Tolerance for empty leading CRLF lines");
    const char *raw = "\r\n\r\nPOST /submit HTTP/1.0\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished line");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "parser state is COMPLETE");

    ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
    KTC_ASSERT(ktc_str_eq_cstr(parser.method, "POST"), "method parsed as POST");
    KTC_ASSERT(ktc_str_eq_cstr(parser.target, "/submit"), "target parsed as /submit");
    KTC_ASSERT(ktc_str_eq_cstr(parser.version, "HTTP/1.0"), "version parsed as HTTP/1.0");
}

static void test_parse_multiple_spaces(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 / §3.2 (H11-PARSE-007)", "Reject multiple spaces in request line");
    const char *raw = "GET  / HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts immediately on syntax violation");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_uri_too_long(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-002)", "Reject request-target > 8192 bytes (414)");
    char raw[9000];
    memcpy(raw, "GET /", 5);
    memset(raw + 5, 'a', 8195);
    memcpy(raw + 8200, " HTTP/1.1\r\n", 11);
    raw[8211] = '\0';

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, 8211);
    KTC_ASSERT(!feeding, "parser halts on oversized URI");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_URI_TOO_LONG, "error set to URI_TOO_LONG");
}

static void test_parse_method_too_long(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-003)", "Reject method token > 32 bytes");
    const char *raw = "VERYLONGMETHODNAMEEXCEEDINGTHIRTYTWOCHARS / HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on oversized method");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_invalid_method_chars(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-001)", "Reject invalid token chars in method");
    const char *raw = "GE[T] / HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on non-token characters in method");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_invalid_version_crlf(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-003)", "Reject bare CR / invalid CRLF termination");
    const char *raw = "GET / HTTP/1.1\rX";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on invalid line ending");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_invalid_version_space(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-004)", "Reject trailing space after version");
    const char *raw = "GET / HTTP/1.1 \r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on unexpected trailing space");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_unsupported_version(void) {
    KTC_TEST_CASE("RFC 9110 §6.2 (H11-STATUS-004)", "Refuse non-HTTP/1.x version (505)");
    const char *raw = "GET / HTTP/2.0\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finishes line reading");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "parser state is COMPLETE");

    ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "verification flags version error");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED,
               "error set to VERSION_NOT_SUPPORTED");
}

static void test_parse_too_much_leading_junk(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-007)", "Reject excessive blank lines (> 20 CRLFs)");
    char raw[128];
    memset(raw, 0, sizeof(raw));
    size_t offset = 0;
    for (int i = 0; i < 22; i++) {
        memcpy(raw + offset, "\r\n", 2);
        offset += 2;
    }
    const char *req = "GET / HTTP/1.1\r\n";
    size_t req_len = strlen(req);
    memcpy(raw + offset, req, req_len);
    raw[offset + req_len] = '\0';

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on excessive blank line spam");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_incremental(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (FSM Streaming)", "Incremental feed across split TCP packets");
    const char *part1 = "GET /index";
    const char *part2 = ".html HT";
    const char *part3 = "TP/1.1\r\n";

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    char buf[128];
    size_t len1 = strlen(part1);
    size_t len2 = strlen(part2);
    size_t len3 = strlen(part3);

    memcpy(buf, part1, len1);
    memcpy(buf + len1, part2, len2);
    memcpy(buf + len1 + len2, part3, len3);
    buf[len1 + len2 + len3] = '\0';

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)part1, len1);
    KTC_ASSERT(feeding, "chunk 1 requires more data");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_TARGET, "state in TARGET");

    feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)part2, len2);
    KTC_ASSERT(feeding, "chunk 2 requires more data");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_VERSION, "state in VERSION");

    feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)part3, len3);
    KTC_ASSERT(!feeding, "chunk 3 completes line");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "state in COMPLETE");

    ktc_req_line_parser_verify(&parser, (const uint8_t *)buf);
    KTC_ASSERT(ktc_str_eq_cstr(parser.method, "GET"), "reconstructed method matches GET");
    KTC_ASSERT(ktc_str_eq_cstr(parser.target, "/index.html"),
               "reconstructed target matches /index.html");
    KTC_ASSERT(ktc_str_eq_cstr(parser.version, "HTTP/1.1"),
               "reconstructed version matches HTTP/1.1");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 2.2: RFC 9112 Request Line Parser");
    test_parse_success();
    test_parse_leading_crlf();
    test_parse_multiple_spaces();
    test_parse_uri_too_long();
    test_parse_method_too_long();
    test_parse_invalid_method_chars();
    test_parse_invalid_version_crlf();
    test_parse_invalid_version_space();
    test_parse_unsupported_version();
    test_parse_too_much_leading_junk();
    test_parse_incremental();
    KTC_TEST_SUITE_END();
}
