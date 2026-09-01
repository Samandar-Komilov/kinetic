#include "ktc/http/body.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_body_resolve_none(void) {
    ktc_header_parser_t hp;
    ktc_header_parser_init(&hp);

    const char *raw = "Host: localhost\r\n\r\n";
    ktc_header_parser_feed(&hp, (const uint8_t *)raw, strlen(raw));
    ktc_header_parser_resolve_and_validate(&hp, (const uint8_t *)raw, ktc_str_from_cstr("/"));

    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);

    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("GET"));
    assert(ok);
    assert(bp.framing == KTC_BODY_FRAMING_NONE);
}

static void test_body_resolve_length(void) {
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
    assert(ok);
    assert(bp.framing == KTC_BODY_FRAMING_LENGTH);
    assert(bp.content_length == 15);
}

static void test_body_resolve_smuggling(void) {
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

    // Smuggling guard: both CL and TE present -> return false
    bool ok = ktc_body_resolve_framing(&bp, &hp, ktc_str_from_cstr("POST"));
    assert(!ok);
}

static void test_body_length_feed(void) {
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_LENGTH;
    bp.content_length = 5;

    uint8_t out[128];
    size_t out_len = 0;

    bool rem = ktc_body_parser_feed(&bp, (const uint8_t *)"hello", 5, out, &out_len, sizeof(out));
    assert(!rem); // Complete
    assert(out_len == 5);
    assert(memcmp(out, "hello", 5) == 0);
}

static void test_body_chunked_feed(void) {
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
    assert(!rem); // Finished parsing chunked payload
    assert(bp.chunk_parser.state == KTC_CHUNK_STATE_COMPLETE);
    assert(out_len == 10);
    assert(memcmp(out, "Wikipedia ", 10) == 0);
}

static void test_body_chunked_trailers(void) {
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
    assert(!rem); // Completed
    assert(bp.chunk_parser.state == KTC_CHUNK_STATE_COMPLETE);
    assert(out_len == 4);
    assert(memcmp(out, "test", 4) == 0);
}

static void test_body_chunked_overflow(void) {
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;

    const char *raw = "FFFFFFFFFFFFFFFFFF\r\n"; // Hex overflow

    uint8_t out[128] = {0};
    size_t out_len = 0;

    bool rem =
        ktc_body_parser_feed(&bp, (const uint8_t *)raw, strlen(raw), out, &out_len, sizeof(out));
    assert(!rem);
    assert(bp.chunk_parser.state == KTC_CHUNK_STATE_ERROR);
}

static void test_body_double_chunked_rejection(void) {
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
    assert(!ok); // Rejected!
}

static void test_body_oversized_cl_rejection(void) {
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
    assert(!ok); // Rejected!
}

static void test_chunked_parser_errors(void) {
    ktc_body_parser_t bp;
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;

    // 1. Invalid non-hex size character
    const char *bad_size_chunk = "G\r\n";
    uint8_t out[128];
    size_t out_len = 0;
    bool rem = ktc_body_parser_feed(&bp, (const uint8_t *)bad_size_chunk, strlen(bad_size_chunk),
                                    out, &out_len, sizeof(out));
    assert(!rem); // parsing aborted
    assert(bp.chunk_parser.state == KTC_CHUNK_STATE_ERROR);

    // 2. Overflowing chunk size
    ktc_body_parser_init(&bp);
    bp.framing = KTC_BODY_FRAMING_CHUNKED;
    const char *overflow_chunk = "FFFFFFFFFFFFFFFFF\r\n"; // 17 hex digits triggers size overflow
    rem = ktc_body_parser_feed(&bp, (const uint8_t *)overflow_chunk, strlen(overflow_chunk), out,
                               &out_len, sizeof(out));
    assert(!rem);
    assert(bp.chunk_parser.state == KTC_CHUNK_STATE_ERROR);
}

int main(void) {
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
    printf("test_body: ok\n");
    return 0;
}
