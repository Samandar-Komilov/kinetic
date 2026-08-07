#include "ktc/http/body.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define KTC_MAX_BODY_SIZE 10485760 // 10MB Cap

void ktc_body_parser_init(ktc_body_parser_t *parser) {
    memset(parser, 0, sizeof(*parser));
    parser->framing = KTC_BODY_FRAMING_NONE;
    parser->chunk_parser.state = KTC_CHUNK_STATE_SIZE;
}

bool ktc_body_resolve_framing(ktc_body_parser_t *parser, const ktc_header_parser_t *header_parser,
                              ktc_str method) {
    bool has_te = false;
    bool has_cl = false;
    ktc_str te_val = ktc_str_null();
    ktc_str cl_val = ktc_str_null();

    for (size_t i = 0; i < header_parser->header_count; i++) {
        if (ktc_str_eq_case_insensitive(header_parser->headers[i].name,
                                        ktc_str_from_cstr("Transfer-Encoding"))) {
            has_te = true;
            te_val = header_parser->headers[i].value;
        }
        if (ktc_str_eq_case_insensitive(header_parser->headers[i].name,
                                        ktc_str_from_cstr("Content-Length"))) {
            if (has_cl) {
                return false; // Duplicate Content-Length is invalid (H11-FRAME-003)
            }
            has_cl = true;
            cl_val = header_parser->headers[i].value;
        }
    }

    // 1. Transfer-Encoding and Content-Length both present (H11-FRAME-004 / H11-SEC-003)
    if (has_te && has_cl) {
        return false; // Reject ambiguous framing immediately to prevent smuggling
    }

    // 2. HEAD requests must not contain a request body (H11-FRAME-001)
    if (ktc_str_eq_case_insensitive(method, ktc_str_from_cstr("HEAD"))) {
        parser->framing = KTC_BODY_FRAMING_NONE;
        return true;
    }

    if (has_te) {
        // Validate that chunked is the final encoding and occurs exactly once (H11-FRAME-002 / Bug
        // 3)
        size_t start = 0;
        bool seen_chunked = false;
        bool last_was_chunked = false;

        while (start < te_val.len) {
            size_t end = start;
            while (end < te_val.len && te_val.ptr[end] != ',') {
                end++;
            }

            ktc_str token = ktc_str_from(te_val.ptr + start, end - start);
            while (token.len > 0 && (token.ptr[0] == ' ' || token.ptr[0] == '\t')) {
                token.ptr++;
                token.len--;
            }
            while (token.len > 0 &&
                   (token.ptr[token.len - 1] == ' ' || token.ptr[token.len - 1] == '\t')) {
                token.len--;
            }

            if (ktc_str_eq_case_insensitive(token, ktc_str_from_cstr("chunked"))) {
                if (seen_chunked) {
                    return false; // chunked applied more than once
                }
                seen_chunked = true;
                last_was_chunked = true;
            } else {
                last_was_chunked = false;
            }

            start = end + 1;
        }

        if (seen_chunked && last_was_chunked) {
            parser->framing = KTC_BODY_FRAMING_CHUNKED;
            parser->chunk_parser.state = KTC_CHUNK_STATE_SIZE;
            return true;
        }

        return false; // chunked present but not final, or other encoding final
    }

    if (has_cl) {
        if (cl_val.len == 0) {
            return false;
        }
        size_t cl_len = 0;
        for (size_t i = 0; i < cl_val.len; i++) {
            if (cl_val.ptr[i] < '0' || cl_val.ptr[i] > '9') {
                return false; // Invalid Content-Length format (H11-FRAME-003)
            }
            size_t next_val = cl_len * 10 + (size_t)(cl_val.ptr[i] - '0');
            if (next_val < cl_len) {
                return false; // Overflow (H11-FRAME-003)
            }
            cl_len = next_val;
        }

        if (cl_len > KTC_MAX_BODY_SIZE) {
            return false; // Content-Length exceeds max body size cap (Bug 4)
        }

        parser->framing = KTC_BODY_FRAMING_LENGTH;
        parser->content_length = cl_len;
        return true;
    }

    // Default framing is no body
    parser->framing = KTC_BODY_FRAMING_NONE;
    return true;
}

