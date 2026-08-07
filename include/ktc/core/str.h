#ifndef KTC_CORE_STR_H
#define KTC_CORE_STR_H

/**
 * @file str.h
 * @brief Non-owning byte slice (ktc_str) utilities for raw HTTP wire data.
 *
 * Wire data must use this slice representation instead of raw char* to enforce octet-based
 * HTTP parsing rules (§ 6.3) and avoid Unicode/wide-character security exploits.
 * Slices do not own their backing memory; the caller must ensure the source buffer
 * remains allocated for the slice's lifetime.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Non-owning slice of a byte array.
 */
typedef struct {
    const uint8_t *ptr; /**< Pointer to the start of the byte array. */
    size_t len;         /**< Length of the byte array slice in bytes. */
} ktc_str;

/**
 * @brief Returns an empty string slice.
 *
 * @return A ktc_str representing an empty slice { NULL, 0 }.
 */
ktc_str ktc_str_null(void);

/**
 * @brief Creates a string slice representing a non-owning region.
 *
 * @param ptr Pointer to the start of the byte array.
 * @param len The length of the slice in bytes.
 * @return A ktc_str wrapping the specified region.
 */
ktc_str ktc_str_from(const uint8_t *ptr, size_t len);

/**
 * @brief Creates a string slice from a null-terminated C string.
 *
 * The terminating null character is not included in the slice length.
 *
 * @param s The null-terminated C string.
 * @return A ktc_str wrapping the C string.
 */
ktc_str ktc_str_from_cstr(const char *s);

/**
 * @brief Checks if a string slice is empty.
 *
 * A slice is empty if its pointer is NULL or its length is 0.
 *
 * @param s The string slice to check.
 * @return true if the slice is empty, false otherwise.
 */
bool ktc_str_is_empty(ktc_str s);

/**
 * @brief Compares two string slices for exact equality (case-sensitive).
 *
 * @param a The first string slice.
 * @param b The second string slice.
 * @return true if both slices are identical in length and content, false otherwise.
 */
bool ktc_str_eq(ktc_str a, ktc_str b);

/**
 * @brief Compares a string slice and a null-terminated C string for exact equality
 * (case-sensitive).
 *
 * @param a The string slice.
 * @param cstr The C string.
 * @return true if they match exactly, false otherwise.
 */
bool ktc_str_eq_cstr(ktc_str a, const char *cstr);

/**
 * @brief Compares two string slices for case-insensitive equality (ASCII only).
 *
 * Typically used for matching HTTP header names.
 *
 * @param a The first string slice.
 * @param b The second string slice.
 * @return true if they match case-insensitively, false otherwise.
 */
bool ktc_str_eq_case_insensitive(ktc_str a, ktc_str b);

/**
 * @brief Compares two string slices lexicographically.
 *
 * Semi-equivalent to memcmp semantics; not locale-aware.
 *
 * @param a The first string slice.
 * @param b The second string slice.
 * @return Less than 0 if a < b, 0 if a == b, greater than 0 if a > b.
 */
int ktc_str_cmp(ktc_str a, ktc_str b);

#endif /* KTC_CORE_STR_H */
