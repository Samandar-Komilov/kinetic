#include "ktc/http/headers.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_headers_parse_success(void) {
    const char *raw = "Host: localhost:8080\r\n"
                      "User-Agent: curl/7.68.0\r\n"
                      "Accept: */*\r\n"
                      "\r\n";

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding); // Complete
    assert(parser.state == KTC_HEADER_STATE_COMPLETE);
    assert(parser.error == KTC_HEADER_ERR_NONE);

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    assert(ok);
    assert(parser.header_count == 3);
    assert(ktc_str_eq_cstr(parser.headers[0].name, "Host"));
    assert(ktc_str_eq_cstr(parser.headers[0].value, "localhost:8080"));
    assert(ktc_str_eq_cstr(parser.headers[1].name, "User-Agent"));
    assert(ktc_str_eq_cstr(parser.headers[1].value, "curl/7.68.0"));
    assert(ktc_str_eq_cstr(parser.host, "localhost:8080"));
}

static void test_headers_case_insensitive_host(void) {
    const char *raw = "hOsT: localhost:3000\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    assert(ok);
    assert(ktc_str_eq_cstr(parser.host, "localhost:3000"));
}

static void test_headers_trim_ows(void) {
    const char *raw = "Host: localhost:8080   \t  \r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    assert(ok);
    // Value must have trailing tabs and spaces stripped
    assert(ktc_str_eq_cstr(parser.host, "localhost:8080"));
}

static void test_headers_space_before_colon(void) {
    const char *raw = "Host : localhost\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_HEADER_STATE_ERROR);
    assert(parser.error == KTC_HEADER_ERR_BAD_SYNTAX);
}

static void test_headers_obs_fold(void) {
    const char *raw = "Host: localhost\r\n"
                      " X-Folded: yes\r\n"
                      "\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_HEADER_STATE_ERROR);
    assert(parser.error == KTC_HEADER_ERR_BAD_SYNTAX);
}

static void test_headers_duplicate_host(void) {
    const char *raw = "Host: localhost\r\n"
                      "Host: duplicate.com\r\n"
                      "\r\n";

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_HEADER_STATE_COMPLETE);

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    assert(!ok);
    assert(parser.error == KTC_HEADER_ERR_DUPLICATE_HOST);
}

static void test_headers_missing_host(void) {
    const char *raw = "User-Agent: curl\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_HEADER_STATE_COMPLETE);

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    assert(!ok);
    assert(parser.error == KTC_HEADER_ERR_MISSING_HOST);
}

static void test_headers_absolute_uri_override(void) {
    const char *raw = "Host: localhost:8080\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_HEADER_STATE_COMPLETE);

    // Absolute URI overrides the Host header authority
    bool ok = ktc_header_parser_resolve_and_validate(
        &parser, (const uint8_t *)raw, ktc_str_from_cstr("http://remote.host:9000/path"));
    assert(ok);
    assert(ktc_str_eq_cstr(parser.host, "remote.host:9000"));
}

static void test_headers_too_large(void) {
    // Generate a header section exceeding 16KB limit
    char raw[18000];
    memset(raw, 'x', sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    assert(!feeding);
    assert(parser.state == KTC_HEADER_STATE_ERROR);
    assert(parser.error == KTC_HEADER_ERR_TOO_LARGE);
}

int main(void) {
    test_headers_parse_success();
    test_headers_case_insensitive_host();
    test_headers_trim_ows();
    test_headers_space_before_colon();
    test_headers_obs_fold();
    test_headers_duplicate_host();
    test_headers_missing_host();
    test_headers_absolute_uri_override();
    test_headers_too_large();
    printf("test_headers: ok\n");
    return 0;
}
