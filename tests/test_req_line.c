#include "ktc/http/req_line.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_success(void) {
    const char *raw = "GET /index.html HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding); // Finished parsing
    assert(parser.state == KTC_REQ_LINE_STATE_COMPLETE);
    assert(parser.error == KTC_REQ_LINE_ERR_NONE);

    ktc_req_line_parser_resolve(&parser, (const uint8_t *)raw);
    assert(ktc_str_eq_cstr(parser.method, "GET"));
    assert(ktc_str_eq_cstr(parser.target, "/index.html"));
    assert(ktc_str_eq_cstr(parser.version, "HTTP/1.1"));
}

static void test_parse_leading_crlf(void) {
    const char *raw = "\r\n\r\nPOST /submit HTTP/1.0\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_COMPLETE);

    ktc_req_line_parser_resolve(&parser, (const uint8_t *)raw);
    assert(ktc_str_eq_cstr(parser.method, "POST"));
    assert(ktc_str_eq_cstr(parser.target, "/submit"));
    assert(ktc_str_eq_cstr(parser.version, "HTTP/1.0"));
}

static void test_parse_multiple_spaces(void) {
    const char *raw = "GET  / HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_ERROR);
    assert(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX);
}

static void test_parse_uri_too_long(void) {
    char raw[9000];
    memcpy(raw, "GET /", 5);
    memset(raw + 5, 'a', 8195);
    memcpy(raw + 8200, " HTTP/1.1\r\n", 11);
    raw[8211] = '\0';

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, 8211);
    assert(!feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_ERROR);
    assert(parser.error == KTC_REQ_LINE_ERR_URI_TOO_LONG);
}

static void test_parse_method_too_long(void) {
    const char *raw = "VERYLONGMETHODNAMEEXCEEDINGTHIRTYTWOCHARS / HTTP/1.1\r\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_ERROR);
    assert(parser.error == KTC_REQ_LINE_ERR_METHOD_NOT_IMPLEMENTED);
}

static void test_parse_incremental(void) {
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
    assert(feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_TARGET);

    feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)part2, len2);
    assert(feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_VERSION);

    feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)part3, len3);
    assert(!feeding);
    assert(parser.state == KTC_REQ_LINE_STATE_COMPLETE);

    ktc_req_line_parser_resolve(&parser, (const uint8_t *)buf);
    assert(ktc_str_eq_cstr(parser.method, "GET"));
    assert(ktc_str_eq_cstr(parser.target, "/index.html"));
    assert(ktc_str_eq_cstr(parser.version, "HTTP/1.1"));
}

int main(void) {
    test_parse_success();
    test_parse_leading_crlf();
    test_parse_multiple_spaces();
    test_parse_uri_too_long();
    test_parse_method_too_long();
    test_parse_incremental();
    printf("test_req_line: ok\n");
    return 0;
}
