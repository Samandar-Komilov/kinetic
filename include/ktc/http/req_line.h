#ifndef KTC_HTTP_REQ_LINE_H
#define KTC_HTTP_REQ_LINE_H

#include "ktc/core/str.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    KTC_REQ_LINE_STATE_IDLE,
    KTC_REQ_LINE_STATE_SKIP_EMPTY,
    KTC_REQ_LINE_STATE_METHOD,
    KTC_REQ_LINE_STATE_TARGET,
    KTC_REQ_LINE_STATE_VERSION,
    KTC_REQ_LINE_STATE_CRLF,
    KTC_REQ_LINE_STATE_COMPLETE,
    KTC_REQ_LINE_STATE_ERROR
} ktc_req_line_state_t;

typedef enum {
    KTC_REQ_LINE_ERR_NONE,
    KTC_REQ_LINE_ERR_BAD_SYNTAX,
    KTC_REQ_LINE_ERR_URI_TOO_LONG,
    KTC_REQ_LINE_ERR_METHOD_NOT_IMPLEMENTED,
    KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED
} ktc_req_line_err_t;

typedef struct {
    ktc_req_line_state_t state;
    ktc_req_line_err_t error;

    // Extracted token views (resolved on completion)
    ktc_str method;
    ktc_str target;
    ktc_str version;

    // Internal parsing indices relative to connection buffer start
    size_t method_start;
    size_t method_len;
    size_t target_start;
    size_t target_len;
    size_t version_start;
    size_t version_len;

    size_t bytes_consumed;  // Total bytes read from beginning
    size_t skipped_leading; // Count of ignored empty lines bytes
} ktc_req_line_parser_t;

void ktc_req_line_parser_init(ktc_req_line_parser_t *parser);

/**
 * Feed bytes incrementally into the parser.
 * Returns true if the parser needs more bytes, or false if it reached KTC_REQ_LINE_STATE_COMPLETE
 * or ERROR. Updates parser->bytes_consumed as it goes.
 */
bool ktc_req_line_parser_feed(ktc_req_line_parser_t *parser, const uint8_t *data, size_t len);

/**
 * Resolves the ktc_str views relative to the given base buffer pointer.
 */
void ktc_req_line_parser_resolve(ktc_req_line_parser_t *parser, const uint8_t *buf_base);

#endif /* KTC_HTTP_REQ_LINE_H */
