#include "ktc/http/headers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_HEADERS_SIZE 16384

static bool is_token_char(uint8_t c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
        return true;
    }
    const char *specials = "!#$%&'*+-.^_`|~";
    return strchr(specials, c) != NULL;
}

static bool starts_with_case_insensitive(const uint8_t *ptr, size_t len, const char *prefix,
                                         size_t prefix_len) {
    if (len < prefix_len) {
        return false;
    }
    for (size_t i = 0; i < prefix_len; i++) {
        uint8_t c1 = ptr[i];
        uint8_t c2 = (uint8_t)prefix[i];
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = (uint8_t)(c1 + ('a' - 'A'));
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = (uint8_t)(c2 + ('a' - 'A'));
        }
        if (c1 != c2) {
            return false;
        }
    }
    return true;
}

void ktc_header_parser_init(ktc_header_parser_t *parser) {
    memset(parser, 0, sizeof(*parser));
    parser->state = KTC_HEADER_STATE_NAME;
    parser->error = KTC_HEADER_ERR_NONE;
}

bool ktc_header_parser_feed(ktc_header_parser_t *parser, const uint8_t *data, size_t len) {
    if (parser->state == KTC_HEADER_STATE_COMPLETE || parser->state == KTC_HEADER_STATE_ERROR) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        size_t current_idx = parser->bytes_consumed;
        parser->bytes_consumed++;
        parser->total_headers_size++;

        if (parser->total_headers_size > MAX_HEADERS_SIZE) {
            parser->state = KTC_HEADER_STATE_ERROR;
            parser->error = KTC_HEADER_ERR_TOO_LARGE;
            return false;
        }

        switch (parser->state) {
        case KTC_HEADER_STATE_NAME:
            if (c == ':') {
                if (parser->current_name_len == 0) {
                    parser->state = KTC_HEADER_STATE_ERROR;
                    parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->state = KTC_HEADER_STATE_VALUE_START;
            } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                // Space before colon is disallowed (H11-HDR-001)
                parser->state = KTC_HEADER_STATE_ERROR;
                parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                return false;
            } else {
                if (!is_token_char(c)) {
                    parser->state = KTC_HEADER_STATE_ERROR;
                    parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                    return false;
                }
                if (parser->current_name_len == 0) {
                    parser->current_name_start = current_idx;
                }
                parser->current_name_len++;
            }
            break;

        case KTC_HEADER_STATE_VALUE_START:
            if (c == ' ' || c == '\t') {
                // Skip optional whitespace (OWS)
            } else if (c == '\r') {
                parser->current_value_start = current_idx;
                parser->current_value_len = 0;
                parser->state = KTC_HEADER_STATE_CR;
            } else if (c < 0x20 || c == 0x7F) {
                parser->state = KTC_HEADER_STATE_ERROR;
                parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                return false;
            } else {
                parser->current_value_start = current_idx;
                parser->current_value_len = 1;
                parser->state = KTC_HEADER_STATE_VALUE;
            }
            break;

        case KTC_HEADER_STATE_VALUE:
            if (c == '\r') {
                parser->state = KTC_HEADER_STATE_CR;
            } else if (c < 0x20 || c == 0x7F) {
                if (c == '\t') {
                    parser->current_value_len++;
                } else {
                    // Control characters/NUL/bare LFs are invalid (H11-HDR-003)
                    parser->state = KTC_HEADER_STATE_ERROR;
                    parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                    return false;
                }
            } else {
                parser->current_value_len++;
            }
            break;

        case KTC_HEADER_STATE_CR:
            if (c == '\n') {
                if (parser->header_count >= KTC_MAX_HEADERS) {
                    parser->state = KTC_HEADER_STATE_ERROR;
                    parser->error = KTC_HEADER_ERR_TOO_LARGE;
                    return false;
                }

                size_t idx = parser->header_count;
                parser->name_start[idx] = parser->current_name_start;
                parser->name_len[idx] = parser->current_name_len;
                parser->value_start[idx] = parser->current_value_start;
                parser->value_len[idx] = parser->current_value_len;
                parser->header_count++;

                parser->current_name_len = 0;
                parser->current_value_len = 0;

                parser->state = KTC_HEADER_STATE_CRLF;
            } else {
                parser->state = KTC_HEADER_STATE_ERROR;
                parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                return false;
            }
            break;

        case KTC_HEADER_STATE_CRLF:
            if (c == '\r') {
                parser->state = KTC_HEADER_STATE_DOUBLE_CRLF;
            } else if (c == ' ' || c == '\t') {
                // Obsolete line folding (obs-fold) is invalid (H11-HDR-002)
                parser->state = KTC_HEADER_STATE_ERROR;
                parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                return false;
            } else {
                if (!is_token_char(c)) {
                    parser->state = KTC_HEADER_STATE_ERROR;
                    parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                    return false;
                }
                parser->current_name_start = current_idx;
                parser->current_name_len = 1;
                parser->state = KTC_HEADER_STATE_NAME;
            }
            break;

        case KTC_HEADER_STATE_DOUBLE_CRLF:
            if (c == '\n') {
                parser->state = KTC_HEADER_STATE_COMPLETE;
                return false; // Done parsing header block
            } else {
                parser->state = KTC_HEADER_STATE_ERROR;
                parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
                return false;
            }

        default:
            parser->state = KTC_HEADER_STATE_ERROR;
            parser->error = KTC_HEADER_ERR_BAD_SYNTAX;
            return false;
        }
    }

    return true;
}

