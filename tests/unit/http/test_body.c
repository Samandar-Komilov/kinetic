#include "ktc/http/body.h"
#include "test_harness.h"

#include <string.h>

static void test_body_resolve_none(void) {
    KTC_TEST_CASE("RFC 9112 §6.3 (H11-FRAME-001)",
                  "GET request without body defaults to FRAMING_NONE");
    ktc_header_parser_t hp;
    ktc_header_parser_init(&hp);

    const char *raw = "Host: localhost\r\n\r\n";
    ktc_header_parser_feed(&hp, (const uint8_t *)raw, strlen(raw));
    ktc_header_parser_resolve_and_validate(&hp, (const uint8_t *)raw, ktc_str_from_cstr("/"));

    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);

    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("GET"));
    KTC_ASSERT(ok, "framing resolution succeeds");
    KTC_ASSERT(bp.framing == KTC_BODY_FRAMING_NONE, "framing resolved to NONE");
}

static void test_body_resolve_length(void) {
    KTC_TEST_CASE("RFC 9112 §6.3 (H11-FRAME-008)",
                  "POST with Content-Length resolves to FRAMING_LENGTH");
    ktc_header_parser_t hp;
    ktc_header_parser_init(&hp);

    const char *raw = "Host: localhost\r\n"
                      "Content-Length: 15\r\n"
                      "\r\n";
    ktc_header_parser_feed(&hp, (const uint8_t *)raw, strlen(raw));
    ktc_header_parser_resolve_and_validate(&hp, (const uint8_t *)raw, ktc_str_from_cstr("/"));

    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);

    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("POST"));
    KTC_ASSERT(ok, "framing resolution succeeds");
    KTC_ASSERT(bp.framing == KTC_BODY_FRAMING_LENGTH, "framing resolved to LENGTH");
    KTC_ASSERT(bp.content_length == 15, "content_length parsed as 15");
}

static void test_body_resolve_smuggling(void) {
    KTC_TEST_CASE("RFC 9112 §6.3 / §11.2 (H11-SEC-003 / H11-FRAME-004)",
                  "Reject request with BOTH Content-Length and Transfer-Encoding");
    ktc_header_parser_t hp;
    ktc_header_parser_init(&hp);

    const char *raw = "Host: localhost\r\n"
                      "Content-Length: 15\r\n"
                      "Transfer-Encoding: chunked\r\n"
                      "\r\n";
    ktc_header_parser_feed(&hp, (const uint8_t *)raw, strlen(raw));
    ktc_header_parser_resolve_and_validate(&hp, (const uint8_t *)raw, ktc_str_from_cstr("/"));

    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);

    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("POST"));
    KTC_ASSERT(!ok, "framing resolution rejects ambiguous CL+TE combination");
}

static void test_body_length_feed(void) {
    KTC_TEST_CASE("RFC 9112 §6.3 (H11-FRAME-008)", "Feed exact Content-Length bytes");
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_LENGTH;
    bp.content_length = 5;

    uint8_t out[128];
    size_t out_len = 0;

    bool rem = ktc_body_parser_feed(&bp, (const uint8_t *)"hello", 5, out, &out_len, sizeof(out));
    KTC_ASSERT(!rem, "body parsing completes when all octets consumed");
    KTC_ASSERT(out_len == 5, "payload output length is 5");
    KTC_ASSERT(memcmp(out, "hello", 5) == 0, "payload output matches hello");
}

static void test_body_chunked_feed(void) {
    KTC_TEST_CASE("RFC 9112 §7.1 (H11-CHUNK-001)", "Decode valid chunked transfer coding stream");
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;

    const char *raw = "4\r\n"
                      "Wiki\r\n"
                      "6\r\n"
                      "pedia \r\n"
                      "0\r\n"
                      "\r\n";

    uint8_t out[128] = {0};
    size_t out_len = 0;

    bool rem =
        ktc_body_parser_feed(&bp, (const uint8_t *)raw, strlen(raw), out, &out_len, sizeof(out));
    KTC_ASSERT(!rem, "chunked stream fully decoded upon terminal 0 chunk");
    KTC_ASSERT(bp.chunk_parser.state == KTC_CHUNK_STATE_COMPLETE, "chunk parser state is COMPLETE");
    KTC_ASSERT(out_len == 10, "decoded output length is 10");
    KTC_ASSERT(memcmp(out, "Wikipedia ", 10) == 0, "decoded payload matches Wikipedia ");
}

