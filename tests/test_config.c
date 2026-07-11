#include "ktc/core/config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_config_parse_example(void) {
    ktc_config_t config;
    bool ok = ktc_config_parse("configs/example.yaml", &config);
    assert(ok);
    assert(config.listen_port == 80);
    assert(strcmp(config.log_level, "INFO") == 0);
}

static void test_config_parse_test_config(void) {
    ktc_config_t config;
    bool ok = ktc_config_parse("configs/test_config.yaml", &config);
    assert(ok);
    assert(config.listen_port == 8080);
    assert(strcmp(config.log_level, "INFO") == 0);
}

static void test_config_parse_nonexistent(void) {
    ktc_config_t config;
    bool ok = ktc_config_parse("nonexistent_file.yaml", &config);
    assert(!ok);
}

int main(void) {
    test_config_parse_example();
    test_config_parse_test_config();
    test_config_parse_nonexistent();
    printf("test_config: ok\n");
    return 0;
}
