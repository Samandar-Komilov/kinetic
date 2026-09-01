#ifndef KTC_HTTP_REQ_LINE_H
#define KTC_HTTP_REQ_LINE_H

/**
 * @file req_line.h
 * @brief HTTP Request Line parser and validator.
 */

#include "ktc/core/str.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief FSM states for parsing the HTTP request line.
 */
typedef enum {
    KTC_REQ_LINE_STATE_IDLE,       /**< Waiting for the first byte. */
    KTC_REQ_LINE_STATE_SKIP_EMPTY, /**< Skipping leading CRLFs (RFC 9112 §2.2). */
    KTC_REQ_LINE_STATE_METHOD,     /**< Reading request method token. */
    KTC_REQ_LINE_STATE_TARGET,     /**< Reading request target URI token. */
    KTC_REQ_LINE_STATE_VERSION,    /**< Reading HTTP version protocol string. */
    KTC_REQ_LINE_STATE_CRLF,       /**< Awaiting terminating LF of request line CRLF. */
    KTC_REQ_LINE_STATE_COMPLETE,   /**< Request line successfully parsed. */
    KTC_REQ_LINE_STATE_ERROR       /**< Syntax or constraint error encountered. */
} ktc_req_line_state_t;

/**
 * @brief Detailed error reasons for request line failure.
 */
typedef enum {
    KTC_REQ_LINE_ERR_NONE,         /**< No error. */
    KTC_REQ_LINE_ERR_BAD_SYNTAX,   /**< Invalid spacing, character, or missing elements. */
    KTC_REQ_LINE_ERR_URI_TOO_LONG, /**< Request target exceeded buffer capacity limits. */
    KTC_REQ_LINE_ERR_METHOD_NOT_IMPLEMENTED, /**< Unknown or unimplemented HTTP method token. */
    KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED   /**< Unsupported HTTP protocol version. */
} ktc_req_line_err_t;

/**
 * @brief State container for parsing and resolving the HTTP request line.
 */
typedef struct {
    ktc_req_line_state_t state; /**< Current parsing FSM state. */
    ktc_req_line_err_t error;   /**< Parse error type if state is KTC_REQ_LINE_STATE_ERROR. */

    /* Extracted token views (resolved on completion) */
    ktc_str method;  /**< Resolved request method slice (e.g. GET). */
    ktc_str target;  /**< Resolved request target URI slice. */
    ktc_str version; /**< Resolved HTTP version protocol slice. */

    /* Internal parsing boundaries relative to the connection buffer start */
    size_t method_start;  /**< Starting buffer index of the method token. */
    size_t method_len;    /**< Byte length of the method token. */
    size_t target_start;  /**< Starting buffer index of the target URI. */
    size_t target_len;    /**< Byte length of the target URI. */
    size_t version_start; /**< Starting buffer index of the HTTP version. */
    size_t version_len;   /**< Byte length of the HTTP version. */

    size_t bytes_consumed;  /**< Total bytes processed from the buffer. */
    size_t skipped_leading; /**< Number of leading empty line bytes skipped. */
} ktc_req_line_parser_t;

/**
 * @brief Initializes the request line parser state to defaults.
 *
 * @param parser Pointer to the request line parser struct.
 */
void ktc_req_line_parser_init(ktc_req_line_parser_t *parser);

/**
 * @brief Feeds incoming network bytes incrementally into the request line parser.
 *
 * Scans the buffer and advances state machine until request line CRLF is reached or error occurs.
 *
 * @param parser Pointer to the request line parser struct.
 * @param data Pointer to the buffer containing incoming network bytes.
 * @param len The size of incoming bytes.
 * @return true if more data is needed, false if complete/error (updates parser->bytes_consumed).
 */
bool ktc_req_line_parser_feed(ktc_req_line_parser_t *parser, const uint8_t *data, size_t len);

/**
 * @brief Resolves non-owning token string views relative to the given base buffer pointer.
 *
 * Translates parsed start and length indices into final `ktc_str` slices pointing to buffer memory.
 *
 * @param parser Pointer to the request line parser struct.
 * @param buf_base The starting base pointer of the accumulated network buffer.
 */
void ktc_req_line_parser_verify(ktc_req_line_parser_t *parser, const uint8_t *buf_base);

#endif /* KTC_HTTP_REQ_LINE_H */
