#include "kinetic/config.h"

#include <libconfig.h>
#include <stdio.h>
#include <string.h>

int kinetic_config_load(kinetic_config *out, const char *path) {
    config_t cfg;
    config_init(&cfg);

    if (config_read_file(&cfg, path) != CONFIG_TRUE) {
        fprintf(stderr, "kinetic: failed to read config '%s': %s (line %d)\n", path,
                config_error_text(&cfg), config_error_line(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    const char *name = "kinetic";
    if (config_lookup_string(&cfg, "kinetic.name", &name) != CONFIG_TRUE) {
        fprintf(stderr, "kinetic: missing required setting 'kinetic.name'\n");
        config_destroy(&cfg);
        return -1;
    }

    int port = 8080;
    if (config_lookup_int(&cfg, "kinetic.listen_port", &port) != CONFIG_TRUE) {
        fprintf(stderr, "kinetic: missing required setting 'kinetic.listen_port'\n");
        config_destroy(&cfg);
        return -1;
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "kinetic: invalid listen_port %d\n", port);
        config_destroy(&cfg);
        return -1;
    }

    snprintf(out->name, sizeof(out->name), "%s", name);
    out->listen_port = (unsigned short)port;

    config_destroy(&cfg);
    return 0;
}
