/* sample_uvserver.c — a minimal libuv TCP/HTTP server skeleton.
 *
 * Goal: touch every libuv API on your list *once*, in the place where a real
 * web server would use it, with enough structure that the lifecycle is correct
 * (every handle is closed before the loop is torn down). It answers any request
 * whose headers end in CRLFCRLF with "Hello, world", then half-closes.
 *
 * Build:  gcc -O2 -Wall -Wextra build/src/sample_uvserver.c -luv -o sample_uvserver
 * Run:    ./build/src/sample_uvserver 8080
 * Test:   curl -v localhost:8080   (or: printf 'GET / HTTP/1.1\r\n\r\n' | nc localhost 8080)
 * Stop:   Ctrl-C  (SIGINT)  — or  kill -TERM <pid>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <uv.h>

/* ---- ktc_str: a borrowed view over bytes (pointer + length, no ownership) ---- */
typedef struct {
    const char *data;
    size_t      len;
} ktc_str;

/* The helper you asked for: view a uv_buf_t's bytes as a ktc_str.
 * NOTE: a uv_buf_t's .len is the *capacity* the allocator handed out, not the
 * number of valid bytes a read produced. So in read_cb we first rebuild the
 * buf with uv_buf_init(base, nread) and pass THAT in — see on_read(). */
static ktc_str ktc_str_from_uv_buf(uv_buf_t buf) {
    ktc_str s = { buf.base, buf.len };
    return s;
}

/* ---- error logging: uv_strerror gives the message, uv_err_name the symbol ---- */
#define LOG_ERR(where, code) \
    fprintf(stderr, "[err] %s: %s (%s)\n", (where), uv_strerror(code), uv_err_name(code))

#define IDLE_TIMEOUT_MS 15000   /* RFC 9112 §8: how long we wait on a stalled msg */
#define LISTEN_BACKLOG  128
#define MAX_REQUEST     (64 * 1024)

/* ---- process-wide handles the signal path needs to reach ---- */
typedef struct {
    uv_loop_t   *loop;
    uv_tcp_t     server;        /* the listener */
    uv_signal_t  sig_int, sig_term, sig_hup;
    int          shutting_down;
} app_t;

static app_t app;

/* ---- per-connection state ---- */
typedef struct {
    uv_tcp_t   handle;          /* the client stream */
    uv_timer_t timer;           /* this connection's idle timeout */
    char      *buf;             /* accumulated request bytes (grows) */
    size_t     len, cap;
    int        closing;         /* guard against double-teardown */
    int        pending_closes;  /* handles still waiting on their close_cb */
} conn_t;

/* a write request carries the heap buffer it must free when the write finishes */
typedef struct {
    uv_write_t req;
    uv_buf_t   buf;
    char      *payload;
    conn_t    *c;
} write_ctx;

