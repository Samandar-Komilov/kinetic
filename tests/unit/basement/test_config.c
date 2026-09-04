#include "ktc/core/config.h"
#include "test_harness.h"

#include <string.h>

static void test_config_parse_example(void) {
    KTC_TEST_CASE("Phase-0.3 (YAML Config)", "Parse configs/example.yaml");
    ktc_config_t config;
    bool ok = ktc_config_parse("configs/example.yaml", &config);
    KTC_ASSERT(ok, "parse succeeds on configs/example.yaml");
    KTC_ASSERT(config.listen_port == 80, "parsed listen_port is 80");
    KTC_ASSERT(strcmp(config.log_level, "INFO") == 0, "parsed log_level is INFO");
}

static void test_config_parse_test_config(void) {
    KTC_TEST_CASE("Phase-0.3 (YAML Config)", "Parse configs/test_config.yaml");
    ktc_config_t config;
    bool ok = ktc_config_parse("configs/test_config.yaml", &config);
    KTC_ASSERT(ok, "parse succeeds on configs/test_config.yaml");
    KTC_ASSERT(config.listen_port == 8080, "parsed listen_port is 8080");
    KTC_ASSERT(strcmp(config.log_level, "INFO") == 0, "parsed log_level is INFO");
}

static void test_config_parse_nonexistent(void) {
    KTC_TEST_CASE("Phase-0.3 (YAML Config)", "Parse nonexistent configuration file");
    ktc_config_t config;
    bool ok = ktc_config_parse("nonexistent_file.yaml", &config);
    KTC_ASSERT(!ok, "parsing nonexistent file returns false gracefully");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 0.3: YAML Config Parser");
    test_config_parse_example();
    test_config_parse_test_config();
    test_config_parse_nonexistent();
    KTC_TEST_SUITE_END();
}
