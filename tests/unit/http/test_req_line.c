#include "ktc/http/req_line.h"
#include "test_harness.h"

#include <string.h>

/* ========================================================================= */
/* Phase 2.2: Standard Methods & Grammar (RFC 9112 §3.2 / H11-REQLINE-001)   */
/* ========================================================================= */

static void test_parse_get_success(void) {
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

static void test_parse_standard_methods(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-001)", "Valid standard HTTP methods");
    const char *methods[] = {"HEAD /api HTTP/1.1\r\n",
                             "POST /submit HTTP/1.1\r\n",
                             "PUT /item/1 HTTP/1.1\r\n",
                             "DELETE /item/2 HTTP/1.1\r\n",
                             "OPTIONS * HTTP/1.1\r\n",
                             "TRACE /trace HTTP/1.1\r\n",
                             "CONNECT example.com:443 HTTP/1.1\r\n",
                             "PATCH /item/3 HTTP/1.1\r\n",
                             "CUSTOM-METHOD /test HTTP/1.1\r\n"};
    const char *expected_methods[] = {"HEAD",  "POST",    "PUT",   "DELETE",       "OPTIONS",
                                      "TRACE", "CONNECT", "PATCH", "CUSTOM-METHOD"};

    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        const char *raw = methods[i];
        bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser completed line");
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE,
                   "state COMPLETE for standard method");

        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(ktc_str_eq_cstr(parser.method, expected_methods[i]),
                   "parsed method matches expected");
    }
}

/* ========================================================================= */
/* Phase 2.2: Request Targets Forms (RFC 9112 §3.2.1)                        */
/* ========================================================================= */

static void test_parse_target_forms(void) {
    KTC_TEST_CASE("RFC 9112 §3.2.1",
                  "Accept origin-form, absolute-form, authority-form, asterisk-form");

    // Origin-form
    {
        const char *raw = "GET /path/to/resource?query=1&page=2#fragment HTTP/1.1\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE,
                   "origin-form with query/hash complete");
        KTC_ASSERT(ktc_str_eq_cstr(parser.target, "/path/to/resource?query=1&page=2#fragment"),
                   "origin-form target extracted accurately");
    }

    // Absolute-form
    {
        const char *raw = "GET http://example.com:8080/api/v1/status?verbose=true HTTP/1.1\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "absolute-form complete");
        KTC_ASSERT(
            ktc_str_eq_cstr(parser.target, "http://example.com:8080/api/v1/status?verbose=true"),
            "absolute-form target extracted accurately");
    }

    // Authority-form (CONNECT)
    {
        const char *raw = "CONNECT proxy.example.com:8443 HTTP/1.1\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "authority-form complete");
        KTC_ASSERT(ktc_str_eq_cstr(parser.target, "proxy.example.com:8443"),
                   "authority-form target extracted accurately");
    }

    // Asterisk-form (OPTIONS *)
    {
        const char *raw = "OPTIONS * HTTP/1.1\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "asterisk-form complete");
        KTC_ASSERT(ktc_str_eq_cstr(parser.target, "*"), "asterisk-form target is *");
    }
}

/* ========================================================================= */
/* Phase 2.1: Line Endings & Octet Streams (RFC 9112 §2.2)                   */
/* ========================================================================= */

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

static void test_parse_leading_crlf_boundary(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-006)",
                  "Leading CRLF tolerance boundary (up to 20 CRLFs)");
    char raw[256];
    memset(raw, 0, sizeof(raw));
    size_t off = 0;
    for (int i = 0; i < 10; i++) {
        off += (size_t)snprintf(raw + off, sizeof(raw) - off, "\r\n");
    }
    const char *tail = "GET / HTTP/1.1\r\n";
    off += (size_t)snprintf(raw + off, sizeof(raw) - off, "%s", tail);

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);
    ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, off);
    ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "10 leading CRLFs allowed");
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

