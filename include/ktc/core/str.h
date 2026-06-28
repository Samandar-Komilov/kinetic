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

ktc_str ktc_str_null(void);
ktc_str ktc_str_from(const uint8_t *ptr, size_t len);
ktc_str ktc_str_from_cstr(const char *s);

bool ktc_str_is_empty(ktc_str s);
bool ktc_str_eq(ktc_str a, ktc_str b);
bool ktc_str_eq_cstr(ktc_str a, const char *cstr);
bool ktc_str_eq_ci(ktc_str a, ktc_str b);
int ktc_str_cmp(ktc_str a, ktc_str b);

#endif /* KTC_CORE_STR_H */
