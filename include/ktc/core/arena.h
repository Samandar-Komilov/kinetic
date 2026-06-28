#ifndef KTC_CORE_ARENA_H
#define KTC_CORE_ARENA_H

/*
 * ktc_arena — bump allocator with whole-region free.
 *
 * One arena per connection or per request.  ktc_arena_reset() rewinds for
 * keep-alive reuse between requests on the same connection.
 */

#include <stddef.h>

typedef struct ktc_arena ktc_arena;

ktc_arena *ktc_arena_create(size_t initial_block);
void ktc_arena_destroy(ktc_arena *a);
void *ktc_arena_alloc(ktc_arena *a, size_t size, size_t align);
void *ktc_arena_calloc(ktc_arena *a, size_t n, size_t size);
void ktc_arena_reset(ktc_arena *a);

#endif /* KTC_CORE_ARENA_H */
