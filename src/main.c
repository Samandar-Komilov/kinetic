#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <yaml.h>

#include "ktc/core/arena.h"
#include "ktc/core/config.h"
#include "ktc/core/connection.h"
#include "ktc/external/log.h"
#include "ktc/http/body.h"
#include "ktc/http/headers.h"
#include "ktc/http/req_line.h"
#include "ktc/http/response.h"

typedef struct {
    uv_loop_t *loop;
    uv_tcp_t server;
    uv_signal_t sig_int, sig_term, sig_hup;
    bool is_shutting_down;
} ktc_app_t;

static ktc_app_t app;

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

    r = uv_listen((uv_stream_t *)&app.server, 128, ktc_on_connection);
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

    /* Server started successfully */
    uv_run(app.loop, UV_RUN_DEFAULT);

    /* Signal arrived, we're shutting down */
    log_info("Cleaning up remaining connections...");
    uv_walk(app.loop, close_walk_cb, NULL);
    uv_run(app.loop, UV_RUN_DEFAULT);

    uv_loop_close(app.loop);
    log_info("Shutdown complete.");

    return 0;
}
