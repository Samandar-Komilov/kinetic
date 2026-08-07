#ifndef KTC_CORE_ARENA_H
#define KTC_CORE_ARENA_H

/**
 * @file arena.h
 * @brief Memory arena bump allocator interface.
 *
 * One arena is typically allocated per connection or per request. ktc_arena_reset()
 * rewinds the allocator to reuse memory chunks between keep-alive requests on the same connection.
 */

#include <stddef.h>

/**
 * @brief Opaque structure representing a bump allocator arena.
 */
typedef struct ktc_arena_t ktc_arena_t;

/**
 * @brief Creates a new memory arena.
 *
 * Allocates the initial block of memory for the arena.
 *
 * @param initial_block The capacity in bytes of the initial allocation region.
 * @return A pointer to the created arena, or NULL on allocation failure.
 */
ktc_arena_t *ktc_arena_create(size_t initial_block);

/**
 * @brief Destroys the arena, freeing all memory blocks and the arena object itself.
 *
 * All pointers allocated from this arena become invalid.
 *
 * @param a Pointer to the arena to destroy. If NULL, no action is taken.
 */
void ktc_arena_destroy(ktc_arena_t *a);

/**
 * @brief Allocates aligned memory from the arena.
 *
 * @param a Pointer to the arena.
 * @param size The number of bytes to allocate.
 * @param align The required byte alignment (e.g., 8, 16).
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void *ktc_arena_alloc(ktc_arena_t *a, size_t size, size_t align);

/**
 * @brief Allocates zero-initialized memory for an array from the arena.
 *
 * Allocates n * size bytes, zeroing the memory.
 *
 * @param a Pointer to the arena.
 * @param n Number of elements.
 * @param size Size of each element.
 * @return A pointer to the allocated, zeroed memory, or NULL if allocation fails.
 */
void *ktc_arena_calloc(ktc_arena_t *a, size_t n, size_t size);

/**
 * @brief Resets the arena, rewinding allocation to the start of the first block.
 *
 * Any overflow blocks allocated during heavy usage are freed. This is useful for reusing
 * the same arena for consecutive HTTP keep-alive requests without memory leaks.
 *
 * @param a Pointer to the arena to reset.
 */
void ktc_arena_reset(ktc_arena_t *a);

#endif /* KTC_CORE_ARENA_H */
