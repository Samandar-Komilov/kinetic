#ifndef KTC_HTTP_BODY_H
#define KTC_HTTP_BODY_H

#include "ktc/core/str.h"
#include "ktc/http/headers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    KTC_BODY_FRAMING_NONE,
    KTC_BODY_FRAMING_LENGTH,
    KTC_BODY_FRAMING_CHUNKED
} ktc_body_framing_t;

typedef enum {
    KTC_CHUNK_STATE_SIZE,
    KTC_CHUNK_STATE_EXTENSION,
    KTC_CHUNK_STATE_SIZE_CRLF,
    KTC_CHUNK_STATE_DATA,
    KTC_CHUNK_STATE_DATA_CRLF,
    KTC_CHUNK_STATE_TRAILERS_START,
    KTC_CHUNK_STATE_TRAILERS_DATA,
    KTC_CHUNK_STATE_TRAILERS_CRLF,
    KTC_CHUNK_STATE_TRAILERS_FINAL_LF,
    KTC_CHUNK_STATE_COMPLETE,
    KTC_CHUNK_STATE_ERROR
} ktc_chunk_state_t;

typedef struct {
    ktc_chunk_state_t state;
    size_t chunk_size;
    size_t chunk_remaining;
    size_t trailers_crlf_count;
    bool last_was_cr;
} ktc_chunk_parser_t;

typedef struct {
    ktc_body_framing_t framing;
    size_t content_length;
    size_t body_consumed;

    ktc_chunk_parser_t chunk_parser;
} ktc_body_parser_t;

void ktc_body_parser_init(ktc_body_parser_t *parser);

/**
 * Resolves request body framing strategy using §6.3 precedence rules.
 * Returns true if valid framing, false if bad request.
 */
bool ktc_body_resolve_framing(ktc_body_parser_t *parser, const ktc_header_parser_t *header_parser,
                              ktc_str method);

/**
 * Feeds body bytes.
 * Returns true if more bytes are needed, false if completed or error.
 */
bool ktc_body_parser_feed(ktc_body_parser_t *parser, const uint8_t *data, size_t len,
                          uint8_t *out_buf, size_t *out_len, size_t max_len);

#endif /* KTC_HTTP_BODY_H */
