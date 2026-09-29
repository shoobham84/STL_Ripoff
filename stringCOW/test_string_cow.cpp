#include "basic_string_cow.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

using str = moo::basic_string_cow<char>;

// ============================================================
// Compile-Time Checks (static_assert)
// ============================================================

// Type layout: the whole point of COW is a single pointer
static_assert(sizeof(str) == sizeof(void*),
    "basic_string_cow should be exactly one pointer wide");

// Type aliases are correct
static_assert(std::is_same_v<str::value_type, char>);
static_assert(std::is_same_v<str::size_type, std::size_t>);
static_assert(std::is_same_v<str::iterator, char*>);
static_assert(std::is_same_v<str::const_iterator, const char*>);
static_assert(std::is_same_v<str::reference_type, char&>);
static_assert(std::is_same_v<str::const_reference, const char&>);

// Move operations must be noexcept for efficient container usage
static_assert(std::is_nothrow_move_constructible_v<str>,
    "Move constructor must be noexcept");
static_assert(std::is_nothrow_move_assignable_v<str>,
    "Move assignment must be noexcept");
static_assert(std::is_nothrow_destructible_v<str>,
    "Destructor must be noexcept");

// Default constructible
static_assert(std::is_default_constructible_v<str>);

// Copy constructible and assignable
static_assert(std::is_copy_constructible_v<str>);
static_assert(std::is_copy_assignable_v<str>);

// Convertible to string_view
static_assert(std::is_convertible_v<str, std::string_view>,
    "Must be implicitly convertible to string_view");

// npos must be the max value
static_assert(str::npos == static_cast<std::size_t>(-1));

// CharacterType concept should accept char and reject non-trivial types
static_assert(moo::CharacterType<char>);
static_assert(moo::CharacterType<wchar_t>);
static_assert(moo::CharacterType<char16_t>);
static_assert(moo::CharacterType<char32_t>);
static_assert(moo::CharacterType<unsigned char>);
static_assert(!moo::CharacterType<std::string>,
    "std::string is NOT trivially copyable, concept must reject it");
static_assert(!moo::CharacterType<std::vector<int> >,
    "std::vector is NOT trivially copyable, concept must reject it");


// ============================================================
// Runtime Tests (assert)
// ============================================================

void test_default_constructor() {
    str s;
    assert(s.size() == 0);
    assert(s.length() == 0);
    assert(s.capacity() == 0);
    assert(s.empty());
    assert(s.c_str() != nullptr);
    assert(s.c_str()[0] == '\0');
    std::cout << "  [PASS] default constructor\n";
}

void test_cstring_constructor() {
    str s("Hello");
    assert(s.size() == 5);
    assert(s.length() == 5);
    assert(!s.empty());
    assert(std::strcmp(s.c_str(), "Hello") == 0);

    // Empty C-string should behave like default
    str s2("");
    assert(s2.size() == 0);
    assert(s2.empty());

    // Null pointer should behave like default
    str s3(nullptr);
    assert(s3.size() == 0);
    assert(s3.empty());

    std::cout << "  [PASS] c-string constructor\n";
}

void test_copy_constructor() {
    str s1("Hello World");
    str s2 = s1;

    // Must have same content
    assert(s2.size() == s1.size());
    assert(std::strcmp(s1.c_str(), s2.c_str()) == 0);

    // COW: must share the same underlying buffer
    assert(s1.c_str() == s2.c_str());

    std::cout << "  [PASS] copy constructor\n";
}

void test_move_constructor() {
    str s1("Move me");
    const char* original_ptr = s1.c_str();
    std::size_t original_size = s1.size();

    str s2 = std::move(s1);

    // s2 stole s1's buffer
    assert(s2.c_str() == original_ptr);
    assert(s2.size() == original_size);
    assert(std::strcmp(s2.c_str(), "Move me") == 0);

    // s1 must be in valid empty state
    assert(s1.empty());
    assert(s1.size() == 0);

    std::cout << "  [PASS] move constructor\n";
}

void test_copy_assignment() {
    str s1("Original");
    str s2("Replace me");
    s2 = s1;

    // Must share buffer after copy assignment (COW)
    assert(s2.c_str() == s1.c_str());
    assert(s2.size() == s1.size());
    assert(std::strcmp(s2.c_str(), "Original") == 0);

    std::cout << "  [PASS] copy assignment\n";
}

void test_move_assignment() {
    str s1("Source");
    const char* original_ptr = s1.c_str();
    str s2("Target");

    s2 = std::move(s1);

    assert(s2.c_str() == original_ptr);
    assert(std::strcmp(s2.c_str(), "Source") == 0);

    std::cout << "  [PASS] move assignment\n";
}

