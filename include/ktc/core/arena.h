#ifndef KTC_CORE_ARENA_H
#define KTC_CORE_ARENA_H

/*
 * ktc_arena — bump allocator with whole-region free.
 *
 * One arena per connection or per request.  ktc_arena_reset() rewinds for
 * keep-alive reuse between requests on the same connection.
 */

#include <stddef.h>

typedef struct ktc_arena_t ktc_arena_t;

/** Create an arena; initial_block is the first bump region capacity in bytes. */
ktc_arena_t *ktc_arena_create(size_t initial_block);

/** Free all blocks and the arena object. */
void ktc_arena_destroy(ktc_arena_t *a);

/** Bump-allocate size bytes with align-byte alignment; returns NULL on OOM. */
void *ktc_arena_alloc(ktc_arena_t *a, size_t size, size_t align);

/** Zero-initialized bump allocation of n * size bytes. */
void *ktc_arena_calloc(ktc_arena_t *a, size_t n, size_t size);

/** Rewind to the first block; frees overflow blocks (keep-alive reuse). */
void ktc_arena_reset(ktc_arena_t *a);

#endif /* KTC_CORE_ARENA_H */
