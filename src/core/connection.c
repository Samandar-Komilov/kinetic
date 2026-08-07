#include <stdlib.h>
#include <string.h>

#include "ktc/core/arena.h"
#include "ktc/core/connection.h"
#include "ktc/external/log.h"
#include "ktc/http/body.h"
#include "ktc/http/headers.h"
#include "ktc/http/req_line.h"
#include "ktc/http/response.h"

#define KTC_CONN_ARENA_INITIAL_SIZE 4096

static bool g_shutting_down = false;

typedef enum {
    KTC_CONN_STATE_REQ_LINE,
    KTC_CONN_STATE_HEADERS,
    KTC_CONN_STATE_BODY,
    KTC_CONN_STATE_COMPLETE,
    KTC_CONN_STATE_ERROR
} ktc_conn_parse_state_t;

typedef struct ktc_conn_t {
    uv_tcp_t client_handle;
    ktc_arena_t *arena;
    bool is_closing;
    int pending_closes_cnt;

    // HTTP Parser State
    ktc_conn_parse_state_t parse_state;
    ktc_req_line_parser_t req_line_parser;
    ktc_header_parser_t header_parser;
    ktc_body_parser_t body_parser;

    char *buf;
    size_t len;
    size_t cap;

    // Request Payload accumulator
    uint8_t *payload;
    size_t payload_len;
    size_t payload_cap;
} ktc_conn_t;

typedef struct {
    uv_write_t req;
    char *base;
    ktc_conn_t *conn;
} ktc_write_req_t;

void ktc_connections_set_shutting_down(bool is_shutting_down) {
    g_shutting_down = is_shutting_down;
}

static void on_handle_closed(uv_handle_t *h) {
    ktc_conn_t *c = h->data;
    if (--c->pending_closes_cnt == 0) {
        if (c->buf) {
            free(c->buf);
        }
        if (c->payload) {
            free(c->payload);
        }
        if (c->arena) {
            ktc_arena_destroy(c->arena);
        }
        free(c);
    }
}

static void conn_close(ktc_conn_t *c) {
    if (c->is_closing) {
        return;
    }
    c->is_closing = true;

    uv_read_stop((uv_stream_t *)&c->client_handle);

    c->pending_closes_cnt = 1;
    uv_close((uv_handle_t *)&c->client_handle, on_handle_closed);
}

static void on_write(uv_write_t *req, int status) {
    ktc_write_req_t *wr = (ktc_write_req_t *)req;
    if (status < 0) {
        log_error("Write failed: %s", uv_strerror(status));
    }
    ktc_conn_t *c = wr->conn;
    free(wr->base);
    free(wr);
    conn_close(c);
}

static void on_alloc(uv_handle_t *handle, size_t suggested, uv_buf_t *buf) {
    (void)handle;
    buf->base = malloc(suggested);
    buf->len = buf->base ? suggested : 0;
}

static void send_response(uv_stream_t *stream, ktc_conn_t *c, int status_code, const char *phrase) {
    char response[1024];
    size_t resp_len = ktc_response_format_empty(response, sizeof(response), status_code, phrase);
    if (resp_len > 0) {
        ktc_write_req_t *wr = malloc(sizeof(ktc_write_req_t));
        if (wr) {
            wr->base = malloc(resp_len + 1);
            memcpy(wr->base, response, resp_len + 1);
            wr->conn = c;
            uv_buf_t wbuf = uv_buf_init(wr->base, (unsigned int)resp_len);
            int r = uv_write(&wr->req, stream, &wbuf, 1, on_write);
            if (r) {
                log_error("uv_write failed: %s", uv_strerror(r));
                free(wr->base);
                free(wr);
                conn_close(c);
            }
        } else {
            conn_close(c);
        }
    } else {
        conn_close(c);
    }
}

static bool accumulate_connection_buffer(ktc_conn_t *c, const char *data, size_t nread,
                                         size_t *out_prev_len) {
    if (c->len + nread + 1 > c->cap) {
        size_t ncap = c->cap ? c->cap * 2 : 1024;
        while (ncap < c->len + nread + 1) {
            ncap *= 2;
        }
        char *nb = realloc(c->buf, ncap);
        if (!nb) {
            log_error("Buffer realloc failed");
            return false;
        }
        c->buf = nb;
        c->cap = ncap;
    }

    *out_prev_len = c->len;
    memcpy(c->buf + c->len, data, nread);
    c->len += nread;
    c->buf[c->len] = '\0';
    return true;
}

