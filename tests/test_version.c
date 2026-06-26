#include "kinetic/version.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *version = kinetic_version_string();

    assert(version != NULL);
    assert(strlen(version) > 0);
    assert(strcmp(version, "0.1.0") == 0);

    printf("test_version: ok\n");
    return 0;
}
