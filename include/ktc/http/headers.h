#ifndef KTC_HTTP_HEADERS_H
#define KTC_HTTP_HEADERS_H

#include "ktc/core/str.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KTC_MAX_HEADERS 64

typedef struct {
    ktc_str name;
    ktc_str value;
} ktc_header_t;

typedef enum {
    KTC_HEADER_STATE_NAME,
    KTC_HEADER_STATE_VALUE_START,
    KTC_HEADER_STATE_VALUE,
    KTC_HEADER_STATE_CR,
    KTC_HEADER_STATE_CRLF,
    KTC_HEADER_STATE_DOUBLE_CRLF,
    KTC_HEADER_STATE_COMPLETE,
    KTC_HEADER_STATE_ERROR
} ktc_header_state_t;

typedef enum {
    KTC_HEADER_ERR_NONE,
    KTC_HEADER_ERR_BAD_SYNTAX,
    KTC_HEADER_ERR_TOO_LARGE,
    KTC_HEADER_ERR_DUPLICATE_HOST,
    KTC_HEADER_ERR_MISSING_HOST
} ktc_header_err_t;

typedef struct {
    ktc_header_state_t state;
    ktc_header_err_t error;

    ktc_header_t headers[KTC_MAX_HEADERS];
    size_t header_count;

    // Resolved Host header view
    ktc_str host;

    // Internal parsing indices
    size_t name_start[KTC_MAX_HEADERS];
    size_t name_len[KTC_MAX_HEADERS];
    size_t value_start[KTC_MAX_HEADERS];
    size_t value_len[KTC_MAX_HEADERS];

    size_t current_name_start;
    size_t current_name_len;
    size_t current_value_start;
    size_t current_value_len;

    size_t bytes_consumed;
    size_t total_headers_size;
} ktc_header_parser_t;

void ktc_header_parser_init(ktc_header_parser_t *parser);

/**
 * Feed bytes incrementally into the header parser.
 * Returns true if more bytes are needed, or false if complete/error.
 */
bool ktc_header_parser_feed(ktc_header_parser_t *parser, const uint8_t *data, size_t len);

/**
 * Resolves header tokens based on the base pointer, and validates the Host header.
 * Returns true if valid, false if invalid (setting appropriate error).
 */
bool ktc_header_parser_resolve_and_validate(ktc_header_parser_t *parser, const uint8_t *buf_base,
                                            ktc_str request_target);

#endif /* KTC_HTTP_HEADERS_H */
