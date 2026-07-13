#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <yaml.h>

#include "ktc/core/arena.h"
#include "ktc/core/config.h"
#include "ktc/external/log.h"
#include "ktc/http/headers.h"
#include "ktc/http/req_line.h"
#include "ktc/http/response.h"

typedef struct {
    uv_loop_t *loop;
    uv_tcp_t server;
    uv_signal_t sig_int, sig_term, sig_hup;
    bool is_shutting_down;
} ktc_app_t;

typedef enum {
    KTC_CONN_STATE_REQ_LINE,
    KTC_CONN_STATE_HEADERS,
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

    char *buf;
    size_t len;
    size_t cap;
} ktc_conn_t;

typedef struct {
    uv_write_t req;
    char *base;
    ktc_conn_t *conn;
} ktc_write_req_t;

static ktc_app_t app;

static void on_handle_closed(uv_handle_t *h) {
    ktc_conn_t *c = h->data;
    if (--c->pending_closes_cnt == 0) {
        if (c->buf) {
            free(c->buf);
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

static void send_lawful_response(uv_stream_t *stream, ktc_conn_t *c, int status_code,
                                 const char *phrase) {
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

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    ktc_conn_t *c = stream->data;

    if (nread > 0) {
        // Accumulate read data into connection buffer
        if (c->len + (size_t)nread + 1 > c->cap) {
            size_t ncap = c->cap ? c->cap * 2 : 1024;
            while (ncap < c->len + (size_t)nread + 1) {
                ncap *= 2;
            }
            char *nb = realloc(c->buf, ncap);
            if (!nb) {
                log_error("Buffer realloc failed");
                free(buf->base);
                conn_close(c);
                return;
            }
            c->buf = nb;
            c->cap = ncap;
        }

        memcpy(c->buf + c->len, buf->base, (size_t)nread);
        size_t prev_len = c->len;
        c->len += (size_t)nread;
        c->buf[c->len] = '\0';

        // 1. Process Request Line
        if (c->parse_state == KTC_CONN_STATE_REQ_LINE) {
            bool feeding = ktc_req_line_parser_feed(
                &c->req_line_parser, (const uint8_t *)(c->buf + prev_len), (size_t)nread);
            if (!feeding) {
                if (c->req_line_parser.state == KTC_REQ_LINE_STATE_COMPLETE) {
                    ktc_req_line_parser_resolve(&c->req_line_parser, (const uint8_t *)c->buf);
                    log_info("Parsed request line: Method=%.*s, Target=%.*s, Version=%.*s",
                             (int)c->req_line_parser.method.len,
                             (const char *)c->req_line_parser.method.ptr,
                             (int)c->req_line_parser.target.len,
                             (const char *)c->req_line_parser.target.ptr,
                             (int)c->req_line_parser.version.len,
                             (const char *)c->req_line_parser.version.ptr);

                    // Transition to Headers State
                    c->parse_state = KTC_CONN_STATE_HEADERS;
                    ktc_header_parser_init(&c->header_parser);
                } else {
                    // Request line error
                    int err_code = 400;
                    const char *phrase = "Bad Request";
                    if (c->req_line_parser.error == KTC_REQ_LINE_ERR_URI_TOO_LONG) {
                        err_code = 414;
                        phrase = "URI Too Long";
                    } else if (c->req_line_parser.error ==
                               KTC_REQ_LINE_ERR_METHOD_NOT_IMPLEMENTED) {
                        err_code = 501;
                        phrase = "Not Implemented";
                    }

                    log_error("Request line parser error: %d", c->req_line_parser.error);
                    c->parse_state = KTC_CONN_STATE_ERROR;
                    uv_read_stop(stream);
                    send_lawful_response(stream, c, err_code, phrase);
                    free(buf->base);
                    return;
                }
            }
        }

        // 2. Process Headers (if request line is parsed)
        if (c->parse_state == KTC_CONN_STATE_HEADERS) {
            size_t consumed = c->req_line_parser.bytes_consumed + c->header_parser.bytes_consumed;
            size_t unparsed_len = c->len - consumed;

            if (unparsed_len > 0) {
                bool feeding = ktc_header_parser_feed(
                    &c->header_parser, (const uint8_t *)(c->buf + consumed), unparsed_len);
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

                            c->parse_state = KTC_CONN_STATE_COMPLETE;
                            uv_read_stop(stream);
                            send_lawful_response(stream, c, 200, "OK");
                        } else {
                            // Header validation failed (e.g. Host invariants)
                            int err_code = 400;
                            const char *phrase = "Bad Request";
                            log_error("Header validation failed: %d", c->header_parser.error);

                            c->parse_state = KTC_CONN_STATE_ERROR;
                            uv_read_stop(stream);
                            send_lawful_response(stream, c, err_code, phrase);
                        }
                    } else {
                        // Header parser error (e.g. formatting error, obs-fold)
                        int err_code = 400;
                        const char *phrase = "Bad Request";
                        if (c->header_parser.error == KTC_HEADER_ERR_TOO_LARGE) {
                            err_code = 431;
                            phrase = "Request Header Fields Too Large";
                        }

                        log_error("Header parser error: %d", c->header_parser.error);
                        c->parse_state = KTC_CONN_STATE_ERROR;
                        uv_read_stop(stream);
                        send_lawful_response(stream, c, err_code, phrase);
                    }
                }
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

static void on_connection(uv_stream_t *server, int status) {
    if (status < 0) {
        log_error("Connection error: %s", uv_strerror(status));
        return;
    }
    if (app.is_shutting_down) {
        return;
    }

    ktc_conn_t *c = calloc(1, sizeof(*c));
    if (!c) {
        log_error("Failed to allocate connection context");
        return;
    }

    c->arena = ktc_arena_create(1024);
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

    uv_tcp_init(app.loop, &c->client_handle);
    c->client_handle.data = c;

    if (uv_accept(server, (uv_stream_t *)&c->client_handle) == 0) {
        uv_os_fd_t fd = -1;
        uv_fileno((uv_handle_t *)&c->client_handle, &fd);
        log_info("Accepted connection (fd=%d)", (int)fd);

        uv_read_start((uv_stream_t *)&c->client_handle, on_alloc, on_read);
    } else {
        conn_close(c);
    }
}

static void close_walk_cb(uv_handle_t *handle, void *arg) {
    (void)arg;
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
}

static void on_signal(uv_signal_t *handle, int signum) {
    (void)handle;
    if (app.is_shutting_down) {
        return;
    }
    app.is_shutting_down = true;
    log_info("Caught signal %d, stopping server and draining...", signum);

    if (!uv_is_closing((uv_handle_t *)&app.server)) {
        uv_close((uv_handle_t *)&app.server, NULL);
    }

    uv_signal_stop(&app.sig_int);
    uv_close((uv_handle_t *)&app.sig_int, NULL);
    uv_signal_stop(&app.sig_term);
    uv_close((uv_handle_t *)&app.sig_term, NULL);
    uv_signal_stop(&app.sig_hup);
    uv_close((uv_handle_t *)&app.sig_hup, NULL);

    uv_stop(app.loop);
}

int main(int argc, char **argv) {
    const char *config_path = DEFAULT_STATIC_CONFIG_DIR;
    if (argc > 1) {
        config_path = argv[1];
    }

    ktc_config_t config;
    if (!ktc_config_parse(config_path, &config)) {
        log_error("Failed to parse config file: %s", config_path);
        return 1;
    }

    int level = LOG_INFO;
    if (strcmp(config.log_level, "TRACE") == 0) {
        level = LOG_TRACE;
    } else if (strcmp(config.log_level, "DEBUG") == 0) {
        level = LOG_DEBUG;
    } else if (strcmp(config.log_level, "INFO") == 0) {
        level = LOG_INFO;
    } else if (strcmp(config.log_level, "WARN") == 0) {
        level = LOG_WARN;
    } else if (strcmp(config.log_level, "ERROR") == 0) {
        level = LOG_ERROR;
    } else if (strcmp(config.log_level, "FATAL") == 0) {
        level = LOG_FATAL;
    }
    log_set_level(level);

    log_info("Loaded config. Port: %d, Log Level: %s", config.listen_port, config.log_level);

    app.loop = uv_default_loop();
    app.is_shutting_down = false;

    uv_tcp_init(app.loop, &app.server);

    struct sockaddr_in addr;
    int r = uv_ip4_addr("0.0.0.0", config.listen_port, &addr);
    if (r) {
        log_error("Invalid IP address: %s", uv_strerror(r));
        return 1;
    }

    r = uv_tcp_bind(&app.server, (const struct sockaddr *)&addr, 0);
    if (r) {
        log_error("Failed to bind to port %d: %s", config.listen_port, uv_strerror(r));
        return 1;
    }

    r = uv_listen((uv_stream_t *)&app.server, 128, on_connection);
    if (r) {
        log_error("Failed to listen: %s", uv_strerror(r));
        return 1;
    }

    log_info("Server listening on port %d", config.listen_port);

    uv_signal_init(app.loop, &app.sig_int);
    uv_signal_start(&app.sig_int, on_signal, SIGINT);
    uv_signal_init(app.loop, &app.sig_term);
    uv_signal_start(&app.sig_term, on_signal, SIGTERM);
    uv_signal_init(app.loop, &app.sig_hup);
    uv_signal_start(&app.sig_hup, on_signal, SIGHUP);

    uv_run(app.loop, UV_RUN_DEFAULT);

    log_info("Cleaning up remaining connections...");
    uv_walk(app.loop, close_walk_cb, NULL);
    uv_run(app.loop, UV_RUN_DEFAULT);

    uv_loop_close(app.loop);
    log_info("Shutdown complete.");

    return 0;
}
