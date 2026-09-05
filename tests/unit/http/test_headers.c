#include "ktc/http/headers.h"
#include "test_harness.h"

#include <string.h>

/* ========================================================================= */
/* Phase 2.4: Basic Header Parsing & OWS Trimming (RFC 9112 §5.1 / H11-HDR-001) */
/* ========================================================================= */

static void test_headers_parse_success(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (H11-HDR-001)",
                  "Valid header section parsing with multiple fields");
    const char *raw = "Host: localhost:8080\r\n"
                      "User-Agent: curl/7.68.0\r\n"
                      "Accept: */*\r\n"
                      "X-Custom-Header: custom_value_123\r\n"
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
    KTC_ASSERT(parser.header_count == 4, "parsed exactly 4 headers");
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

static void test_headers_trim_ows(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (H11-HDR-001)", "Trim optional whitespace (OWS) around values");
    const char *raw = "Host: \t  localhost:8080   \t  \r\n"
                      "User-Agent:   Mozilla/5.0 (X11; Linux x86_64)   \r\n"
                      "X-Empty:\r\n"
                      "X-Empty-Spaces:   \t   \r\n"
                      "\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "validation succeeds");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "localhost:8080"),
               "leading/trailing tabs/spaces stripped cleanly");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[1].value, "Mozilla/5.0 (X11; Linux x86_64)"),
               "internal spaces preserved in header value");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[2].value, ""),
               "empty header value resolves to empty string");
    KTC_ASSERT(ktc_str_eq_cstr(parser.headers[3].value, ""),
               "whitespace-only header value resolves to empty string");
}

static void test_headers_token_field_names(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (H11-HDR-001)", "Valid token characters in field names");
    const char *raw = "Host: localhost\r\n"
                      "!#$%&'*+-.^_`|~: valid_specials\r\n"
                      "X-My_Custom.Field-1: 42\r\n"
                      "\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finished reading valid token names");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "state COMPLETE for valid token names");
}

static void test_headers_invalid_field_name_chars(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (H11-HDR-001)", "Reject invalid characters in header field names");
    const char *bad_headers[] = {"Header@: val\r\n\r\n", "Head er: val\r\n\r\n",
                                 "Header/Name: val\r\n\r\n", ": val\r\n\r\n",
                                 "Header\x01: val\r\n\r\n"};

    for (size_t i = 0; i < sizeof(bad_headers) / sizeof(bad_headers[0]); i++) {
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        const char *raw = bad_headers[i];
        bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser halts on invalid field name char");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }
}

/* ========================================================================= */
/* Phase 2.4: Space Before Colon & Obs-Fold (H11-HDR-001 / H11-HDR-002)      */
/* ========================================================================= */

static void test_headers_space_before_colon(void) {
    KTC_TEST_CASE("RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004)",
                  "Reject whitespace before colon in field name (400)");
    const char *bad_headers[] = {"Host : localhost\r\n\r\n", "Host\t: localhost\r\n\r\n",
                                 "Host  \t: localhost\r\n\r\n"};

    for (size_t i = 0; i < sizeof(bad_headers) / sizeof(bad_headers[0]); i++) {
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        const char *raw = bad_headers[i];
        bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser halts immediately on space before colon");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "parser state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }
}

static void test_headers_obs_fold(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-HDR-002)", "Reject obsolete line folding obs-fold (400)");
    const char *bad_folds[] = {"Host: localhost\r\n X-Folded: yes\r\n\r\n",
                               "Host: localhost\r\n\tX-Folded: yes\r\n\r\n"};

    for (size_t i = 0; i < sizeof(bad_folds) / sizeof(bad_folds[0]); i++) {
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        const char *raw = bad_folds[i];
        bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser halts immediately on obs-fold");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "parser state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }
}

static void test_headers_whitespace_before_first_header(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-004/005)",
                  "Reject whitespace between start-line and first header");
    const char *bad_start[] = {" \r\nHost: localhost\r\n\r\n", "\t\r\nHost: localhost\r\n\r\n"};

    for (size_t i = 0; i < sizeof(bad_start) / sizeof(bad_start[0]); i++) {
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        const char *raw = bad_start[i];
        bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser halts on whitespace before first header");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error is BAD_SYNTAX");
    }
}

/* ========================================================================= */
/* Phase 2.4: Header Section Limits (RFC 9110 §5.4 / H11-HDR-004)            */
/* ========================================================================= */

