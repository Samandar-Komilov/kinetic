#include "ktc/core/arena.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KTC_ARENA_DEFAULT_ALIGN 8
#define KTC_ARENA_MIN_BLOCK 64

typedef struct ktc_arena_block {
    struct ktc_arena_block *next;
    size_t cap;
    size_t used;
} ktc_arena_block;

struct ktc_arena_t {
    ktc_arena_block *head;
    ktc_arena_block *initial;
};

static ktc_arena_block *block_new(size_t cap) {
    if (cap < KTC_ARENA_MIN_BLOCK) {
        cap = KTC_ARENA_MIN_BLOCK;
    }
    ktc_arena_block *b = malloc(sizeof(ktc_arena_block) + cap);
    if (!b) {
        return NULL;
    }
    b->next = NULL;
    b->cap = cap;
    b->used = 0;
    return b;
}

static uint8_t *block_data(ktc_arena_block *b) {
    return (uint8_t *)(b + 1);
}

static uintptr_t align_up(uintptr_t val, uintptr_t align) {
    return (val + align - 1) & ~(align - 1);
}

ktc_arena_t *ktc_arena_create(size_t initial_block) {
    ktc_arena_t *a = malloc(sizeof(*a));
    if (!a) {
        return NULL;
    }
    ktc_arena_block *b = block_new(initial_block);
    if (!b) {
        free(a);
        return NULL;
    }
    a->head = b;
    a->initial = b;
    return a;
}

void ktc_arena_destroy(ktc_arena_t *a) {
    if (!a) {
        return;
    }
    ktc_arena_block *b = a->initial;
    while (b) {
        ktc_arena_block *next = b->next;
        free(b);
        b = next;
    }
    free(a);
}

void *ktc_arena_alloc(ktc_arena_t *a, size_t size, size_t align) {
    if (!a || size == 0) {
        return NULL;
    }
    if (align == 0) {
        align = KTC_ARENA_DEFAULT_ALIGN;
    }

    uintptr_t base = (uintptr_t)block_data(a->head);
    uintptr_t cur = base + (uintptr_t)a->head->used;
    uintptr_t aligned = align_up(cur, (uintptr_t)align);
    size_t new_used = (size_t)(aligned - base) + size;

    if (new_used <= a->head->cap) {
        a->head->used = new_used;
        return (void *)aligned;
    }

    size_t new_cap = a->head->cap * 2;
    if (new_cap < size + align) {
        new_cap = size + align;
    }
    ktc_arena_block *nb = block_new(new_cap);
    if (!nb) {
        return NULL;
    }

    a->head->next = nb;
    a->head = nb;

    base = (uintptr_t)block_data(nb);
    aligned = align_up(base, (uintptr_t)align);
    nb->used = (size_t)(aligned - base) + size;
    return (void *)aligned;
}

void *ktc_arena_calloc(ktc_arena_t *a, size_t n, size_t size) {
    if (n == 0 || size == 0) {
        return NULL;
    }
    if (n > SIZE_MAX / size) {
        return NULL;
    }
    void *ptr = ktc_arena_alloc(a, n * size, 0);
    if (ptr) {
        memset(ptr, 0, n * size);
    }
    return ptr;
}

void ktc_arena_reset(ktc_arena_t *a) {
    if (!a) {
        return;
    }
    ktc_arena_block *b = a->initial->next;
    while (b) {
        ktc_arena_block *next = b->next;
        free(b);
        b = next;
    }
    a->initial->next = NULL;
    a->initial->used = 0;
    a->head = a->initial;
}