static const char RESP_200[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 13\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Hello, world\n";

static const char RESP_400[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";

/* uv_now() reads the loop's cached "now"; uv_update_time() refreshes that cache.
 * The loop normally caches time once per iteration, so we force a refresh here
 * to get an accurate millisecond stamp for the log line. */
static uint64_t now_ms(void) {
    uv_update_time(app.loop);
    return uv_now(app.loop);
}

/* ============================ teardown plumbing ============================ */

static void on_handle_closed(uv_handle_t *h) {
    conn_t *c = h->data;
    if (--c->pending_closes == 0) {     /* both stream and timer are now closed */
        free(c->buf);
        free(c);
    }
}

/* Close a connection cleanly: stop reading, stop the timer, then uv_close BOTH
 * handles. uv_is_closing() guards against starting teardown twice. The conn_t
 * is freed only once both close callbacks have fired (pending_closes hits 0). */
static void conn_close(conn_t *c) {
    if (c->closing) return;
    c->closing = 1;

    uv_read_stop((uv_stream_t *)&c->handle);
    uv_timer_stop(&c->timer);

    c->pending_closes = 2;
    if (!uv_is_closing((uv_handle_t *)&c->timer))
        uv_close((uv_handle_t *)&c->timer, on_handle_closed);
    else c->pending_closes--;

    if (!uv_is_closing((uv_handle_t *)&c->handle))
        uv_close((uv_handle_t *)&c->handle, on_handle_closed);
    else c->pending_closes--;
}

static void on_shutdown(uv_shutdown_t *req, int status) {
    conn_t *c = req->data;
    if (status) LOG_ERR("uv_shutdown", status);
    free(req);
    conn_close(c);              /* write side flushed; now tear the handle down */
}

/* RFC 9112 §9.6 graceful close: half-close the write side so the peer sees EOF
 * after our bytes, instead of an abrupt RST. */
static void conn_shutdown(conn_t *c) {
    if (c->closing) return;
    uv_shutdown_t *req = malloc(sizeof *req);
    req->data = c;
    int r = uv_shutdown(req, (uv_stream_t *)&c->handle, on_shutdown);
    if (r) { LOG_ERR("uv_shutdown", r); free(req); conn_close(c); }
}

/* ============================== write path =============================== */

static void on_write(uv_write_t *req, int status) {
    write_ctx *w = (write_ctx *)req;   /* req is the first member, so cast back */
    conn_t *c = w->c;
    if (status) LOG_ERR("uv_write", status);
    free(w->payload);
    free(w);
    if (status) { conn_close(c); return; }
    conn_shutdown(c);                  /* Connection: close → half-close now */
}

/* Send a canned response. use_write2 picks uv_write2 vs uv_write purely so you
 * can see both: uv_write2's extra arg passes a handle (fd) over a pipe for IPC;
 * with NULL it is exactly uv_write. */
static void conn_respond(conn_t *c, const char *resp, size_t len, int use_write2) {
    write_ctx *w = malloc(sizeof *w);
    w->payload = malloc(len);
    memcpy(w->payload, resp, len);
    w->buf = uv_buf_init(w->payload, (unsigned)len);   /* wrap ptr+len */
    w->c = c;

    int r = use_write2
        ? uv_write2(&w->req, (uv_stream_t *)&c->handle, &w->buf, 1, NULL, on_write)
        : uv_write (&w->req, (uv_stream_t *)&c->handle, &w->buf, 1,       on_write);
    if (r) { LOG_ERR(use_write2 ? "uv_write2" : "uv_write", r);
             free(w->payload); free(w); conn_close(c); }
}

/* ============================== read path =============================== */

/* append grows a per-connection scratch buffer. This realloc-grows freely
 * because nobody outside the connection holds a pointer into it between reads —
 * the exact condition that makes growable single buffers safe. */
static int conn_append(conn_t *c, const char *p, size_t n) {
    if (c->len + n + 1 > c->cap) {
        size_t ncap = c->cap ? c->cap : 1024;
        while (ncap < c->len + n + 1) ncap *= 2;
        char *nb = realloc(c->buf, ncap);
        if (!nb) return -1;
        c->buf = nb; c->cap = ncap;
    }
    memcpy(c->buf + c->len, p, n);
    c->len += n;
    c->buf[c->len] = '\0';
    return 0;
}

/* alloc_cb: hand libuv a buffer to read into. Simplest correct version: malloc
 * the suggested size; on_read frees it. (A pool or per-conn arena would go here
 * in a tuned server.) */
static void on_alloc(uv_handle_t *handle, size_t suggested, uv_buf_t *buf) {
    (void)handle;
    buf->base = malloc(suggested);
    buf->len  = buf->base ? suggested : 0;
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    conn_t *c = stream->data;

    if (nread > 0) {
        /* Rebuild a uv_buf_t whose len is the VALID byte count, then view it. */
        uv_buf_t got  = uv_buf_init(buf->base, (unsigned)nread);
        ktc_str  view = ktc_str_from_uv_buf(got);

        uv_timer_again(&c->timer);     /* fresh bytes → reset the idle countdown */

        if (conn_append(c, view.data, view.len) != 0) {
            conn_respond(c, RESP_400, sizeof RESP_400 - 1, 0);
            free(buf->base);
            return;
        }
        /* Toy "parser": a request is complete once headers end in CRLFCRLF. */
        if (c->len > 4 && strstr(c->buf, "\r\n\r\n")) {
            uv_read_stop(stream);                          /* done reading */
            conn_respond(c, RESP_200, sizeof RESP_200 - 1, /*use_write2=*/1);
        } else if (c->len > MAX_REQUEST) {
            uv_read_stop(stream);
            conn_respond(c, RESP_400, sizeof RESP_400 - 1, /*use_write2=*/0);
        }
    } else if (nread == UV_EOF) {
        fprintf(stderr, "[conn] peer closed (EOF)\n");
        conn_close(c);
    } else if (nread < 0) {
        LOG_ERR("uv_read", (int)nread);
        conn_close(c);
    }

    free(buf->base);   /* we malloc per alloc; release after copying */
}

/* ======================= idle timeout (per connection) ======================= */

static void on_idle_timeout(uv_timer_t *timer) {
    conn_t *c = timer->data;
    fprintf(stderr, "[conn] idle timeout after %dms (RFC 9112 §8) — closing\n",
            IDLE_TIMEOUT_MS);
    conn_close(c);
}

/* ============================ accept new clients ============================ */

static void on_connection(uv_stream_t *server, int status) {
    if (status < 0) { LOG_ERR("on_connection", status); return; }
    if (app.shutting_down) return;     /* refuse new work while draining */

    conn_t *c = calloc(1, sizeof *c);
    uv_tcp_init(app.loop, &c->handle);
    c->handle.data = c;
    uv_timer_init(app.loop, &c->timer);
    c->timer.data = c;

    if (uv_accept(server, (uv_stream_t *)&c->handle) == 0) {
        /* uv_fileno: pull the OS socket fd out of the handle (logging / fd
         * passing / SO_REUSEPORT bookkeeping live here in a real server). */
        uv_os_fd_t fd = -1;
        uv_fileno((uv_handle_t *)&c->handle, &fd);
        fprintf(stderr, "[conn] accepted fd=%d at t=%llu ms\n",
                (int)fd, (unsigned long long)now_ms());

        /* Start the idle timer with repeat == timeout so uv_timer_again() can
         * re-arm it with the same interval on every read. */
        uv_timer_start(&c->timer, on_idle_timeout, IDLE_TIMEOUT_MS, IDLE_TIMEOUT_MS);

        uv_read_start((uv_stream_t *)&c->handle, on_alloc, on_read);
    } else {
        conn_close(c);
    }
}

/* ============================== shutdown ============================== */

/* Final sweep: close anything still open (e.g. client conns that were mid-flight
 * when the signal arrived). uv_is_closing() skips handles already tearing down.
 * uv_handle_type_name() is the "type check" used here just to label the log. */
static void close_walk_cb(uv_handle_t *handle, void *arg) {
    (void)arg;
    if (!uv_is_closing(handle)) {
        fprintf(stderr, "[shutdown] force-closing a %s handle\n",
                uv_handle_type_name(uv_handle_get_type(handle)));
        uv_close(handle, NULL);
    }
}

/* On SIGINT/SIGTERM/SIGHUP: stop accepting, drop the signal watchers, and break
 * the loop. uv_stop() makes the *current* uv_run return after this iteration. */
static void on_signal(uv_signal_t *handle, int signum) {
    (void)handle;
    if (app.shutting_down) return;
    app.shutting_down = 1;
    fprintf(stderr, "\n[signal] caught %d — stopping accept, draining\n", signum);

    if (!uv_is_closing((uv_handle_t *)&app.server))
        uv_close((uv_handle_t *)&app.server, NULL);     /* stop accept */

    uv_signal_stop(&app.sig_int);  uv_close((uv_handle_t *)&app.sig_int,  NULL);
    uv_signal_stop(&app.sig_term); uv_close((uv_handle_t *)&app.sig_term, NULL);
    uv_signal_stop(&app.sig_hup);  uv_close((uv_handle_t *)&app.sig_hup,  NULL);

    uv_stop(app.loop);             /* break out of uv_run() */
}

/* ================================= main ================================= */

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 8080;

    /* One loop per worker process. uv_default_loop() returns a uv_loop_t*. */
    app.loop = uv_default_loop();

    /* Listener: init → bind → listen. */
    uv_tcp_init(app.loop, &app.server);
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    int r = uv_tcp_bind(&app.server, (const struct sockaddr *)&addr, 0);
    if (r) { LOG_ERR("uv_tcp_bind", r); return 1; }

    r = uv_listen((uv_stream_t *)&app.server, LISTEN_BACKLOG, on_connection);
    if (r) { LOG_ERR("uv_listen", r); return 1; }

    /* Daemon lifecycle signals. */
    uv_signal_init(app.loop, &app.sig_int);
    uv_signal_init(app.loop, &app.sig_term);
    uv_signal_init(app.loop, &app.sig_hup);
    uv_signal_start(&app.sig_int,  on_signal, SIGINT);
    uv_signal_start(&app.sig_term, on_signal, SIGTERM);
    uv_signal_start(&app.sig_hup,  on_signal, SIGHUP);

    fprintf(stderr, "[boot] listening on 0.0.0.0:%d (pid %d)\n", port, (int)getpid());

    /* Drive the loop until uv_stop() (signal) or every handle has closed. */
    uv_run(app.loop, UV_RUN_DEFAULT);

    /* uv_stop leaves handles open; close them, then run once more so their
     * close callbacks fire, before uv_loop_close (which fails with EBUSY if
     * any handle is still alive). */
    uv_walk(app.loop, close_walk_cb, NULL);
    uv_run(app.loop, UV_RUN_DEFAULT);

    r = uv_loop_close(app.loop);
    if (r) LOG_ERR("uv_loop_close", r);
    fprintf(stderr, "[exit] clean shutdown\n");
    return 0;
}

/*
 * =============================================================================
 * Reference — what each piece does and why
 * =============================================================================
 *
 * This file is a teaching skeleton for notebook 0 Era 1. It is not production
 * HTTP yet — the "parser" is strstr for CRLFCRLF — but the libuv lifecycle is
 * real: bind, accept, read, write, half-close, signal drain, loop teardown.
 *
 * ---- libuv handle hierarchy (why all the casts?) ----
 *
 *   uv_handle_t   base: uv_close(), uv_is_closing(), .data user pointer
 *       └── uv_stream_t   listen/accept/read/write/shutdown
 *               └── uv_tcp_t   TCP sockets (listener + client)
 *
 * Functions like uv_listen() take uv_stream_t*; uv_close() takes uv_handle_t*.
 * Casting &app.server or &c->handle is an upcast — same memory, wider API.
 * No copy happens.
 *
 * ---- types ----
 *
 * ktc_str
 *   Borrowed {data, len} view over bytes. In KinetiC proper this lives in
 *   include/ktc/core/str.h as {ptr, len} with uint8_t. Here it previews how
 *   wire data should be viewed: length-delimited, not NUL-terminated.
 *
 * app_t / static app
 *   Process-wide state the signal handler must reach: the event loop, the
 *   listener handle, signal watchers, and a shutting_down flag to refuse new
 *   accepts while draining.
 *
 * conn_t
 *   Per-client state: TCP stream, idle timer, growable read buffer, and
 *   teardown guards (closing, pending_closes). One conn_t per accepted socket.
 *
 * write_ctx
 *   Outbound write bundle. uv_write_t must stay alive until on_write fires;
 *   we heap-allocate write_ctx and free it in the callback. payload is copied
 *   because the response string may be stack/static — libuv needs stable bytes
 *   until the write completes.
 *
 * ---- helpers & constants ----
 *
 * LOG_ERR(where, code)
 *   Prints libuv error name + message. uv_strerror() is human text;
 *   uv_err_name() is the symbol (e.g. ECONNRESET).
 *
 * IDLE_TIMEOUT_MS / LISTEN_BACKLOG / MAX_REQUEST
 *   Idle timeout implements RFC 9112 §8 incomplete-message policy (roughly).
 *   Backlog is the kernel accept queue depth. MAX_REQUEST caps buffer growth.
 *
 * RESP_200 / RESP_400
 *   Canned HTTP/1.1 responses with Connection: close — persistence is later.
 *
 * ktc_str_from_uv_buf(buf)
 *   Wraps a uv_buf_t as a byte slice. Callers must pass a buf whose .len is
 *   the valid byte count (nread), not the allocation size — see on_read().
 *
 * now_ms()
 *   Returns loop time in ms. uv_update_time() refreshes the cached clock;
 *   without it, uv_now() might be stale mid-iteration.
 *
 * ---- teardown ----
 *
 * on_handle_closed(h)
 *   close callback shared by the client stream and its timer. h->data points
 *   back to conn_t (set in on_connection). pending_closes starts at 2; when
 *   both handles finish closing, we free the read buffer and conn_t.
 *
 * conn_close(c)
 *   Idempotent connection teardown. Stops read + timer, then uv_close() on
 *   both handles. uv_is_closing() avoids double-close if shutdown already
 *   started. Does not free conn_t immediately — waits for on_handle_closed.
 *
 * conn_shutdown(c) / on_shutdown(req, status)
 *   RFC 9112 §9.6 graceful close: after the response is fully written, call
 *   uv_shutdown() to half-close our write side. Peer reads remaining bytes,
 *   then sees EOF — cleaner than RST. on_shutdown frees the request and calls
 *   conn_close() to tear down both handles.
 *
 * ---- write path ----
 *
 * conn_respond(c, resp, len, use_write2)
 *   Copies resp to heap, wraps it in uv_buf_t, queues uv_write or uv_write2.
 *   use_write2 is demo-only: uv_write2(..., NULL) equals uv_write. On queue
 *   failure, frees memory and conn_close() immediately.
 *
 * on_write(req, status)
 *   Write finished. Recovers write_ctx via cast (uv_write_t is first member).
 *   Frees payload + write_ctx. On success → conn_shutdown(); on error →
 *   conn_close().
 *
 * ---- read path ----
 *
 * conn_append(c, p, n)
 *   Appends incoming octets to the per-connection buffer (realloc x2 growth).
 *   Safe here because nothing else holds pointers into buf between reads.
 *   Adds a trailing NUL for strstr debugging — not for wire parsing in prod.
 *
 * on_alloc(handle, suggested, buf)
 *   libuv read callback: provide a buffer for the next read. We malloc(suggested)
 *   per read; on_read frees it after copying into conn_t->buf. A real server
 *   might use a pool or per-conn slab from ktc_arena.
 *
 * on_read(stream, nread, buf)
 *   Core read handler. stream->data → conn_t (set at accept time).
 *
 *   nread > 0  — got bytes: rebuild buf with valid len, append, reset idle timer.
 *                Toy completion: headers end with "\r\n\r\n" → respond 200.
 *                Over MAX_REQUEST → 400. Stops read before responding.
 *   nread == UV_EOF — peer closed; conn_close().
 *   nread < 0  — libuv error; log and conn_close().
 *
 *   Always free(buf->base) — paired with on_alloc's malloc.
 *
 * on_idle_timeout(timer)
 *   Fires when no bytes arrive for IDLE_TIMEOUT_MS. timer->data → conn_t.
 *   Closes stalled connections (RFC 9112 §8 incomplete messages).
 *
 * ---- accept ----
 *
 * on_connection(server, status)
 *   uv_listen callback — a client is waiting. Allocates conn_t, inits TCP +
 *   timer handles, sets handle.data and timer.data to c so every callback can
 *   find the connection without global lookup.
 *
 *   uv_accept(server, &c->handle) transfers the accepted socket into c->handle.
 *   uv_fileno() logs the OS fd (useful for debugging, fd passing, reuseport).
 *   Starts idle timer (repeat interval = timeout so uv_timer_again works).
 *   uv_read_start() begins async reads with on_alloc/on_read.
 *
 *   If accept fails or we're shutting_down, closes the half-built conn_t.
 *
 * ---- process shutdown ----
 *
 * close_walk_cb(handle, arg)
 *   uv_walk visitor: force uv_close() on any handle still alive after uv_stop.
 *   Skips handles already closing. arg unused — (void)arg silences -Wunused.
 *
 * on_signal(handle, signum)
 *   SIGINT/SIGTERM/SIGHUP handler. Sets shutting_down, closes listener (no
 *   new clients), stops and closes signal handles, calls uv_stop() to make the
 *   running uv_run() return. Does NOT close client connections here — walk+run
 *   in main handles stragglers. handle unused — (void)handle silences warning.
 *
 * ---- main ----
 *
 * main(argc, argv)
 *   Boot sequence:
 *     1. uv_default_loop() — one loop per worker process.
 *     2. uv_tcp_init → uv_ip4_addr → uv_tcp_bind → uv_listen
 *        Bind 0.0.0.0 = all IPv4 interfaces (localhost + LAN). Use 127.0.0.1
 *        during dev to block remote connections.
 *     3. Register signal handlers for graceful stop.
 *     4. uv_run() — event loop until uv_stop or all handles close.
 *     5. uv_walk + uv_run again — uv_stop leaves handles open; close them and
 *        pump the loop so close callbacks run. Required before uv_loop_close().
 *     6. uv_loop_close() — fails with EBUSY if any handle remains.
 *
 * ---- Era 1 checklist mapping (notebook0.md) ----
 *
 *   [x] Listen on a port (libuv uv_tcp_t)
 *   [x] Accept connections; one handle per client
 *   [x] Read bytes into a buffer (no real HTTP grammar yet)
 *   [x] Close on EOF / error / timeout
 *   [ ] Your code: ktc_str in include/, ktc_arena for conn state, log.h, YAML port
 *
 * =============================================================================
 */