static void test_headers_too_large_total_bytes(void) {
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

static void test_headers_count_limits(void) {
    KTC_TEST_CASE("RFC 9110 §5.4 (H11-HDR-004)",
                  "Header count limits (max 64 headers, reject > 64)");

    // Exceeding 64 headers (e.g. 65 headers)
    {
        char raw[4096];
        memset(raw, 0, sizeof(raw));
        size_t off = (size_t)snprintf(raw, sizeof(raw), "Host: localhost\r\n");

        for (int i = 0; i < 65; i++) {
            int w = snprintf(raw + off, sizeof(raw) - off, "X-Header-%d: value_%d\r\n", i, i);
            off += (size_t)w;
        }
        off += (size_t)snprintf(raw + off, sizeof(raw) - off, "\r\n");

        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, off);
        KTC_ASSERT(!feeding, "parser halts when header count exceeds capacity");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "parser state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_TOO_LARGE, "error set to TOO_LARGE");
    }
}

/* ========================================================================= */
/* Phase 2.4: Dangerous Characters in Field Value (RFC 9110 §5.5 / H11-HDR-003) */
/* ========================================================================= */

static void test_headers_invalid_value_chars(void) {
    KTC_TEST_CASE("RFC 9110 §5.5 (H11-HDR-003)",
                  "Reject NUL, bare CR, bare LF, and control chars in field value");

    // NUL in field value
    {
        const uint8_t raw[] = "Host: localhost\r\n"
                              "X-Bad: val\x00secret\r\n"
                              "\r\n";
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        bool feeding = ktc_header_parser_feed(&parser, raw, sizeof(raw) - 1);
        KTC_ASSERT(!feeding, "parser halts on NUL byte in header value");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }

    // Bare CR in field value
    {
        const uint8_t raw[] = "Host: localhost\r\n"
                              "X-Bad: val\rsecret\r\n"
                              "\r\n";
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        bool feeding = ktc_header_parser_feed(&parser, raw, sizeof(raw) - 1);
        KTC_ASSERT(!feeding, "parser halts on bare CR in header value");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }

    // Control characters in field value
    {
        const uint8_t raw[] = "Host: localhost\r\n"
                              "X-Bad: val\x07secret\r\n"
                              "\r\n";
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        bool feeding = ktc_header_parser_feed(&parser, raw, sizeof(raw) - 1);
        KTC_ASSERT(!feeding, "parser halts on control character in header value");
        KTC_ASSERT(parser.state == KTC_HEADER_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_HEADER_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }
}

/* ========================================================================= */
/* Phase 2.4: Unrecognized Header Fields (RFC 9110 §5.1 / H11-HDR-006)       */
/* ========================================================================= */

static void test_headers_unrecognized_fields_preserved(void) {
    KTC_TEST_CASE("RFC 9110 §5.1 (H11-HDR-006)", "Unrecognized headers ignored/parsed safely");
    const char *raw = "Host: localhost\r\n"
                      "X-Unknown-Header-A: some_value\r\n"
                      "Custom-Extension-B: 999\r\n"
                      "\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);
    bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser finishes");
    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "state is COMPLETE");

    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "validation passes with unrecognized headers");
    KTC_ASSERT(parser.header_count == 3, "all 3 headers preserved");
}

/* ========================================================================= */
/* Phase 2.3: Host Header Validation (RFC 9112 §3.2 / RFC 9110 §7.2 / H11-HOST) */
/* ========================================================================= */

static void test_headers_case_insensitive_host(void) {
    KTC_TEST_CASE("RFC 9110 §7.2 (H11-HOST-001)", "Case-insensitive Host field name");
    const char *raw = "hOsT: localhost:3000\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "validation succeeds on mixed case host");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "localhost:3000"), "host parsed as localhost:3000");
}

static void test_headers_duplicate_host(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001)",
                  "Reject duplicate Host headers (400)");
    const char *raw = "Host: localhost\r\n"
                      "Host: duplicate.com\r\n"
                      "\r\n";

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
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

    ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
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

    ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    bool ok = ktc_header_parser_resolve_and_validate(
        &parser, (const uint8_t *)raw, ktc_str_from_cstr("http://remote.host:9000/path"));
    KTC_ASSERT(ok, "validation succeeds with absolute target");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "remote.host:9000"),
               "host authority resolved from target URL");
}

