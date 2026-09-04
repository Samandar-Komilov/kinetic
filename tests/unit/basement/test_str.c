#include "ktc/core/str.h"
#include "test_harness.h"

static void test_null(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_null");
    ktc_str s = ktc_str_null();
    KTC_ASSERT(s.ptr == NULL, "ptr is NULL");
    KTC_ASSERT(s.len == 0, "len is 0");
    KTC_ASSERT(ktc_str_is_empty(s), "is_empty returns true");
}

static void test_from_cstr(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_from_cstr");
    ktc_str s = ktc_str_from_cstr("hello");
    KTC_ASSERT(s.len == 5, "string length is 5");
    KTC_ASSERT(!ktc_str_is_empty(s), "is_empty returns false for non-empty string");

    ktc_str empty = ktc_str_from_cstr("");
    KTC_ASSERT(empty.len == 0, "empty string length is 0");
    KTC_ASSERT(ktc_str_is_empty(empty), "is_empty returns true for empty string");

    ktc_str null_s = ktc_str_from_cstr(NULL);
    KTC_ASSERT(ktc_str_is_empty(null_s), "is_empty returns true for NULL pointer");
}

static void test_from_bytes(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_from");
    uint8_t data[] = {0x47, 0x45, 0x54, 0x20}; /* "GET " */
    ktc_str s = ktc_str_from(data, 4);
    KTC_ASSERT(s.len == 4, "byte slice length is 4");
    KTC_ASSERT(ktc_str_eq_cstr(s, "GET "), "matches expected C string");
}

static void test_eq(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_eq");
    ktc_str a = ktc_str_from_cstr("foo");
    ktc_str b = ktc_str_from_cstr("foo");
    ktc_str c = ktc_str_from_cstr("bar");

    KTC_ASSERT(ktc_str_eq(a, b), "identical strings return true");
    KTC_ASSERT(!ktc_str_eq(a, c), "differing strings return false");
    KTC_ASSERT(ktc_str_eq(ktc_str_null(), ktc_str_null()), "null strings equal null strings");
}

static void test_eq_cstr(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_eq_cstr");
    ktc_str s = ktc_str_from_cstr("Host");
    KTC_ASSERT(ktc_str_eq_cstr(s, "Host"), "exact case match returns true");
    KTC_ASSERT(!ktc_str_eq_cstr(s, "host"), "case sensitive match fails on lower case");
}

static void test_eq_case_insensitive(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_eq_case_insensitive");
    ktc_str host = ktc_str_from_cstr("Host");
    KTC_ASSERT(ktc_str_eq_case_insensitive(host, ktc_str_from_cstr("host")), "matches lowercase");
    KTC_ASSERT(ktc_str_eq_case_insensitive(host, ktc_str_from_cstr("HOST")), "matches uppercase");
    KTC_ASSERT(!ktc_str_eq_case_insensitive(host, ktc_str_from_cstr("Hosts")),
               "fails on length mismatch");
}

static void test_cmp(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str_cmp");
    ktc_str abc = ktc_str_from_cstr("abc");
    ktc_str abd = ktc_str_from_cstr("abd");
    ktc_str ab = ktc_str_from_cstr("ab");

    KTC_ASSERT(ktc_str_cmp(abc, abd) < 0, "abc < abd");
    KTC_ASSERT(ktc_str_cmp(abd, abc) > 0, "abd > abc");
    KTC_ASSERT(ktc_str_cmp(abc, abc) == 0, "abc == abc");
    KTC_ASSERT(ktc_str_cmp(ab, abc) < 0, "ab < abc");
}

static void test_str_edge_cases(void) {
    KTC_TEST_CASE("Phase-0.4 (Memory Primitives)", "ktc_str edge cases");
    ktc_str s_null = ktc_str_null();
    KTC_ASSERT(ktc_str_is_empty(s_null), "null string is empty");

    ktc_str s_empty = ktc_str_from((const uint8_t *)"", 0);
    KTC_ASSERT(ktc_str_is_empty(s_empty), "0-length slice is empty");

    ktc_str s_a = ktc_str_from_cstr("abcdefghijklmnopqrstuvwxyz");
    ktc_str s_b = ktc_str_from_cstr("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    KTC_ASSERT(ktc_str_eq_case_insensitive(s_a, s_b), "full alphabet case-insensitive match");

    ktc_str s_special1 = ktc_str_from_cstr("host-name_123");
    ktc_str s_special2 = ktc_str_from_cstr("HOST-NAME_123");
    KTC_ASSERT(ktc_str_eq_case_insensitive(s_special1, s_special2),
               "special chars with case match");

    ktc_str s_bracket1 = ktc_str_from_cstr("a[b");
    ktc_str s_bracket2 = ktc_str_from_cstr("A[B");
    KTC_ASSERT(ktc_str_eq_case_insensitive(s_bracket1, s_bracket2),
               "bracket chars between Z and a handle correctly");
}

int main(void) {
    KTC_TEST_SUITE_START("Phase 0.4: ktc_str Memory Slices");
    test_null();
    test_from_cstr();
    test_from_bytes();
    test_eq();
    test_eq_cstr();
    test_eq_case_insensitive();
    test_cmp();
    test_str_edge_cases();
    KTC_TEST_SUITE_END();
}