static void test_parse_octet_stream_high_bytes(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-001)",
                  "Parse messages as octet stream (high-bit raw bytes)");
    // UTF-8 bytes in URI / query string: /search?q= (10) + \xC3\xA9 (2) + \xF0\x9F\x98\x80 (4) = 16
    // bytes
    const uint8_t raw[] = "GET /search?q=\xC3\xA9\xF0\x9F\x98\x80 HTTP/1.1\r\n";
    size_t raw_len = sizeof(raw) - 1;

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, raw, raw_len);
    KTC_ASSERT(!feeding, "parser completed line with high bytes");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "parser state is COMPLETE");

    ktc_req_line_parser_verify(&parser, raw);
    KTC_ASSERT(parser.target.len == 16, "target length correctly matches raw octets count");
}

static void test_parse_bare_cr_rejection(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-003)", "Reject bare CR in request-line");
    const char *raw = "GET / HTTP/1.1\rX";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on invalid line ending");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

static void test_parse_bare_lf_rejection(void) {
    KTC_TEST_CASE("RFC 9112 §2.2 (H11-PARSE-003)", "Reject bare LF without CR in request-line");
    const char *raw = "GET / HTTP/1.1\n";
    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
    KTC_ASSERT(!feeding, "parser halts on bare LF");
    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
    KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
}

/* ========================================================================= */
/* Phase 2.2: URI & Method Limits (H11-REQLINE-002 / H11-REQLINE-003)        */
/* ========================================================================= */

static void test_parse_uri_limits_boundary(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-002)", "Target URI 8192-byte limit boundary check");

    // Exactly 8192 bytes URI target
    {
        char raw[8250];
        memset(raw, 0, sizeof(raw));
        size_t off = (size_t)snprintf(raw, sizeof(raw), "GET /");
        memset(raw + off, 'a', 8191); // 1 + 8191 = 8192 bytes URI
        off += 8191;
        off += (size_t)snprintf(raw + off, sizeof(raw) - off, " HTTP/1.1\r\n");
        size_t total_len = off;

        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, total_len);
        KTC_ASSERT(!feeding, "parser consumes valid 8192 URI");
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "8192 bytes target is accepted");
    }

    // 8193 bytes URI target -> 414 URI Too Long
    {
        char raw[8250];
        memset(raw, 0, sizeof(raw));
        size_t off = (size_t)snprintf(raw, sizeof(raw), "GET /");
        memset(raw + off, 'a', 8192); // 1 + 8192 = 8193 bytes URI
        off += 8192;
        off += (size_t)snprintf(raw + off, sizeof(raw) - off, " HTTP/1.1\r\n");
        size_t total_len = off;

        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, total_len);
        KTC_ASSERT(!feeding, "parser halts on oversized URI");
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "parser state is ERROR");
        KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_URI_TOO_LONG, "error set to URI_TOO_LONG");
    }
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
    const char *bad_methods[] = {"GE[T] / HTTP/1.1\r\n", "GET@ / HTTP/1.1\r\n",
                                 "GET/ / HTTP/1.1\r\n", "G=ET / HTTP/1.1\r\n"};

    for (size_t i = 0; i < sizeof(bad_methods) / sizeof(bad_methods[0]); i++) {
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        const char *raw = bad_methods[i];
        bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser halts on non-token characters");
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error set to BAD_SYNTAX");
    }
}

/* ========================================================================= */
/* Phase 2.2: Version Validation (H11-STATUS-004)                            */
/* ========================================================================= */

static void test_parse_http_versions(void) {
    KTC_TEST_CASE("RFC 9110 §6.2 (H11-STATUS-004)",
                  "Support HTTP/1.1 & HTTP/1.0, refuse others (505)");

    // HTTP/1.0
    {
        const char *raw = "GET / HTTP/1.0\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE, "HTTP/1.0 is supported");
    }

    // HTTP/2.0
    {
        const char *raw = "GET / HTTP/2.0\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "HTTP/2.0 triggers error");
        KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED,
                   "error VERSION_NOT_SUPPORTED");
    }

    // HTTP/3.0
    {
        const char *raw = "GET / HTTP/3.0\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "HTTP/3.0 triggers error");
        KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED,
                   "error VERSION_NOT_SUPPORTED");
    }

    // HTTP/0.9
    {
        const char *raw = "GET / HTTP/0.9\r\n";
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "HTTP/0.9 triggers error");
        KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED,
                   "error VERSION_NOT_SUPPORTED");
    }
}

