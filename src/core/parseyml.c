#include "ktc/core/config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

bool ktc_config_parse(const char *filepath, ktc_config_t *config) {
    if (!filepath || !config) {
        return false;
    }

    FILE *fh = fopen(filepath, "r");
    if (!fh) {
        fprintf(stderr, "ktc_config_parse fopen('%s') failed: ", filepath);
        perror("");
        return false;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fclose(fh);
        return false;
    }
    yaml_parser_set_input_file(&parser, fh);

    // Initialize with default values
    memset(config, 0, sizeof(*config));
    strncpy(config->name, "kinetic", sizeof(config->name) - 1);
    config->listen_port = 8080;
    strncpy(config->log_level, "INFO", sizeof(config->log_level) - 1);

    char keys[8][64];
    bool has_key[8];
    memset(keys, 0, sizeof(keys));
    memset(has_key, 0, sizeof(has_key));

    int depth = 0;
    bool success = true;
    bool done = false;

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "libyaml parse error: %s line %ld col %ld\n", parser.problem,
                    (long)parser.problem_mark.line, (long)parser.problem_mark.column);
            success = false;
            break;
        }

        switch (event.type) {
        case YAML_STREAM_END_EVENT:
            done = true;
            break;

        case YAML_MAPPING_START_EVENT:
            if (depth >= 0 && depth < 8) {
                has_key[depth] = false;
            }
            depth++;
            break;

        case YAML_MAPPING_END_EVENT:
            if (depth > 0 && depth <= 8) {
                keys[depth - 1][0] = '\0';
                has_key[depth - 1] = false;
            }
            depth--;
            if (depth < 0) {
                depth = 0;
            }
            if (depth > 0 && depth <= 8) {
                keys[depth - 1][0] = '\0';
                has_key[depth - 1] = false;
            }
            break;

        case YAML_SCALAR_EVENT: {
            const char *value = (const char *)event.data.scalar.value;

            if (depth > 0 && depth <= 8) {
                int idx = depth - 1;
                if (!has_key[idx]) {
                    strncpy(keys[idx], value, sizeof(keys[idx]) - 1);
                    keys[idx][sizeof(keys[idx]) - 1] = '\0';
                    has_key[idx] = true;
                } else {
                    // This is the value
                    if (depth == 2 && strcmp(keys[0], "log") == 0 &&
                        strcmp(keys[1], "level") == 0) {
                        strncpy(config->log_level, value, sizeof(config->log_level) - 1);
                        config->log_level[sizeof(config->log_level) - 1] = '\0';
                    } else if (depth == 3 && strcmp(keys[0], "entryPoints") == 0 &&
                               strcmp(keys[1], "web") == 0 && strcmp(keys[2], "address") == 0) {
                        if (value[0] == ':') {
                            config->listen_port = (int)strtol(value + 1, NULL, 10);
                        } else {
                            config->listen_port = (int)strtol(value, NULL, 10);
                        }
                    }
                    keys[idx][0] = '\0';
                    has_key[idx] = false;
                }
            }
            break;
        }

        default:
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return success;
}
