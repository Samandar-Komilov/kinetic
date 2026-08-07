#ifndef KTC_CORE_CONFIG_H
#define KTC_CORE_CONFIG_H

/**
 * @file config.h
 * @brief Application configuration parsing and structures.
 */

#include <stdbool.h>

/**
 * @brief Default path to the static YAML configuration file.
 */
#define DEFAULT_STATIC_CONFIG_DIR "/etc/kinetic/kinetic.yaml"

/**
 * @brief Configuration parameters for the kinetic server.
 */
typedef struct {
    char name[128];     /**< Server instance name. */
    int listen_port;    /**< Port to listen for incoming connections. */
    char log_level[32]; /**< Log level (e.g. TRACE, DEBUG, INFO, WARN, ERROR). */
} ktc_config_t;

/**
 * @brief Parses a simple YAML configuration file into a configuration struct.
 *
 * @param filepath The path to the YAML configuration file.
 * @param config Pointer to the destination configuration struct to populate.
 * @return true on success, false on parsing or validation error.
 */
bool ktc_config_parse(const char *filepath, ktc_config_t *config);

#endif /* KTC_CORE_CONFIG_H */
