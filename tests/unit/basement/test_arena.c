#include "ktc/core/arena.h"
#include "test_harness.h"

#include <stdint.h>

static void test_create_destroy(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_arena_create and destroy");
    ktc_arena_t *a = ktc_arena_create(256);
    KTC_ASSERT(a != NULL, "arena created successfully");
    ktc_arena_destroy(a);
    ktc_arena_destroy(NULL);
    KTC_ASSERT(true, "safe destruction of NULL arena");
}

static void test_alloc_basic(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_arena_alloc basic");
    ktc_arena_t *a = ktc_arena_create(256);
    void *p1 = ktc_arena_alloc(a, 16, 8);
    void *p2 = ktc_arena_alloc(a, 16, 8);
    KTC_ASSERT(p1 != NULL && p2 != NULL, "both allocations succeed");
    KTC_ASSERT((uint8_t *)p2 >= (uint8_t *)p1 + 16, "second allocation placed after first");
    ktc_arena_destroy(a);
}

static void test_reset(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_arena_reset O(1) reuse");
    ktc_arena_t *a = ktc_arena_create(64);
    void *p1 = ktc_arena_alloc(a, 64, 1);
    void *p2 = ktc_arena_alloc(a, 16, 1);
    KTC_ASSERT(p1 != NULL && p2 != NULL, "initial allocations succeed across blocks");

    ktc_arena_reset(a);
    void *p3 = ktc_arena_alloc(a, 16, 1);
    KTC_ASSERT(p3 == p1, "allocation after reset reuses first block pointer");
    ktc_arena_destroy(a);
}

static void test_arena_edge_cases(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_arena edge cases");
    ktc_arena_t *a = ktc_arena_create(64);
    KTC_ASSERT(a != NULL, "arena created");

    void *p1 = ktc_arena_alloc(a, 0, 8);
    KTC_ASSERT(p1 == NULL, "size 0 allocation returns NULL");

    size_t alignments[] = {1, 2, 4, 8, 16, 32, 64};
    bool align_ok = true;
    for (size_t i = 0; i < sizeof(alignments) / sizeof(alignments[0]); i++) {
        size_t align = alignments[i];
        void *p = ktc_arena_alloc(a, 1, align);
        if (!p || ((uintptr_t)p % align) != 0) {
            align_ok = false;
            break;
        }
    }
    KTC_ASSERT(align_ok, "allocations honor alignment constraints (1, 2, 4, 8, 16, 32, 64)");

    void *p_large = ktc_arena_alloc(a, 500, 8);
    KTC_ASSERT(p_large != NULL && ((uintptr_t)p_large % 8) == 0,
               "large allocation exceeding initial block triggers growth");

    ktc_arena_reset(a);
    void *p_reused = ktc_arena_alloc(a, 10, 8);
    KTC_ASSERT(p_reused != NULL, "subsequent allocation after reset succeeds");

    ktc_arena_destroy(a);
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 0.4: ktc_arena Bump Allocator");
    test_create_destroy();
    test_alloc_basic();
    test_reset();
    test_arena_edge_cases();
    KTC_TEST_SUITE_END();
}