void test_self_assignment() {
    str s1("SelfTest");
    const char* ptr_before = s1.c_str();

    // Copy self-assignment must be safe
    s1 = s1;
    assert(s1.c_str() == ptr_before);
    assert(std::strcmp(s1.c_str(), "SelfTest") == 0);

    // Move self-assignment must be safe
    s1 = std::move(s1);
    // After self-move, just verify it doesn't crash and is in a valid state
    (void)s1.size();

    std::cout << "  [PASS] self-assignment\n";
}

void test_cow_detach_on_write() {
    str s1("Shared");
    str s2 = s1;

    // Before mutation: same buffer
    assert(s1.c_str() == s2.c_str());

    // Mutate s1 via operator[]
    s1[0] = 'X';

    // After mutation: buffers must have diverged
    assert(s1.c_str() != s2.c_str());

    // s1 was modified
    assert(std::strcmp(s1.c_str(), "Xhared") == 0);

    // s2 is untouched
    assert(std::strcmp(s2.c_str(), "Shared") == 0);

    std::cout << "  [PASS] COW detach on write\n";
}

void test_cow_chain() {
    // Create a chain of copies, then mutate the middle one
    str s1("Chain");
    str s2 = s1;
    str s3 = s2;

    // All 3 share the same buffer
    assert(s1.c_str() == s2.c_str());
    assert(s2.c_str() == s3.c_str());

    // Mutate s2
    s2[0] = 'Z';

    // s2 got its own buffer, s1 and s3 still share
    assert(s1.c_str() != s2.c_str());
    assert(s1.c_str() == s3.c_str());
    assert(std::strcmp(s1.c_str(), "Chain") == 0);
    assert(std::strcmp(s2.c_str(), "Zhain") == 0);
    assert(std::strcmp(s3.c_str(), "Chain") == 0);

    std::cout << "  [PASS] COW chain sharing\n";
}

void test_const_access_no_detach() {
    str s1("NoDetach");
    str s2 = s1;

    // Const operator[] must NOT detach
    const str& cs1 = s1;
    char c = cs1[0];
    assert(c == 'N');

    // Still shared after const access
    assert(s1.c_str() == s2.c_str());

    std::cout << "  [PASS] const access does not detach\n";
}

void test_operator_index() {
    str s("abcde");

    // Read via const
    const str& cs = s;
    assert(cs[0] == 'a');
    assert(cs[4] == 'e');

    // Write
    s[2] = 'Z';
    assert(std::strcmp(s.c_str(), "abZde") == 0);

    std::cout << "  [PASS] operator[]\n";
}

void test_push_back() {
    str s;
    assert(s.empty());

    s.push_back('H');
    s.push_back('i');
    s.push_back('!');

    assert(s.size() == 3);
    assert(std::strcmp(s.c_str(), "Hi!") == 0);

    // Null-terminator must be in place
    assert(s.c_str()[3] == '\0');

    std::cout << "  [PASS] push_back\n";
}

void test_push_back_growth() {
    str s;

    // Push enough characters to trigger multiple capacity growths
    for (int i = 0; i < 100; ++i) {
        s.push_back('x');
    }
    assert(s.size() == 100);
    assert(s.capacity() >= 100);

    // Verify all characters are correct
    for (std::size_t i = 0; i < 100; ++i) {
        const str& cs = s;
        assert(cs[i] == 'x');
    }

    // Verify null termination
    assert(s.c_str()[100] == '\0');

    std::cout << "  [PASS] push_back growth\n";
}

void test_push_back_detaches_shared() {
    str s1("AB");
    str s2 = s1; // shared

    assert(s1.c_str() == s2.c_str());

    s1.push_back('C');

    // Must have detached
    assert(s1.c_str() != s2.c_str());
    assert(std::strcmp(s1.c_str(), "ABC") == 0);
    assert(std::strcmp(s2.c_str(), "AB") == 0);

    std::cout << "  [PASS] push_back detaches shared buffer\n";
}

void test_reserve() {
    str s("tiny");
    std::size_t old_cap = s.capacity();

    s.reserve(1000);
    assert(s.capacity() >= 1000);
    // Content must be preserved
    assert(std::strcmp(s.c_str(), "tiny") == 0);
    assert(s.size() == 4);

    // Reserve smaller than current capacity should be a no-op
    s.reserve(5);
    assert(s.capacity() >= 1000); // capacity should NOT shrink

    std::cout << "  [PASS] reserve\n";
}

void test_clear() {
    str s("Clear me");
    assert(!s.empty());

    s.clear();
    assert(s.empty());
    assert(s.size() == 0);
    assert(s.c_str()[0] == '\0');

    // Clearing an already empty string must be safe
    s.clear();
    assert(s.empty());

    std::cout << "  [PASS] clear\n";
}

void test_clear_cow_independence() {
    str s1("Shared");
    str s2 = s1;

    s1.clear();

    // s1 is empty, s2 is untouched
    assert(s1.empty());
    assert(std::strcmp(s2.c_str(), "Shared") == 0);
    assert(s2.size() == 6);

    std::cout << "  [PASS] clear COW independence\n";
}

