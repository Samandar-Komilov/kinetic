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

static void test_eq_case_insensitive(void) {
    ktc_str host = ktc_str_from_cstr("Host");
    assert(ktc_str_eq_case_insensitive(host, ktc_str_from_cstr("host")));
    assert(ktc_str_eq_case_insensitive(host, ktc_str_from_cstr("HOST")));
    assert(!ktc_str_eq_case_insensitive(host, ktc_str_from_cstr("Hosts")));
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

static void test_str_edge_cases(void) {
    ktc_str s_null = ktc_str_null();
    assert(ktc_str_is_empty(s_null));

    ktc_str s_empty = ktc_str_from((const uint8_t *)"", 0);
    assert(ktc_str_is_empty(s_empty));

    // Case-insensitive comparisons on boundaries
    ktc_str s_a = ktc_str_from_cstr("abcdefghijklmnopqrstuvwxyz");
    ktc_str s_b = ktc_str_from_cstr("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    assert(ktc_str_eq_case_insensitive(s_a, s_b));

    // Special characters comparison
    ktc_str s_special1 = ktc_str_from_cstr("host-name_123");
    ktc_str s_special2 = ktc_str_from_cstr("HOST-NAME_123");
    assert(ktc_str_eq_case_insensitive(s_special1, s_special2));

    // Non-ASCII symbols comparison should still match exactly if case is not changed,
    // but check behavior on symbols like '[' and ']' which lie between 'Z' and 'a'.
    ktc_str s_bracket1 = ktc_str_from_cstr("a[b");
    ktc_str s_bracket2 = ktc_str_from_cstr("A[B");
    assert(ktc_str_eq_case_insensitive(s_bracket1, s_bracket2));
}

int main(void) {
    test_null();
    test_from_cstr();
    test_from_bytes();
    test_eq();
    test_eq_cstr();
    test_eq_case_insensitive();
    test_cmp();
    test_str_edge_cases();
    printf("test_str: ok\n");
    return 0;
}
