#include "ktc/http/headers.h"
#include "test_harness.h"

#include <string.h>

static void test_headers_parse_success(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (H11-HDR-001)", "Valid header section parsing");
    const char *raw = "Host: localhost:8080\r\n"
                      "User-Agent: curl/7.68.0\r\n"
                      "Accept: */*\r\n"
                      "\r\n";

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser completed consuming headers");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "parser state is COMPLETE");
    KTC_ASSERT(parser.error == KTC_HEADER_ERR_NONE, "parser error is NONE");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "header resolution & validation succeeds");
    KTC_ASSERT(parser.header_count == 3, "parsed exactly 3 headers");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[0].name, "Host"), "header 0 name is Host");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[0].value, "localhost:8080"),
               "header 0 value is localhost:8080");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[1].name, "User-Agent"),
               "header 1 name is User-Agent");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[1].value, "curl/7.68.0"),
               "header 1 value is curl/7.68.0");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "localhost:8080"),
               "resolved host matches localhost:8080");
}

static void test_headers_case_insensitive_host(void) {
    KTC_TEST_CASE("RFC 9110 §7.2 (H11-HOST-001)", "Case-insensitive Host field name");
    const char *raw = "hOsT: localhost:3000\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "validation succeeds on mixed case host");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "localhost:3000"), "host parsed as localhost:3000");
}

static void test_headers_trim_ows(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (H11-HDR-001)", "Trim optional whitespace (OWS) around values");
    const char *raw = "Host: localhost:8080   \t  \r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "validation succeeds");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "localhost:8080"),
               "trailing tabs/spaces stripped cleanly");
}

static void test_headers_space_before_colon(void) {
    KTC_TEST_CASE("RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004)",
                  "Reject space before colon in field name (400)");
    const char *raw = "Host : localhost\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts immediately");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_headers_obs_fold(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-HDR-002)", "Reject obsolete line folding obs-fold (400)");
    const char *raw = "Host: localhost\r\n"
                      " X-Folded: yes\r\n"
                      "\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts immediately");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_headers_duplicate_host(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001)",
                  "Reject duplicate Host headers (400)");
    const char *raw = "Host: localhost\r\n"
                      "Host: duplicate.com\r\n"
                      "\r\n";

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading section");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "parser state is COMPLETE");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(!ok, "validation fails on duplicate host");
    KTC_ASSERT(parser.error == KTC_HEADER_ERR_DUPLICATE_HOST, "error set to DUPLICATE_HOST");
}

static void test_headers_missing_host(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001)",
                  "Reject missing Host header in HTTP/1.1 (400)");
    const char *raw = "User-Agent: curl\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading section");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "parser state is COMPLETE");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(!ok, "validation fails on missing host");
    KTC_ASSERT(parser.error == KTC_HEADER_ERR_MISSING_HOST, "error set to MISSING_HOST");
}

static void test_headers_absolute_uri_override(void) {
    KTC_TEST_CASE("RFC 9112 §3.2.3 (H11-HOST-002)", "Absolute URI authority overrides Host header");
    const char *raw = "Host: localhost:8080\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading section");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "parser state is COMPLETE");

    bool ok = ktc_header_parser_resolve_and_validate(
        &parser, (const uint8_t *)raw, ktc_str_from_cstr("http://remote.host:9000/path"));
    KTC_ASSERT(ok, "validation succeeds with absolute target");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "remote.host:9000"),
               "host authority resolved from target URL");
}

static void test_headers_too_large(void) {
    KTC_TEST_CASE("RFC 9110 §5.4 (H11-HDR-004)", "Reject header section > 16KB (431)");
    char raw[18000];
    memset(raw, 'x', sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on oversized headers");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_HEADER_ERR_TOO_LARGE, "error set to TOO_LARGE");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 2.3 & 2.4: RFC 9112 Header Parser & Host Validation");
    test_headers_parse_success();
    test_headers_case_insensitive_host();
    test_headers_trim_ows();
    test_headers_space_before_colon();
    test_headers_obs_fold();
    test_headers_duplicate_host();
    test_headers_missing_host();
    test_headers_absolute_uri_override();
    test_headers_too_large();
    KTC_TEST_SUITE_END();
}
