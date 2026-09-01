#include "ktc/core/arena.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_create_destroy(void) {
    ktc_arena_t *a = ktc_arena_create(256);
    assert(a != NULL);
    ktc_arena_destroy(a);
    ktc_arena_destroy(NULL);
}

static void test_alloc_basic(void) {
    ktc_arena_t *a = ktc_arena_create(256);
    void *p1 = ktc_arena_alloc(a, 16, 8);
    void *p2 = ktc_arena_alloc(a, 16, 8);
    assert(p1 != NULL && p2 != NULL);
    assert((uint8_t *)p2 >= (uint8_t *)p1 + 16);
    ktc_arena_destroy(a);
}

static void test_reset(void) {
    ktc_arena_t *a = ktc_arena_create(64);
    void *p1 = ktc_arena_alloc(a, 64, 1);
    void *p2 = ktc_arena_alloc(a, 16, 1);
    assert(p1 != NULL && p2 != NULL);
    ktc_arena_reset(a);
    void *p3 = ktc_arena_alloc(a, 16, 1);
    assert(p3 == p1);
    ktc_arena_destroy(a);
}

static void test_arena_edge_cases(void) {
    // 1. Allocate size 0
    ktc_arena_t *a = ktc_arena_create(64);
    assert(a != NULL);
    void *p1 = ktc_arena_alloc(a, 0, 8);
    assert(p1 == NULL); // Returns NULL when size is 0

    // 2. Alignment test (verify alignments of 1, 2, 4, 8, 16, 32, 64)
    size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
    for (size_t i = 0; i < sizeof(alignments) / sizeof(alignments[0]); i++) {
        size_t align = alignments[i];
        void *p = ktc_arena_alloc(a, 1, align);
        assert(p != NULL);
        assert(((uintptr_t)p % align) == 0);
    }

    // 3. Overflow test (allocating more than block size to trigger new block chain)
    void *p_large = ktc_arena_alloc(a, 500, 8);
    assert(p_large != NULL);
    assert(((uintptr_t)p_large % 8) == 0);

    ktc_arena_reset(a);
    // 4. Reset reuse verification
    void *p_reused = ktc_arena_alloc(a, 10, 8);
    assert(p_reused != NULL);

    ktc_arena_destroy(a);
}

int main(void) {
    test_create_destroy();
    test_alloc_basic();
    test_reset();
    test_arena_edge_cases();
    printf("test_arena: ok\n");
    return 0;
}