static void handle_request_line(ktc_conn_t *c, uv_stream_t *stream, size_t prev_len, size_t nread) {
    bool feeding =
        ktc_req_line_parser_feed(&c->req_line_parser, (const uint8_t *)(c->buf + prev_len), nread);
    if (!feeding) {
        if (c->req_line_parser.state == KTC_REQ_LINE_STATE_COMPLETE) {
            ktc_req_line_parser_resolve(&c->req_line_parser, (const uint8_t *)c->buf);
            if (c->req_line_parser.state == KTC_REQ_LINE_STATE_COMPLETE) {
                log_info(
                    "Parsed request line: Method=%.*s, Target=%.*s, Version=%.*s",
                    (int)c->req_line_parser.method.len, (const char *)c->req_line_parser.method.ptr,
                    (int)c->req_line_parser.target.len, (const char *)c->req_line_parser.target.ptr,
                    (int)c->req_line_parser.version.len,
                    (const char *)c->req_line_parser.version.ptr);

                c->parse_state = KTC_CONN_STATE_HEADERS;
                ktc_header_parser_init(&c->header_parser);
            } else {
                int err_code = 400;
                const char *phrase = "Bad Request";
                if (c->req_line_parser.error == KTC_REQ_LINE_ERR_VERSION_NOT_SUPPORTED) {
                    err_code = 505;
                    phrase = "HTTP Version Not Supported";
                }
                log_error("Request line resolution error: %d", c->req_line_parser.error);
                c->parse_state = KTC_CONN_STATE_ERROR;
                uv_read_stop(stream);
                send_response(stream, c, err_code, phrase);
            }
        } else {
            int err_code = 400;
            const char *phrase = "Bad Request";
            if (c->req_line_parser.error == KTC_REQ_LINE_ERR_URI_TOO_LONG) {
                err_code = 414;
                phrase = "URI Too Long";
            } else if (c->req_line_parser.error == KTC_REQ_LINE_ERR_METHOD_NOT_IMPLEMENTED) {
                err_code = 501;
                phrase = "Not Implemented";
            }

            log_error("Request line parser error: %d", c->req_line_parser.error);
            c->parse_state = KTC_CONN_STATE_ERROR;
            uv_read_stop(stream);
            send_response(stream, c, err_code, phrase);
        }
    }
}

static void handle_headers(ktc_conn_t *c, uv_stream_t *stream) {
    size_t consumed = c->req_line_parser.bytes_consumed + c->header_parser.bytes_consumed;
    size_t unparsed_len = c->len - consumed;

    if (unparsed_len > 0) {
        bool feeding = ktc_header_parser_feed(&c->header_parser,
                                              (const uint8_t *)(c->buf + consumed), unparsed_len);
        if (!feeding) {
            if (c->header_parser.state == KTC_HEADER_STATE_COMPLETE) {
                bool valid = ktc_header_parser_resolve_and_validate(
                    &c->header_parser,
                    (const uint8_t *)(c->buf + c->req_line_parser.bytes_consumed),
                    c->req_line_parser.target);
                if (valid) {
                    log_info("Headers successfully parsed and validated. Host: %.*s",
                             (int)c->header_parser.host.len,
                             (const char *)c->header_parser.host.ptr);

                    ktc_body_parser_init(&c->body_parser);
                    if (ktc_body_resolve_framing(&c->body_parser, &c->header_parser,
                                                 c->req_line_parser.method)) {
                        if (c->body_parser.framing == KTC_BODY_FRAMING_NONE) {
                            c->parse_state = KTC_CONN_STATE_COMPLETE;
                            uv_read_stop(stream);
                            send_response(stream, c, 200, "OK");
                        } else {
                            c->parse_state = KTC_CONN_STATE_BODY;
                            c->payload = NULL;
                            c->payload_len = 0;
                            c->payload_cap = 0;
                        }
                    } else {
                        log_error("Framing resolution failed (smuggling or invalid CL)");
                        c->parse_state = KTC_CONN_STATE_ERROR;
                        uv_read_stop(stream);
                        send_response(stream, c, 400, "Bad Request");
                    }
                } else {
                    int err_code = 400;
                    const char *phrase = "Bad Request";
                    log_error("Header validation failed: %d", c->header_parser.error);

                    c->parse_state = KTC_CONN_STATE_ERROR;
                    uv_read_stop(stream);
                    send_response(stream, c, err_code, phrase);
                }
            } else {
                int err_code = 400;
                const char *phrase = "Bad Request";
                if (c->header_parser.error == KTC_HEADER_ERR_TOO_LARGE) {
                    err_code = 431;
                    phrase = "Request Header Fields Too Large";
                }

                log_error("Header parser error: %d", c->header_parser.error);
                c->parse_state = KTC_CONN_STATE_ERROR;
                uv_read_stop(stream);
                send_response(stream, c, err_code, phrase);
            }
        }
    }
}

