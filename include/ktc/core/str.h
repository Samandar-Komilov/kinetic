#ifndef KTC_CORE_STR_H
#define KTC_CORE_STR_H

/*
 * ktc_str — a non-owning octet slice { ptr, len }.
 *
 * Wire data MUST use this type, never bare char*.  Slices are non-owning:
 * the backing buffer must remain alive for the slice's lifetime.
 *
 * uint8_t (not char) satisfies H11-PARSE-001: parse HTTP as octets, not
 * Unicode.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *ptr;
    size_t len;
} ktc_str;

/** Empty slice { NULL, 0 }. */
ktc_str ktc_str_null(void);

/** Non-owning slice over [ptr, ptr + len) octets. */
ktc_str ktc_str_from(const uint8_t *ptr, size_t len);

/** Slice over a NUL-terminated C string (length excludes the terminator). */
ktc_str ktc_str_from_cstr(const char *s);

/** True when ptr is NULL or len is zero. */
bool ktc_str_is_empty(ktc_str s);

/** Byte-for-byte equality (case-sensitive). */
bool ktc_str_eq(ktc_str a, ktc_str b);

/** True when a equals the C string cstr (case-sensitive). */
bool ktc_str_eq_cstr(ktc_str a, const char *cstr);

/** Case-insensitive equality (ASCII A–Z only; for HTTP header names). */
bool ktc_str_eq_case_insensitive(ktc_str a, ktc_str b);

/** Lexicographic byte order (memcmp semantics; not locale-aware). */
int ktc_str_cmp(ktc_str a, ktc_str b);

#endif /* KTC_CORE_STR_H */