static void test_body_chunked_trailers(void) {
    KTC_TEST_CASE("RFC 9112 §7.1.1 (H11-CHUNK-002)", "Decode chunked stream with trailing headers");
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;

    const char *raw = "4\r\n"
                      "test\r\n"
                      "0\r\n"
                      "X-Trailer-One: value1\r\n"
                      "X-Trailer-Two: value2\r\n"
                      "\r\n";

    uint8_t out[128] = {0};
    size_t out_len = 0;

    bool rem =
        ktc_body_parser_feed(&bp, (const uint8_t *)raw, strlen(raw), out, &out_len, sizeof(out));
    KTC_ASSERT(!rem, "chunked stream with trailers completes");
    KTC_ASSERT(bp.chunk_parser.state == KTC_CHUNK_STATE_COMPLETE, "chunk parser state is COMPLETE");
    KTC_ASSERT(out_len == 4, "decoded output length is 4");
    KTC_ASSERT(memcmp(out, "test", 4) == 0, "decoded payload matches test");
}

static void test_body_chunked_overflow(void) {
    KTC_TEST_CASE("RFC 9112 §7.1 (H11-CHUNK-003)",
                  "Reject integer overflow on chunk size hex token");
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;

    const char *raw = "FFFFFFFFFFFFFFFFFF\r\n"; // Hex overflow

    uint8_t out[128] = {0};
    size_t out_len = 0;

    bool rem =
        ktc_body_parser_feed(&bp, (const uint8_t *)raw, strlen(raw), out, &out_len, sizeof(out));
    KTC_ASSERT(!rem, "parser halts on overflow");
    KTC_ASSERT(bp.chunk_parser.state == KTC_CHUNK_STATE_ERROR, "chunk parser state set to ERROR");
}

static void test_body_double_chunked_rejection(void) {
    KTC_TEST_CASE("RFC 9112 §6.3 (H11-FRAME-002)", "Reject invalid Transfer-Encoding listing");
    ktc_header_parser_t hp;
    ktc_header_parser_init(&hp);

    const char *raw = "Host: localhost\r\n"
                      "Transfer-Encoding: chunked, chunked\r\n"
                      "\r\n";
    ktc_header_parser_feed(&hp, (const uint8_t *)raw, strlen(raw));
    ktc_header_parser_resolve_and_validate(&hp, (const uint8_t *)raw, ktc_str_from_cstr("/"));

    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);

    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("POST"));
    KTC_ASSERT(!ok, "rejects non-standard duplicate transfer encodings");
}

static void test_body_oversized_cl_rejection(void) {
    KTC_TEST_CASE("RFC 9110 §15.5.14 (Payload Limit)",
                  "Reject Content-Length exceeding 10MB safety cap (413)");
    ktc_header_parser_t hp;
    ktc_header_parser_init(&hp);

    const char *raw = "Host: localhost\r\n"
                      "Content-Length: 10485761\r\n" // 10MB + 1 byte
                      "\r\n";
    ktc_header_parser_feed(&hp, (const uint8_t *)raw, strlen(raw));
    ktc_header_parser_resolve_and_validate(&hp, (const uint8_t *)raw, ktc_str_from_cstr("/"));

    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);

    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("POST"));
    KTC_ASSERT(!ok, "rejects payload declaration exceeding server maximum limit");
}

static void test_chunked_parser_errors(void) {
    KTC_TEST_CASE("RFC 9112 §7.1 (H11-CHUNK-001)",
                  "Reject invalid non-hex characters in chunk size");
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;

    const char *bad_size_chunk = "G\r\n";
    uint8_t out[128];
    size_t out_len = 0;
    bool rem = ktc_body_parser_feed(&bp, (const uint8_t *)bad_size_chunk, strlen(bad_size_chunk),
                                    out, &out_len, sizeof(out));
    KTC_ASSERT(!rem, "parser halts on non-hex char");
    KTC_ASSERT(bp.chunk_parser.state == KTC_CHUNK_STATE_ERROR, "chunk parser state is ERROR");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 2.5: RFC 9112 Body Framing & Chunked Decoding");
    test_body_resolve_none();
    test_body_resolve_length();
    test_body_resolve_smuggling();
    test_body_length_feed();
    test_body_chunked_feed();
    test_body_chunked_trailers();
    test_body_chunked_overflow();
    test_body_double_chunked_rejection();
    test_body_oversized_cl_rejection();
    test_chunked_parser_errors();
    KTC_TEST_SUITE_END();
}