bool ktc_body_parser_feed(ktc_body_parser_t *parser, const uint8_t *data, size_t len,
                          uint8_t *out_buf, size_t *out_len, size_t max_len) {
    if (parser->framing == KTC_BODY_FRAMING_NONE) {
        return false;
    }

    if (parser->framing == KTC_BODY_FRAMING_LENGTH) {
        if (parser->body_consumed >= parser->content_length) {
            return false;
        }
        size_t rem = parser->content_length - parser->body_consumed;
        size_t chunk = len < rem ? len : rem;

        if (*out_len + chunk > max_len) {
            return false; // Output buffer overflow protection
        }

        memcpy(out_buf + *out_len, data, chunk);
        *out_len += chunk;
        parser->body_consumed += chunk;

        return parser->body_consumed < parser->content_length;
    }

    // Framing is chunked (KTC_BODY_FRAMING_CHUNKED)
    ktc_chunk_parser_t *cp = &parser->chunk_parser;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        parser->body_consumed++;

        switch (cp->state) {
        case KTC_CHUNK_STATE_SIZE:
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                size_t digit = 0;
                if (c >= '0' && c <= '9') {
                    digit = (size_t)c - (size_t)'0';
                } else if (c >= 'a' && c <= 'f') {
                    digit = (size_t)c - (size_t)'a' + 10;
                } else {
                    digit = (size_t)c - (size_t)'A' + 10;
                }

                // Guard against hex size overflow (H11-CHUNK-003)
                if (cp->chunk_size > (SIZE_MAX >> 4)) {
                    cp->state = KTC_CHUNK_STATE_ERROR;
                    return false;
                }
                cp->chunk_size = (cp->chunk_size << 4) | digit;
            } else if (c == ';') {
                cp->state = KTC_CHUNK_STATE_EXTENSION;
            } else if (c == '\r') {
                cp->state = KTC_CHUNK_STATE_SIZE_CRLF;
            } else {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            break;

        case KTC_CHUNK_STATE_EXTENSION:
            if (c == '\r') {
                cp->state = KTC_CHUNK_STATE_SIZE_CRLF;
            } else if (c == '\n') {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            // Silently skip chunk extension characters (H11-CHUNK-002)
            break;

        case KTC_CHUNK_STATE_SIZE_CRLF:
            if (c == '\n') {
                if (cp->chunk_size == 0) {
                    cp->state = KTC_CHUNK_STATE_TRAILERS_START;
                    cp->trailers_crlf_count = 0;
                    cp->last_was_cr = false;
                } else {
                    cp->chunk_remaining = cp->chunk_size;
                    cp->state = KTC_CHUNK_STATE_DATA;
                }
            } else {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            break;

        case KTC_CHUNK_STATE_DATA:
            if (*out_len >= max_len) {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            out_buf[*out_len] = c;
            (*out_len)++;
            cp->chunk_remaining--;

            if (cp->chunk_remaining == 0) {
                cp->state = KTC_CHUNK_STATE_DATA_CRLF;
            }
            break;

        case KTC_CHUNK_STATE_DATA_CRLF:
            if (c == '\r') {
                // Awaiting LF
            } else if (c == '\n') {
                cp->chunk_size = 0;
                cp->state = KTC_CHUNK_STATE_SIZE;
            } else {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            break;

        case KTC_CHUNK_STATE_TRAILERS_START:
            if (c == '\r') {
                cp->state = KTC_CHUNK_STATE_TRAILERS_FINAL_LF;
            } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                       c == '-' || c == '_') {
                // Start of trailer line field-name
                cp->state = KTC_CHUNK_STATE_TRAILERS_DATA;
            } else {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            break;

        case KTC_CHUNK_STATE_TRAILERS_DATA:
            if (c == '\r') {
                cp->state = KTC_CHUNK_STATE_TRAILERS_CRLF;
            } else if (c < 0x20 || c == 0x7F) {
                // Control character inside trailer line is invalid
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            break;

        case KTC_CHUNK_STATE_TRAILERS_CRLF:
            if (c == '\n') {
                cp->state = KTC_CHUNK_STATE_TRAILERS_START;
            } else {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }
            break;

        case KTC_CHUNK_STATE_TRAILERS_FINAL_LF:
            if (c == '\n') {
                cp->state = KTC_CHUNK_STATE_COMPLETE;
                return false; // Finished chunked block successfully
            } else {
                cp->state = KTC_CHUNK_STATE_ERROR;
                return false;
            }

        default:
            cp->state = KTC_CHUNK_STATE_ERROR;
            return false;
        }
    }

    return cp->state != KTC_CHUNK_STATE_COMPLETE && cp->state != KTC_CHUNK_STATE_ERROR;
}