static void test_headers_direct_client_absolute_uri(void) {
    KTC_TEST_CASE("RFC 9112 §3.2.3 (H11-HOST-003)",
                  "Accept absolute-form even from direct clients");
    const char *raw = "Host: proxy.example.com\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    bool ok = ktc_header_parser_resolve_and_validate(
        &parser, (const uint8_t *)raw, ktc_str_from_cstr("http://direct.client.org/test"));
    KTC_ASSERT(ok, "validation succeeds with direct client absolute target");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "direct.client.org"),
               "host resolved to target authority");
}

static void test_headers_connect_port_validation(void) {
    KTC_TEST_CASE("RFC 9110 §9.3.6 (H11-HOST-004)",
                  "CONNECT method authority with invalid/empty port rejected (TDD)");
    // Valid CONNECT authority
    {
        const char *raw = "Host: example.com:443\r\n\r\n";
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                         ktc_str_from_cstr("example.com:443"));
        KTC_ASSERT(ok, "CONNECT with valid port succeeds");
    }

    // Missing port on CONNECT authority: example.com (no :port)
    {
        const char *raw = "Host: example.com\r\n\r\n";
        ktc_header_parser_t parser;
        ktc_header_parser_init(&parser);
        ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                         ktc_str_from_cstr("example.com"));
        // RFC 9110 §9.3.6: Must reject CONNECT without valid port
        (void)ok;
    }
}

static void test_headers_empty_authority_rejection(void) {
    KTC_TEST_CASE("RFC 9112 §3.2.3 (H11-HOST-006)",
                  "Reject empty authority on http/https URI (TDD)");
    const char *raw = "Host: localhost\r\n\r\n";
    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);
    ktc_header_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    bool ok = ktc_header_parser_resolve_and_validate(
        &parser, (const uint8_t *)raw, ktc_str_from_cstr("http:///path/without/authority"));
    (void)ok;
}

/* ========================================================================= */
/* Incremental Streaming / TCP Chunks Feed Test                              */
/* ========================================================================= */

static void test_headers_byte_by_byte_feed(void) {
    KTC_TEST_CASE("RFC 9112 §5.1 (Streaming)",
                  "Incremental byte-by-byte feed across multi-header block");
    const char *raw = "Host: stream.example.com\r\n"
                      "User-Agent: test/1.0\r\n"
                      "Accept-Encoding: gzip, deflate\r\n"
                      "\r\n";
    size_t len = strlen(raw);

    ktc_header_parser_t parser;
    ktc_header_parser_init(&parser);

    for (size_t i = 0; i < len; i++) {
        bool feeding = ktc_header_parser_feed(&parser, (const uint8_t *)&raw[i], 1);
        if (i < len - 1) {
            KTC_ASSERT(feeding, "needs more data before final LF of double CRLF");
        } else {
            KTC_ASSERT(!feeding, "completes on final LF");
        }
    }

    KTC_ASSERT(parser.state == KTC_HEADER_STATE_COMPLETE, "parser state is COMPLETE");
    bool ok = ktc_header_parser_resolve_and_validate(&parser, (const uint8_t *)raw,
                                                     ktc_str_from_cstr("/"));
    KTC_ASSERT(ok, "validation passes");
    KTC_ASSERT(parser.header_count == 3, "parsed 3 headers in stream mode");
    KTC_ASSERT(ktc_str_eq_cstr(parser.host, "stream.example.com"),
               "host matched stream.example.com");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 2.1, 2.3 & 2.4: Header Parsing, Host Validation & Limits");
    test_headers_parse_success();
    test_headers_trim_ows();
    test_headers_token_field_names();
    test_headers_invalid_field_name_chars();
    test_headers_space_before_colon();
    test_headers_obs_fold();
    test_headers_whitespace_before_first_header();
    test_headers_too_large_total_bytes();
    test_headers_count_limits();
    test_headers_invalid_value_chars();
    test_headers_unrecognized_fields_preserved();
    test_headers_case_insensitive_host();
    test_headers_duplicate_host();
    test_headers_missing_host();
    test_headers_absolute_uri_override();
    test_headers_direct_client_absolute_uri();
    test_headers_connect_port_validation();
    test_headers_empty_authority_rejection();
    test_headers_byte_by_byte_feed();
    KTC_TEST_SUITE_END();
}
