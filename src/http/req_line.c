#include "ktc/http/req_line.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_TARGET_LEN 8192
#define MAX_METHOD_LEN 32
#define MAX_PRE_REQUEST_JUNK 20

static bool is_token_char(uint8_t c) {
    // RFC 9110 token characters:
    // A-Z, a-z, 0-9, and !#$%&'*+-.^_`|~
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        return true;
    }
    const char *specials = "!#$%&'*+-.^_`|~";
    return strchr(specials, c) != NULL;
}

void ktc_req_line_parser_init(ktc_req_line_parser_t *parser) {
    memset(parser, 0, sizeof(*parser));
    parser->state = KTC_REQ_LINE_STATE_IDLE;
    parser->error = KTC_REQ_LINE_ERR_NONE;
}

bool ktc_req_line_parser_feed(ktc_req_line_parser_t *parser, const uint8_t *data, size_t len) {
    if (parser->state == KTC_REQ_LINE_STATE_COMPLETE || parser->state == KTC_REQ_LINE_STATE_ERROR) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        size_t current_idx = parser->bytes_consumed;
        parser->bytes_consumed++;

        switch (parser->state) {
        case KTC_REQ_LINE_STATE_IDLE:
            if (c == '\r') {
                parser->skipped_leading++;
                if (parser->skipped_leading > MAX_PRE_REQUEST_JUNK) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->state = KTC_REQ_LINE_STATE_SKIP_EMPTY;
            } else if (c == '\n') {
                parser->skipped_leading++;
                if (parser->skipped_leading > MAX_PRE_REQUEST_JUNK) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
            } else if (c == ' ' || c == '\t') {
                // Leading space before method is invalid
                parser->state = KTC_REQ_LINE_STATE_ERROR;
                parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                return false;
            } else {
                // First char of method
                if (!is_token_char(c)) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->method_start = current_idx;
                parser->state = KTC_REQ_LINE_STATE_METHOD;
            }
            break;

        case KTC_REQ_LINE_STATE_SKIP_EMPTY:
            if (c == '\n') {
                parser->state = KTC_REQ_LINE_STATE_IDLE;
            } else {
                // \r must be followed by \n
                parser->state = KTC_REQ_LINE_STATE_ERROR;
                parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                return false;
            }
            break;

        case KTC_REQ_LINE_STATE_METHOD:
            if (c == ' ') {
                parser->method_len = current_idx - parser->method_start;
                if (parser->method_len == 0) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
                if (parser->method_len > MAX_METHOD_LEN) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->state = KTC_REQ_LINE_STATE_TARGET;
            } else {
                if (!is_token_char(c)) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
            }
            break;

        case KTC_REQ_LINE_STATE_TARGET:
            if (parser->target_len == 0) {
                // Initialize target start
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->target_start = current_idx;
                parser->target_len = 1;
            } else {
                if (c == ' ') {
                    parser->target_len = current_idx - parser->target_start;
                    parser->state = KTC_REQ_LINE_STATE_VERSION;
                } else if (c == '\r' || c == '\n' || c < 0x20 || c == 0x7F) {
                    // Control chars and spaces not allowed in URI
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                } else {
                    size_t current_len = current_idx + 1 - parser->target_start;
                    if (current_len > MAX_TARGET_LEN) {
                        parser->state = KTC_REQ_LINE_STATE_ERROR;
                        parser->error = KTC_REQ_LINE_ERR_URI_TOO_LONG;
                        return false;
                    }
                    parser->target_len = current_len;
                }
            }
            break;

        case KTC_REQ_LINE_STATE_VERSION:
            if (parser->version_len == 0) {
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->version_start = current_idx;
                parser->version_len = 1;
            } else {
                if (c == '\r') {
                    parser->version_len = current_idx - parser->version_start;
                    parser->state = KTC_REQ_LINE_STATE_CRLF;
                } else if (c == ' ' || c == '\t' || c == '\n' || c < 0x20 || c == 0x7F) {
                    parser->state = KTC_REQ_LINE_STATE_ERROR;
                    parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                    return false;
                } else {
                    parser->version_len = current_idx + 1 - parser->version_start;
                }
            }
            break;

        case KTC_REQ_LINE_STATE_CRLF:
            if (c == '\n') {
                parser->state = KTC_REQ_LINE_STATE_COMPLETE;
                return false; // Complete parsing
            } else {
                parser->state = KTC_REQ_LINE_STATE_ERROR;
                parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
                return false;
            }

        default:
            parser->state = KTC_REQ_LINE_STATE_ERROR;
            parser->error = KTC_REQ_LINE_ERR_BAD_SYNTAX;
            return false;
        }
    }

    return true;
}

void ktc_req_line_parser_resolve(ktc_req_line_parser_t *parser, const uint8_t *buf_base) {
    if (parser->state == KTC_REQ_LINE_STATE_COMPLETE) {
        parser->method = ktc_str_from(buf_base + parser->method_start, parser->method_len);
        parser->target = ktc_str_from(buf_base + parser->target_start, parser->target_len);
        parser->version = ktc_str_from(buf_base + parser->version_start, parser->version_len);

        // Validate HTTP version format (H11-VERSION-001 / RFC 9112 §2.3)
        // Must be exactly "HTTP/1.1" or "HTTP/1.0"
        if (!ktc_str_eq_cstr(parser->version, "HTTP/1.1") &&
            !ktc_str_eq_cstr(parser->version, "HTTP/1.0")) {
            parser->state = KTC_REQ_LINE_STATE_ERROR;
            parser->error = KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED;
            parser->method = ktc_str_null();
            parser->target = ktc_str_null();
            parser->version = ktc_str_null();
        }
    } else {
        parser->method = ktc_str_null();
        parser->target = ktc_str_null();
        parser->version = ktc_str_null();
    }
}
