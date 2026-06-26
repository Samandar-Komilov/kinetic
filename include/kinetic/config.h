#ifndef KINETIC_CONFIG_H
#define KINETIC_CONFIG_H

#include <stddef.h>

typedef struct kinetic_config {
    char name[64];
    unsigned short listen_port;
} kinetic_config;

/* Load settings from a libconfig .conf file. Returns 0 on success. */
int kinetic_config_load(kinetic_config *out, const char *path);

#endif /* KINETIC_CONFIG_H */
