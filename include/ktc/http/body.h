#ifndef KTC_HTTP_BODY_H
#define KTC_HTTP_BODY_H

/**
 * @file body.h
 * @brief HTTP Request Body parsing and decoding.
 */

#include "ktc/core/str.h"
#include "ktc/http/headers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief HTTP/1.1 body transfer framing strategy.
 */
typedef enum {
    KTC_BODY_FRAMING_NONE,   /**< No request body present (e.g., standard GET/HEAD). */
    KTC_BODY_FRAMING_LENGTH, /**< Content-Length framing. */
    KTC_BODY_FRAMING_CHUNKED /**< Chunked Transfer-Encoding framing. */
} ktc_body_framing_t;

/**
 * @brief FSM states for the HTTP chunked transfer-encoding parser.
 */
typedef enum {
    KTC_CHUNK_STATE_SIZE,              /**< Parsing chunk size hex digits. */
    KTC_CHUNK_STATE_EXTENSION,         /**< Skipping optional chunk extensions. */
    KTC_CHUNK_STATE_SIZE_CRLF,         /**< Validating CRLF after the size indicator. */
    KTC_CHUNK_STATE_DATA,              /**< Reading chunk payload octets. */
    KTC_CHUNK_STATE_DATA_CRLF,         /**< Validating CRLF ending a data chunk. */
    KTC_CHUNK_STATE_TRAILERS_START,    /**< Checking for trailers/final CRLF. */
    KTC_CHUNK_STATE_TRAILERS_DATA,     /**< Parsing trailer headers block. */
    KTC_CHUNK_STATE_TRAILERS_CRLF,     /**< Validating trailer lines CRLF. */
    KTC_CHUNK_STATE_TRAILERS_FINAL_LF, /**< Final LF of the trailers block. */
    KTC_CHUNK_STATE_COMPLETE,          /**< Chunked transfer fully complete. */
    KTC_CHUNK_STATE_ERROR              /**< Parser validation or formatting error. */
} ktc_chunk_state_t;

/**
 * @brief State container for chunked transfer decoding.
 */
typedef struct {
    ktc_chunk_state_t state;    /**< Current FSM state. */
    size_t chunk_size;          /**< Resolved size of the active chunk. */
    size_t chunk_remaining;     /**< Bytes left to read in the current chunk. */
    size_t trailers_crlf_count; /**< Counter for empty trailer CRLFs. */
    bool last_was_cr;           /**< Invalidation check helper for CR occurrence. */
} ktc_chunk_parser_t;

/**
 * @brief Parser context for request body framing and decoding.
 */
typedef struct {
    ktc_body_framing_t framing;      /**< Resolved framing strategy. */
    size_t content_length;           /**< Expected content length (for KTC_BODY_FRAMING_LENGTH). */
    size_t body_consumed;            /**< Raw stream bytes consumed from the body socket. */
    ktc_chunk_parser_t chunk_parser; /**< Nested chunk parser context. */
} ktc_body_parser_t;

/**
 * @brief Initializes the body parser context to default values.
 *
 * @param parser Pointer to the body parser struct.
 */
void ktc_body_parser_init(ktc_body_parser_t *parser);

/**
 * @brief Resolves the request body framing strategy using RFC 9112 §6 precedence rules.
 *
 * Decides whether Content-Length, Transfer-Encoding: chunked, or no body is expected.
 *
 * @param parser Pointer to the body parser struct.
 * @param header_parser Pointer to the header parser struct containing resolved header fields.
 * @param method The HTTP request method slice.
 * @return true if framing was successfully resolved, false if a conflict (bad request) is detected.
 */
bool ktc_body_resolve_framing(ktc_body_parser_t *parser, const ktc_header_parser_t *header_parser,
                              ktc_str method);

/**
 * @brief Feeds raw network stream bytes incrementally into the body parser.
 *
 * Decodes the body stream according to the resolved framing strategy. For chunked encoding,
 * decodes and copies chunk payloads into out_buf. For content-length, copies raw bytes.
 *
 * @param parser Pointer to the body parser struct.
 * @param data Pointer to the buffer containing incoming network bytes.
 * @param len The size of incoming bytes to process.
 * @param out_buf Target buffer to receive decoded payload bytes.
 * @param out_len In/Out pointer. On input, must point to the current size of the decoded payload.
 *                On output, updated to the new total size of the decoded payload.
 * @param max_len Maximum capacity of the destination buffer `out_buf`.
 * @return true if more data is expected, false if body parsing completed or a parser error occurs.
 */
bool ktc_body_parser_feed(ktc_body_parser_t *parser, const uint8_t *data, size_t len,
                          uint8_t *out_buf, size_t *out_len, size_t max_len);

#endif /* KTC_HTTP_BODY_H */
