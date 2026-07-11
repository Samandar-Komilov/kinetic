#ifndef KTC_CORE_CONFIG_H
#define KTC_CORE_CONFIG_H

#include <stdbool.h>

#define DEFAULT_STATIC_CONFIG_DIR "/etc/kinetic/kinetic.yaml"

typedef struct {
    char name[128];
    int listen_port;
    char log_level[32];
} ktc_config_t;

/**
 * Parses a simple YAML configuration file into the ktc_config_t struct.
 * Returns true on success, false on error.
 */
bool ktc_config_parse(const char *filepath, ktc_config_t *config);

#endif /* KTC_CORE_CONFIG_H */
