#include "ktc/core/str.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_null(void) {
    ktc_str s = ktc_str_null();
    assert(s.ptr == NULL);
    assert(s.len == 0);
    assert(ktc_str_is_empty(s));
}

static void test_from_cstr(void) {
    ktc_str s = ktc_str_from_cstr("hello");
    assert(s.len == 5);
    assert(!ktc_str_is_empty(s));

    ktc_str empty = ktc_str_from_cstr("");
    assert(empty.len == 0);
    assert(ktc_str_is_empty(empty));

    ktc_str null_s = ktc_str_from_cstr(NULL);
    assert(ktc_str_is_empty(null_s));
}

static void test_from_bytes(void) {
    uint8_t data[] = {0x47, 0x45, 0x54, 0x20}; /* "GET " */
    ktc_str s = ktc_str_from(data, 4);
    assert(s.len == 4);
    assert(ktc_str_eq_cstr(s, "GET "));
}

static void test_eq(void) {
    ktc_str a = ktc_str_from_cstr("foo");
    ktc_str b = ktc_str_from_cstr("foo");
    ktc_str c = ktc_str_from_cstr("bar");

    assert(ktc_str_eq(a, b));
    assert(!ktc_str_eq(a, c));
    assert(ktc_str_eq(ktc_str_null(), ktc_str_null()));
}

static void test_eq_cstr(void) {
    ktc_str s = ktc_str_from_cstr("Host");
    assert(ktc_str_eq_cstr(s, "Host"));
    assert(!ktc_str_eq_cstr(s, "host"));
}

static void test_eq_ci(void) {
    ktc_str host = ktc_str_from_cstr("Host");
    assert(ktc_str_eq_ci(host, ktc_str_from_cstr("host")));
    assert(ktc_str_eq_ci(host, ktc_str_from_cstr("HOST")));
    assert(!ktc_str_eq_ci(host, ktc_str_from_cstr("Hosts")));
}

static void test_cmp(void) {
    ktc_str abc = ktc_str_from_cstr("abc");
    ktc_str abd = ktc_str_from_cstr("abd");
    ktc_str ab = ktc_str_from_cstr("ab");

    assert(ktc_str_cmp(abc, abd) < 0);
    assert(ktc_str_cmp(abd, abc) > 0);
    assert(ktc_str_cmp(abc, abc) == 0);
    assert(ktc_str_cmp(ab, abc) < 0);
}

int main(void) {
    test_null();
    test_from_cstr();
    test_from_bytes();
    test_eq();
    test_eq_cstr();
    test_eq_ci();
    test_cmp();
    printf("test_str: ok\n");
    return 0;
}