/* ========================================================================= */
/* Phase 2.2: Syntax Violations (H11-REQLINE-004)                            */
/* ========================================================================= */

static void test_parse_syntax_violations(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (H11-REQLINE-004)", "Reject syntax violations in request line");

    const char *bad_lines[] = {
        "GET  / HTTP/1.1\r\n",        // Double space between method & target
        "GET /  HTTP/1.1\r\n",        // Double space between target & version
        "GET / HTTP/1.1 \r\n",        // Trailing space after version
        " GET / HTTP/1.1\r\n",        // Leading space before method
        "GET\t/\tHTTP/1.1\r\n",       // Tab separators
        "GET /\tHTTP/1.1\r\n",        // Tab before version
        " / HTTP/1.1\r\n",            // Missing method
        "GET /\r\n",                  // Missing version
        "GET  HTTP/1.1\r\n",          // Missing target
        "GET /\x01/test HTTP/1.1\r\n" // Control character in URI
    };

    for (size_t i = 0; i < sizeof(bad_lines) / sizeof(bad_lines[0]); i++) {
        ktc_req_line_parser_t parser;
        ktc_req_line_parser_init(&parser);
        const char *raw = bad_lines[i];
        bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)raw, strlen(raw));
        KTC_ASSERT(!feeding, "parser halts on syntax violation");
        KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_ERROR, "state is ERROR");
        KTC_ASSERT(parser.error == KTC_REQ_LINE_ERR_BAD_SYNTAX, "error is BAD_SYNTAX");
    }
}

/* ========================================================================= */
/* Streaming / TCP Packet Boundary Feed Tests                                */
/* ========================================================================= */

static void test_parse_byte_by_byte_feed(void) {
    KTC_TEST_CASE("RFC 9112 §3.2 (FSM Streaming)",
                  "Incremental byte-by-byte feed across entire request line");
    const char *raw = "GET /stream/test?id=123 HTTP/1.1\r\n";
    size_t len = strlen(raw);

    ktc_req_line_parser_t parser;
    ktc_req_line_parser_init(&parser);

    for (size_t i = 0; i < len; i++) {
        bool feeding = ktc_req_line_parser_feed(&parser, (const uint8_t *)&raw[i], 1);
        if (i < len - 1) {
            KTC_ASSERT(feeding, "needs more data before final LF");
        } else {
            KTC_ASSERT(!feeding, "completes on final LF");
        }
    }

    KTC_ASSERT(parser.state == KTC_REQ_LINE_STATE_COMPLETE,
               "parser COMPLETE after single-byte stream");
    ktc_req_line_parser_verify(&parser, (const uint8_t *)raw);
    KTC_ASSERT(ktc_str_eq_cstr(parser.method, "GET"), "method parsed as GET");
    KTC_ASSERT(ktc_str_eq_cstr(parser.target, "/stream/test?id=123"), "target accurately parsed");
    KTC_ASSERT(ktc_str_eq_cstr(parser.version, "HTTP/1.1"), "version is HTTP/1.1");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 2.1 & 2.2: RFC 9112 Request Line & Octet Stream Grammar");
    test_parse_get_success();
    test_parse_standard_methods();
    test_parse_target_forms();
    test_parse_leading_crlf();
    test_parse_leading_crlf_boundary();
    test_parse_too_much_leading_junk();
    test_parse_octet_stream_high_bytes();
    test_parse_bare_cr_rejection();
    test_parse_bare_lf_rejection();
    test_parse_uri_limits_boundary();
    test_parse_method_too_long();
    test_parse_invalid_method_chars();
    test_parse_http_versions();
    test_parse_syntax_violations();
    test_parse_byte_by_byte_feed();
    KTC_TEST_SUITE_END();
}