bool ktc_header_parser_resolve_and_validate(ktc_header_parser_t *parser, const uint8_t *buf_base,
                                            ktc_str request_target) {
    if (parser->state != KTC_HEADER_STATE_COMPLETE) {
        return false;
    }

    // Resolve header views and strip trailing spaces
    for (size_t i = 0; i < parser->header_count; i++) {
        parser->headers[i].name =
            ktc_str_from(buf_base + parser->name_start[i], parser->name_len[i]);

        size_t vstart = parser->value_start[i];
        size_t vlen = parser->value_len[i];
        while (vlen > 0 &&
               (buf_base[vstart + vlen - 1] == ' ' || buf_base[vstart + vlen - 1] == '\t')) {
            vlen--;
        }
        parser->headers[i].value = ktc_str_from(buf_base + vstart, vlen);
    }

    // Check absolute-form target URI (H11-HOST-002)
    // If request_target starts with "http://" or "https://", parse host from URI
    bool is_absolute = false;
    if (starts_with_case_insensitive(request_target.ptr, request_target.len, "http://", 7) ||
        starts_with_case_insensitive(request_target.ptr, request_target.len, "https://", 8)) {
        is_absolute = true;
    }

    if (is_absolute) {
        // Extract authority from absolute-form target URI
        const uint8_t *p = request_target.ptr;
        size_t rem = request_target.len;
        if (starts_with_case_insensitive(p, rem, "http://", 7)) {
            p += 7;
            rem -= 7;
        } else {
            p += 8;
            rem -= 8;
        }
        // Find next '/' or end of target URI
        const uint8_t *slash = memchr(p, '/', rem);
        size_t host_len = slash ? (size_t)(slash - p) : rem;
        parser->host = ktc_str_from(p, host_len);
        return true;
    }

    // Otherwise, validate the Host header (H11-HOST-001)
    bool host_found = false;
    for (size_t i = 0; i < parser->header_count; i++) {
        if (ktc_str_eq_case_insensitive(parser->headers[i].name, ktc_str_from_cstr("Host"))) {
            if (host_found) {
                parser->error = KTC_HEADER_ERR_DUPLICATE_HOST;
                return false;
            }
            host_found = true;
            parser->host = parser->headers[i].value;
        }
    }

    if (!host_found) {
        parser->error = KTC_HEADER_ERR_MISSING_HOST;
        return false;
    }

    return true;
}
