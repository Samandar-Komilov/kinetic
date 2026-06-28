#include "ktc/core/arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_create_destroy(void) {
    ktc_arena *a = ktc_arena_create(256);
    assert(a != NULL);
    ktc_arena_destroy(a);
    ktc_arena_destroy(NULL);
}

static void test_alloc_basic(void) {
    ktc_arena *a = ktc_arena_create(256);
    void *p1 = ktc_arena_alloc(a, 16, 8);
    void *p2 = ktc_arena_alloc(a, 16, 8);
    assert(p1 != NULL && p2 != NULL);
    assert((uint8_t *)p2 >= (uint8_t *)p1 + 16);
    ktc_arena_destroy(a);
}

static void test_reset(void) {
    ktc_arena *a = ktc_arena_create(64);
    void *p1 = ktc_arena_alloc(a, 64, 1);
    void *p2 = ktc_arena_alloc(a, 16, 1);
    assert(p1 != NULL && p2 != NULL);
    ktc_arena_reset(a);
    void *p3 = ktc_arena_alloc(a, 16, 1);
    assert(p3 == p1);
    ktc_arena_destroy(a);
}

int main(void) {
    test_create_destroy();
    test_alloc_basic();
    test_reset();
    printf("test_arena: ok\n");
    return 0;
}