void test_equality_operator() {
    str s1("same");
    str s2("same");
    str s3("diff");

    assert(s1 == s2);
    assert(!(s1 == s3));

    // COW copies must be equal (fast path: pointer comparison)
    str s4 = s1;
    assert(s1 == s4);

    // Empty strings
    str e1, e2;
    assert(e1 == e2);

    std::cout << "  [PASS] operator==\n";
}

void test_spaceship_operator() {
    str a("apple");
    str b("banana");
    str c("apple");

    assert((a <=> b) < 0);
    assert((b <=> a) > 0);
    assert((a <=> c) == 0);

    // COW copies: fast path
    str d = a;
    assert((a <=> d) == 0);

    std::cout << "  [PASS] operator<=>\n";
}

void test_string_view_conversion() {
    str s("ViewMe");
    std::string_view sv = s;

    assert(sv.size() == 6);
    assert(sv == "ViewMe");

    // Modifying through string_view is impossible (const),
    // verify the data pointer matches
    assert(sv.data() == s.c_str());

    std::cout << "  [PASS] string_view conversion\n";
}

void test_starts_with() {
    str s("Hello World");
    assert(s.starts_with("Hello"));
    assert(s.starts_with("H"));
    assert(s.starts_with("Hello World"));
    assert(!s.starts_with("World"));
    assert(s.starts_with(""));

    std::cout << "  [PASS] starts_with\n";
}

void test_ends_with() {
    str s("Hello World");
    assert(s.ends_with("World"));
    assert(s.ends_with("d"));
    assert(s.ends_with("Hello World"));
    assert(!s.ends_with("Hello"));
    assert(s.ends_with(""));

    std::cout << "  [PASS] ends_with\n";
}

void test_contains() {
    str s("The quick brown fox");
    assert(s.contains("quick"));
    assert(s.contains("brown fox"));
    assert(s.contains("The"));
    assert(!s.contains("lazy"));
    assert(s.contains(""));

    std::cout << "  [PASS] contains\n";
}

void test_find() {
    str s("abcabc");
    assert(s.find("abc") == 0);
    assert(s.find("abc", 1) == 3);
    assert(s.find("xyz") == str::npos);
    assert(s.find("c") == 2);

    std::cout << "  [PASS] find\n";
}

void test_stream_output() {
    str s("StreamTest");
    std::ostringstream oss;
    oss << s;
    assert(oss.str() == "StreamTest");

    std::cout << "  [PASS] operator<<\n";
}

void test_empty_string_operations() {
    // Ensure no crashes on operations with empty strings
    str e;
    str e2 = e; // copy empty
    str e3 = std::move(e); // move empty

    assert(e2.empty());
    assert(e3.empty());

    e2.clear();
    assert(e2.empty());

    assert(e2 == e3);

    // push_back on empty
    e2.push_back('a');
    assert(e2.size() == 1);
    assert(std::strcmp(e2.c_str(), "a") == 0);

    std::cout << "  [PASS] empty string operations\n";
}

void test_multiple_detach_cycles() {
    // Create a string, copy it many times, mutate each copy
    str original("DETACH");

    for (int i = 0; i < 50; ++i) {
        str copy = original;
        assert(copy.c_str() == original.c_str()); // shared
        copy[0] = static_cast<char>('A' + (i % 26));
        assert(copy.c_str() != original.c_str()); // detached
    }

    // Original must be completely untouched
    assert(std::strcmp(original.c_str(), "DETACH") == 0);

    std::cout << "  [PASS] multiple detach cycles\n";
}

void test_scope_destruction_order() {
    // Ensure that destroying copies in various orders doesn't crash
    str* s1 = new str("Scope");
    str* s2 = new str(*s1);
    str* s3 = new str(*s2);

    // Destroy in reverse order
    delete s3;
    delete s1;
    // s2 must still be valid
    assert(std::strcmp(s2->c_str(), "Scope") == 0);
    delete s2;

    std::cout << "  [PASS] scope destruction order\n";
}

int main() {
    std::cout << "=== basic_string_cow Test Suite ===\n\n";

    std::cout << "[Compile-time checks passed (static_assert)]\n\n";

    std::cout << "[Runtime tests]\n";
    test_default_constructor();
    test_cstring_constructor();
    test_copy_constructor();
    test_move_constructor();
    test_copy_assignment();
    test_move_assignment();
    test_self_assignment();
    test_cow_detach_on_write();
    test_cow_chain();
    test_const_access_no_detach();
    test_operator_index();
    test_push_back();
    test_push_back_growth();
    test_push_back_detaches_shared();
    test_reserve();
    test_clear();
    test_clear_cow_independence();
    test_equality_operator();
    test_spaceship_operator();
    test_string_view_conversion();
    test_starts_with();
    test_ends_with();
    test_contains();
    test_find();
    test_stream_output();
    test_empty_string_operations();
    test_multiple_detach_cycles();
    test_scope_destruction_order();

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
