#include "ktc/version.h"

#include <stdio.h>

int main(void) {
    printf("KinetiC %s\n", ktc_version_string());
    printf("Core ready: ktc_str, ktc_arena\n");
    printf("Next: follow docs/notebooks/notebook0.md\n");
    return 0;
}
