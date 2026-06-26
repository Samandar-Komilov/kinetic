#include "kinetic/config.h"
#include "kinetic/version.h"

#include <stdio.h>
#include <uv.h>

static void on_idle(uv_idle_t *handle) {
    uv_close((uv_handle_t *)handle, NULL);
}

int main(int argc, char **argv) {
    const char *config_path = "configs/kinetic.conf";
    if (argc > 1) {
        config_path = argv[1];
    }

    kinetic_config cfg;
    if (kinetic_config_load(&cfg, config_path) != 0) {
        return 1;
    }

    uv_loop_t *loop = uv_default_loop();

    uv_idle_t idle;
    uv_idle_init(loop, &idle);
    uv_idle_start(&idle, on_idle);

    printf("Hello from %s %s (libuv %s)\n", cfg.name, kinetic_version_string(),
           uv_version_string());

    printf("Configured to listen on port %u\n", cfg.listen_port);

    int rc = uv_run(loop, UV_RUN_DEFAULT);
    if (rc != 0) {
        fprintf(stderr, "kinetic: event loop exited with error %d\n", rc);
        return 1;
    }

    return 0;
}
