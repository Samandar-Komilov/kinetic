#include "ktc/core/str.h"

#include <string.h>

ktc_str ktc_str_null(void) {
    return (ktc_str){.ptr = NULL, .len = 0};
}

ktc_str ktc_str_from(const uint8_t *ptr, size_t len) {
    return (ktc_str){.ptr = ptr, .len = len};
}

ktc_str ktc_str_from_cstr(const char *s) {
    if (!s) {
        return ktc_str_null();
    }
    return (ktc_str){.ptr = (const uint8_t *)s, .len = strlen(s)};
}

bool ktc_str_is_empty(ktc_str s) {
    return s.ptr == NULL || s.len == 0;
}

bool ktc_str_eq(ktc_str a, ktc_str b) {
    if (a.len != b.len) {
        return false;
    }
    if (a.len == 0) {
        return true;
    }
    if (a.ptr == b.ptr) {
        return true;
    }
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

bool ktc_str_eq_cstr(ktc_str a, const char *cstr) {
    return ktc_str_eq(a, ktc_str_from_cstr(cstr));
}

bool ktc_str_eq_case_insensitive(ktc_str a, ktc_str b) {
    if (a.len != b.len) {
        return false;
    }
    for (size_t i = 0; i < a.len; i++) {
        uint8_t ca = a.ptr[i];
        uint8_t cb = b.ptr[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = (uint8_t)(ca + ('a' - 'A'));
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (uint8_t)(cb + ('a' - 'A'));
        }
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

int ktc_str_cmp(ktc_str a, ktc_str b) {
    if (a.len == 0 && b.len == 0) {
        return 0;
    }
    if (a.len == 0) {
        return -1;
    }
    if (b.len == 0) {
        return 1;
    }
    size_t min_len = a.len < b.len ? a.len : b.len;
    int r = memcmp(a.ptr, b.ptr, min_len);
    if (r != 0) {
        return r;
    }
    if (a.len < b.len) {
        return -1;
    }
    if (a.len > b.len) {
        return 1;
    }
    return 0;
}
