#ifndef KTC_HTTP_HEADERS_H
#define KTC_HTTP_HEADERS_H

/**
 * @file headers.h
 * @brief HTTP Headers parser and validator.
 */

#include "ktc/core/str.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Maximum number of headers allowed in a single HTTP request.
 */
#define KTC_MAX_HEADERS 64

/**
 * @brief Representation of a single key-value HTTP header pair.
 */
typedef struct {
    ktc_str name;  /**< Non-owning string slice of the header field name. */
    ktc_str value; /**< Non-owning string slice of the header field value. */
} ktc_header_t;

/**
 * @brief FSM states for the incremental HTTP header parser.
 */
typedef enum {
    KTC_HEADER_STATE_NAME,        /**< Currently reading the header name token. */
    KTC_HEADER_STATE_VALUE_START, /**< Skipping whitespace/LWSP preceding the header value. */
    KTC_HEADER_STATE_VALUE,       /**< Currently reading the header value field. */
    KTC_HEADER_STATE_CR,          /**< Found a Carriage Return (CR) expecting Line Feed (LF). */
    KTC_HEADER_STATE_CRLF,        /**< Found a full CRLF pair terminating a single header field. */
    KTC_HEADER_STATE_DOUBLE_CRLF, /**< Found double CRLF, completing the headers block. */
    KTC_HEADER_STATE_COMPLETE,    /**< Parsing process completed successfully. */
    KTC_HEADER_STATE_ERROR        /**< Header format, syntax, or size error encountered. */
} ktc_header_state_t;

/**
 * @brief Specific error classification types for validation failures.
 */
typedef enum {
    KTC_HEADER_ERR_NONE,       /**< No errors. */
    KTC_HEADER_ERR_BAD_SYNTAX, /**< Syntax error (e.g. invalid chars, missing colon, obs-fold). */
    KTC_HEADER_ERR_TOO_LARGE,  /**< Individual header or total size exceeded maximum limits. */
    KTC_HEADER_ERR_DUPLICATE_HOST, /**< RFC 9112 §3.2.4: Reject multiple Host headers. */
    KTC_HEADER_ERR_MISSING_HOST    /**< RFC 9112 §3.2.4: Missing Host header in HTTP/1.1. */
} ktc_header_err_t;

/**
 * @brief State container for header parsing, validation, and resolution.
 */
typedef struct {
    ktc_header_state_t state; /**< Active parser FSM state. */
    ktc_header_err_t error;   /**< Parse error code if state is KTC_HEADER_STATE_ERROR. */

    ktc_header_t headers[KTC_MAX_HEADERS]; /**< Extracted headers array. */
    size_t header_count;                   /**< Total number of successfully parsed headers. */

    ktc_str host; /**< Decoded Host header slice (cached on validation). */

    /* Internal index tracking mapping raw slice boundaries into the connection buffer */
    size_t name_start[KTC_MAX_HEADERS];  /**< Starting offset indices for header names. */
    size_t name_len[KTC_MAX_HEADERS];    /**< Lengths of header names. */
    size_t value_start[KTC_MAX_HEADERS]; /**< Starting offset indices for header values. */
    size_t value_len[KTC_MAX_HEADERS];   /**< Lengths of header values. */

    size_t current_name_start;  /**< Workspace: current header name starting index. */
    size_t current_name_len;    /**< Workspace: current header name length. */
    size_t current_value_start; /**< Workspace: current header value starting index. */
    size_t current_value_len;   /**< Workspace: current header value length. */

    size_t bytes_consumed;     /**< Total bytes processed from the headers buffer. */
    size_t total_headers_size; /**< Accumulated byte size of the parsed headers. */
} ktc_header_parser_t;

/**
 * @brief Initializes the header parser state to defaults.
 *
 * @param parser Pointer to the header parser struct.
 */
void ktc_header_parser_init(ktc_header_parser_t *parser);

/**
 * @brief Feeds incoming network bytes incrementally into the header parser.
 *
 * Scans for individual headers and transitions state until double CRLF is reached.
 *
 * @param parser Pointer to the header parser struct.
 * @param data Pointer to the buffer containing incoming network bytes.
 * @param len The size of incoming bytes.
 * @return true if more data is needed, false if double CRLF completes parsing or an error is hit.
 */
bool ktc_header_parser_feed(ktc_header_parser_t *parser, const uint8_t *data, size_t len);

/**
 * @brief Resolves non-owning header slices relative to the base buffer and validates invariants.
 *
 * Performs post-parse resolution of non-owning string views and verifies mandatory HTTP/1.1
 * rules (e.g., exactly one Host header, matching target constraints).
 *
 * @param parser Pointer to the header parser struct.
 * @param buf_base The starting base pointer of the accumulated network buffer.
 * @param request_target The target URI parsed from the Request Line.
 * @return true if validation succeeds, false if invalid (sets parser->error).
 */
bool ktc_header_parser_resolve_and_validate(ktc_header_parser_t *parser, const uint8_t *buf_base,
                                            ktc_str request_target);

#endif /* KTC_HTTP_HEADERS_H */