static void handle_body(ktc_conn_t *c, uv_stream_t *stream) {
    size_t consumed = c->req_line_parser.bytes_consumed + c->header_parser.bytes_consumed +
                      c->body_parser.body_consumed;
    size_t unparsed_len = c->len - consumed;

    if (unparsed_len > 0) {
        size_t max_body_size = 10485760; // 10MB Cap
        if (c->payload_len + unparsed_len > max_body_size) {
            log_error("Request body exceeds maximum size limit (10MB)");
            c->parse_state = KTC_CONN_STATE_ERROR;
            uv_read_stop(stream);
            send_response(stream, c, 413, "Content Too Large");
            return;
        }

        if (c->payload_len + unparsed_len > c->payload_cap) {
            size_t ncap = c->payload_cap ? c->payload_cap * 2 : 1024;
            while (ncap < c->payload_len + unparsed_len) {
                ncap *= 2;
            }
            uint8_t *np = realloc(c->payload, ncap);
            if (!np) {
                log_error("Payload buffer realloc failed");
                conn_close(c);
                return;
            }
            c->payload = np;
            c->payload_cap = ncap;
        }

        bool feeding =
            ktc_body_parser_feed(&c->body_parser, (const uint8_t *)(c->buf + consumed),
                                 unparsed_len, c->payload, &c->payload_len, c->payload_cap);
        if (!feeding) {
            if (c->body_parser.framing == KTC_BODY_FRAMING_LENGTH &&
                c->body_parser.body_consumed == c->body_parser.content_length) {
                log_info("Body fully parsed (length=%zu)", c->payload_len);
                c->parse_state = KTC_CONN_STATE_COMPLETE;
                uv_read_stop(stream);
                send_response(stream, c, 200, "OK");
            } else if (c->body_parser.framing == KTC_BODY_FRAMING_CHUNKED &&
                       c->body_parser.chunk_parser.state == KTC_CHUNK_STATE_COMPLETE) {
                log_info("Chunked body fully decoded (payload_len=%zu)", c->payload_len);
                c->parse_state = KTC_CONN_STATE_COMPLETE;
                uv_read_stop(stream);
                send_response(stream, c, 200, "OK");
            } else {
                log_error("Body parsing error. Framing: %d, FSM state: %d", c->body_parser.framing,
                          c->body_parser.chunk_parser.state);
                c->parse_state = KTC_CONN_STATE_ERROR;
                uv_read_stop(stream);
                send_response(stream, c, 400, "Bad Request");
            }
        }
    }
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    ktc_conn_t *c = stream->data;

    if (nread > 0) {
        size_t prev_len = 0;
        if (!accumulate_connection_buffer(c, buf->base, (size_t)nread, &prev_len)) {
            free(buf->base);
            conn_close(c);
            return;
        }

        // 1. Process Request Line
        if (c->parse_state == KTC_CONN_STATE_REQ_LINE) {
            handle_request_line(c, stream, prev_len, (size_t)nread);
            if (c->parse_state == KTC_CONN_STATE_ERROR || c->is_closing) {
                free(buf->base);
                return;
            }
        }

        // 2. Process Headers (if request line is parsed)
        if (c->parse_state == KTC_CONN_STATE_HEADERS) {
            handle_headers(c, stream);
            if (c->parse_state == KTC_CONN_STATE_ERROR || c->is_closing) {
                free(buf->base);
                return;
            }
        }

        // 3. Process Body
        if (c->parse_state == KTC_CONN_STATE_BODY) {
            handle_body(c, stream);
            if (c->parse_state == KTC_CONN_STATE_ERROR || c->is_closing) {
                free(buf->base);
                return;
            }
        }

        free(buf->base);
    } else {
        if (nread == UV_EOF) {
            log_info("Client disconnected cleanly (EOF)");
        } else if (nread < 0) {
            log_error("Read error: %s", uv_strerror((int)nread));
        }
        free(buf->base);
        conn_close(c);
    }
}

void ktc_on_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        log_error("Connection error: %s", uv_strerror(status));
        return;
    }
    if (g_shutting_down) {
        return;
    }

    uv_loop_t *loop = server->loop;

    // Allocate connection context. Context owns the arena.
    ktc_conn_t *c = calloc(1, sizeof(*c));
    if (!c) {
        log_error("Failed to allocate connection context");
        return;
    }

    c->arena = ktc_arena_create(KTC_CONN_ARENA_INITIAL_SIZE);
    if (!c->arena) {
        log_error("Failed to create connection arena");
        free(c);
        return;
    }

    c->parse_state = KTC_CONN_STATE_REQ_LINE;
    ktc_req_line_parser_init(&c->req_line_parser);
    c->buf = NULL;
    c->len = 0;
    c->cap = 0;

    c->payload = NULL;
    c->payload_len = 0;
    c->payload_cap = 0;

    uv_tcp_init(loop, &c->client_handle);
    c->client_handle.data = c;

    if (uv_accept(server, (uv_stream_t *)&c->client_handle) == 0) {
        // Enable TCP_NODELAY, Win on latency (no Nagle's Algorithm)
        // Small data packets won't be delayed to group into larger segments
        uv_tcp_nodelay(&c->client_handle, 1);
        uv_os_fd_t fd = -1;
        uv_fileno((uv_handle_t *)&c->client_handle, &fd);
        log_info("Accepted connection (fd=%d)", (int)fd);

        uv_read_start((uv_stream_t *)&c->client_handle, on_alloc, on_read);
    } else {
        conn_close(c);
    }
}