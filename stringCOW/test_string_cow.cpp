#include "basic_string_cow.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>


// ============================================================
// Compile-Time Checks (static_assert)
// ============================================================

// 32-byte layout for SSO + COW
static_assert(sizeof(moo::string) == 32, "size of moo::string must be 32 bytes");

// Type aliases are correct
static_assert(std::is_same_v<moo::string::value_type, char>);
static_assert(std::is_same_v<moo::string::size_type, std::size_t>);
static_assert(std::is_same_v<moo::string::iterator, char*>);
static_assert(std::is_same_v<moo::string::const_iterator, const char*>);
static_assert(std::is_same_v<moo::string::reference_type, char&>);
static_assert(std::is_same_v<moo::string::const_reference, const char&>);

// Move operations must be noexcept
static_assert(std::is_nothrow_move_constructible_v<moo::string>,
    "Move constructor must be noexcept");
static_assert(std::is_nothrow_move_assignable_v<moo::string>,
    "Move assignment must be noexcept");
static_assert(std::is_nothrow_destructible_v<moo::string>,
    "Destructor must be noexcept");

// Default, Copy constructible and assignable
static_assert(std::is_default_constructible_v<moo::string>);
static_assert(std::is_copy_constructible_v<moo::string>);
static_assert(std::is_copy_assignable_v<moo::string>);

// Convertible to string_view
static_assert(std::is_convertible_v<moo::string, std::string_view>,
    "Must be implicitly convertible to string_view");

// npos must be max size_t
static_assert(moo::string::npos == static_cast<std::size_t>(-1));

// CharacterType concept checks
static_assert(moo::CharacterType<char>);
static_assert(moo::CharacterType<wchar_t>);
static_assert(moo::CharacterType<char16_t>);
static_assert(moo::CharacterType<char32_t>);
static_assert(moo::CharacterType<unsigned char>);
static_assert(!moo::CharacterType<std::string>);
static_assert(!moo::CharacterType<std::vector<int>>);


// ============================================================
// Runtime Tests (assert)
// ============================================================

void test_default_constructor() {
    moo::string s;
    assert(s.size() == 0);
    assert(s.length() == 0);
    assert(s.capacity() == 30); // SSO capacity is 30
    assert(s.empty());
    assert(s.c_str() != nullptr);
    assert(s.c_str()[0] == '\0');
    std::cout << "  [PASS] default constructor\n";
}

void test_cstring_constructor() {
    // Small string (SSO)
    moo::string s("Hello");
    assert(s.size() == 5);
    assert(s.length() == 5);
    assert(s.capacity() == 30);
    assert(!s.empty());
    assert(std::strcmp(s.c_str(), "Hello") == 0);

    // Large string (> 30 chars, Heap)
    const char* long_text = "This is a long string that exceeds the SSO capacity of 30 bytes!";
    moo::string s_long(long_text);
    assert(s_long.size() == std::strlen(long_text));
    assert(s_long.capacity() >= s_long.size());
    assert(std::strcmp(s_long.c_str(), long_text) == 0);

    // Empty C-string
    moo::string s2("");
    assert(s2.size() == 0);
    assert(s2.empty());

    // Null pointer
    moo::string s3(nullptr);
    assert(s3.size() == 0);
    assert(s3.empty());

    std::cout << "  [PASS] c-string constructor\n";
}

void test_copy_constructor() {
    // 1. SSO copy: independent stack buffers
    moo::string s1("Hello World");
    moo::string s2 = s1;
    assert(s2.size() == s1.size());
    assert(std::strcmp(s1.c_str(), s2.c_str()) == 0);
    assert(s1.c_str() != s2.c_str()); // SSO buffers live on separate stack frames

    // 2. Heap COW copy: shared buffer
    const char* long_text = "This is a long string that exceeds the SSO capacity of 31 bytes!";
    moo::string h1(long_text);
    moo::string h2 = h1;
    assert(h2.size() == h1.size());
    assert(std::strcmp(h1.c_str(), h2.c_str()) == 0);
    assert(h1.c_str() == h2.c_str()); // COW: shared underlying buffer!

    std::cout << "  [PASS] copy constructor\n";
}

void test_move_constructor() {
    // Move SSO
    moo::string s1("Move SSO");
    moo::string s2 = std::move(s1);
    assert(std::strcmp(s2.c_str(), "Move SSO") == 0);
    assert(s1.empty());

    // Move Heap
    const char* long_text = "A long string that uses heap allocation for move constructor testing!";
    moo::string h1(long_text);
    const char* original_ptr = h1.c_str();
    std::size_t original_size = h1.size();

    moo::string h2 = std::move(h1);
    assert(h2.c_str() == original_ptr);
    assert(h2.size() == original_size);
    assert(std::strcmp(h2.c_str(), long_text) == 0);
    assert(h1.empty());

    std::cout << "  [PASS] move constructor\n";
}

void test_copy_assignment() {
    // SSO copy assignment
    moo::string s1("Original");
    moo::string s2("Replace me");
    s2 = s1;
    assert(s2.size() == s1.size());
    assert(std::strcmp(s2.c_str(), "Original") == 0);
    assert(s2.c_str() != s1.c_str());

    // Heap COW copy assignment
    moo::string h1("A long string for testing copy assignment in COW heap mode!");
    moo::string h2("Short");
    h2 = h1;
    assert(h2.c_str() == h1.c_str()); // COW: shares buffer
    assert(std::strcmp(h2.c_str(), h1.c_str()) == 0);

    std::cout << "  [PASS] copy assignment\n";
}

void test_move_assignment() {
    // Move assignment Heap -> Heap
    moo::string h1("Source long string for move assignment test!");
    const char* original_ptr = h1.c_str();
    moo::string h2("Target");

    h2 = std::move(h1);
    assert(h2.c_str() == original_ptr);
    assert(std::strcmp(h2.c_str(), "Source long string for move assignment test!") == 0);

    std::cout << "  [PASS] move assignment\n";
}

void test_self_assignment() {
    moo::string s1("SelfTest");
    s1 = s1;
    assert(std::strcmp(s1.c_str(), "SelfTest") == 0);

    moo::string h1("A long string for testing self-assignment in COW mode!");
    const char* ptr = h1.c_str();
    h1 = h1;
    assert(h1.c_str() == ptr);
    assert(std::strcmp(h1.c_str(), "A long string for testing self-assignment in COW mode!") == 0);

    std::cout << "  [PASS] self-assignment\n";
}

void test_cow_detach_on_write() {
    // Must be in Heap mode (> 31 chars) to test COW detach
    moo::string s1("A long string that exceeds 31 chars to test COW detach!");
    moo::string s2 = s1;

    assert(s1.c_str() == s2.c_str()); // shared

    s1[0] = 'X'; // trigger detach

    assert(s1.c_str() != s2.c_str()); // detached!
    assert(s1[0] == 'X');
    assert(s2[0] == 'A');

    std::cout << "  [PASS] COW detach on write\n";
}

void test_cow_chain() {
    moo::string s1("Chain of multiple shared COW string instances that are long!");
    moo::string s2 = s1;
    moo::string s3 = s2;

    assert(s1.c_str() == s2.c_str());
    assert(s2.c_str() == s3.c_str());

    s2[0] = 'Z'; // mutate middle

    assert(s1.c_str() != s2.c_str());
    assert(s1.c_str() == s3.c_str()); // s1 and s3 still share!
    assert(s2[0] == 'Z');
    assert(s1[0] == 'C');
    assert(s3[0] == 'C');

    std::cout << "  [PASS] COW chain sharing\n";
}

void test_const_access_no_detach() {
    moo::string s1("NoDetach on a long shared COW heap string instance!");
    moo::string s2 = s1;

    const moo::string& cs1 = s1;
    char c = cs1[0];
    assert(c == 'N');
    assert(s1.c_str() == s2.c_str()); // still shared!

    std::cout << "  [PASS] const access does not detach\n";
}

void test_operator_index() {
    moo::string s("abcde");
    const moo::string& cs = s;
    assert(cs[0] == 'a');
    assert(cs[4] == 'e');

    s[2] = 'Z';
    assert(std::strcmp(s.c_str(), "abZde") == 0);

    std::cout << "  [PASS] operator[]\n";
}

void test_push_back() {
    moo::string s;
    assert(s.empty());

    s.push_back('H');
    s.push_back('i');
    s.push_back('!');

    assert(s.size() == 3);
    assert(std::strcmp(s.c_str(), "Hi!") == 0);
    assert(s.c_str()[3] == '\0');

    std::cout << "  [PASS] push_back\n";
}

void test_sso_to_heap_promotion() {
    moo::string s;
    // Push 30 characters: should stay in SSO
    for (int i = 0; i < 30; ++i) {
        s.push_back(static_cast<char>('a' + (i % 26)));
    }
    assert(s.size() == 30);
    assert(s.capacity() == 30);

    // 31st push: triggers promotion from SSO to Heap!
    s.push_back('!');
    assert(s.size() == 31);
    assert(s.capacity() >= 31);
    assert(s[30] == '!');
    assert(s.c_str()[31] == '\0');

    // Continue pushing in Heap mode
    for (int i = 0; i < 50; ++i) {
        s.push_back('x');
    }
    assert(s.size() == 81);
    assert(s.c_str()[81] == '\0');

    std::cout << "  [PASS] SSO to heap promotion\n";
}

void test_push_back_detaches_shared() {
    moo::string s1("A long shared COW string before push_back testing!");
    moo::string s2 = s1;

    assert(s1.c_str() == s2.c_str());

    s1.push_back('X');

    assert(s1.c_str() != s2.c_str());
    assert(s1.ends_with("X"));
    assert(!s2.ends_with("X"));

    std::cout << "  [PASS] push_back detaches shared buffer\n";
}

void test_reserve() {
    moo::string s("tiny");

    // Reserve within SSO
    s.reserve(20);
    assert(s.capacity() == 30); // SSO capacity remains 30

    // Reserve beyond SSO -> promotes to Heap
    s.reserve(1000);
    assert(s.capacity() >= 1000);
    assert(std::strcmp(s.c_str(), "tiny") == 0);
    assert(s.size() == 4);

    // Smaller reserve should not shrink
    s.reserve(5);
    assert(s.capacity() >= 1000);

    std::cout << "  [PASS] reserve\n";
}

void test_clear() {
    // Clear SSO
    moo::string s("Clear me");
    assert(!s.empty());
    s.clear();
    assert(s.empty());
    assert(s.size() == 0);
    assert(s.c_str()[0] == '\0');

    // Clear Heap
    moo::string h("A long string that is in heap mode before clear!");
    h.clear();
    assert(h.empty());
    assert(h.size() == 0);
    assert(h.c_str()[0] == '\0');

    std::cout << "  [PASS] clear\n";
}

void test_clear_cow_independence() {
    moo::string s1("Shared long string for testing clear independence!");
    moo::string s2 = s1;

    assert(s1.c_str() == s2.c_str());
    s1.clear();

    assert(s1.empty());
    assert(!s2.empty());
    assert(std::strcmp(s2.c_str(), "Shared long string for testing clear independence!") == 0);

    std::cout << "  [PASS] clear COW independence\n";
}

void test_equality_operator() {
    moo::string s1("same");
    moo::string s2("same");
    moo::string s3("diff");

    assert(s1 == s2);
    assert(!(s1 == s3));

    // COW copies fast-path
    moo::string h1("A long string for equality check testing COW fast path!");
    moo::string h2 = h1;
    assert(h1 == h2);

    moo::string e1, e2;
    assert(e1 == e2);

    std::cout << "  [PASS] operator==\n";
}

void test_spaceship_operator() {
    moo::string a("apple");
    moo::string b("banana");
    moo::string c("apple");

    assert((a <=> b) < 0);
    assert((b <=> a) > 0);
    assert((a <=> c) == 0);

    moo::string h1("A long string for spaceship operator testing!");
    moo::string h2 = h1;
    assert((h1 <=> h2) == 0);

    std::cout << "  [PASS] operator<=>\n";
}

void test_string_view_conversion() {
    moo::string s("ViewMe");
    std::string_view sv = s;

    assert(sv.size() == 6);
    assert(sv == "ViewMe");
    assert(sv.data() == s.c_str());

    std::cout << "  [PASS] string_view conversion\n";
}

void test_starts_with() {
    moo::string s("Hello World");
    assert(s.starts_with("Hello"));
    assert(s.starts_with("H"));
    assert(s.starts_with("Hello World"));
    assert(!s.starts_with("World"));
    assert(s.starts_with(""));

    std::cout << "  [PASS] starts_with\n";
}

void test_ends_with() {
    moo::string s("Hello World");
    assert(s.ends_with("World"));
    assert(s.ends_with("d"));
    assert(s.ends_with("Hello World"));
    assert(!s.ends_with("Hello"));
    assert(s.ends_with(""));

    std::cout << "  [PASS] ends_with\n";
}

void test_contains() {
    moo::string s("The quick brown fox");
    assert(s.contains("quick"));
    assert(s.contains("brown fox"));
    assert(s.contains("The"));
    assert(!s.contains("lazy"));
    assert(s.contains(""));

    std::cout << "  [PASS] contains\n";
}

void test_find() {
    moo::string s("abcabc");
    assert(s.find("abc") == 0);
    assert(s.find("abc", 1) == 3);
    assert(s.find("xyz") == moo::string::npos);
    assert(s.find("c") == 2);

    std::cout << "  [PASS] find\n";
}

void test_stream_output() {
    moo::string s("StreamTest");
    std::ostringstream oss;
    oss << s;
    assert(oss.str() == "StreamTest");

    std::cout << "  [PASS] operator<<\n";
}

void test_empty_string_operations() {
    moo::string e;
    moo::string e2 = e;
    moo::string e3 = std::move(e);

    assert(e2.empty());
    assert(e3.empty());

    e2.clear();
    assert(e2.empty());
    assert(e2 == e3);

    e2.push_back('a');
    assert(e2.size() == 1);
    assert(std::strcmp(e2.c_str(), "a") == 0);

    std::cout << "  [PASS] empty string operations\n";
}

void test_multiple_detach_cycles() {
    moo::string original("DETACH - long string exceeding the SSO capacity of 31 bytes!");

    for (int i = 0; i < 50; ++i) {
        moo::string copy = original;
        assert(copy.c_str() == original.c_str());
        copy[0] = static_cast<char>('A' + (i % 26));
        assert(copy.c_str() != original.c_str());
    }

    assert(original[0] == 'D');
    std::cout << "  [PASS] multiple detach cycles\n";
}

void test_scope_destruction_order() {
    moo::string* s1 = new moo::string("Scope - long string exceeding SSO limit for heap destruction!");
    moo::string* s2 = new moo::string(*s1);
    moo::string* s3 = new moo::string(*s2);

    assert(s1->c_str() == s2->c_str());
    assert(s2->c_str() == s3->c_str());

    delete s3;
    delete s1;
    assert(s2->starts_with("Scope"));
    delete s2;

    std::cout << "  [PASS] scope destruction order\n";
}

void test_fill_constructor() {
    // SSO fill (<= 30)
    moo::string s1(10, 'a');
    assert(s1.size() == 10);
    assert(s1 == "aaaaaaaaaa");

    // Heap fill (> 30)
    moo::string s2(50, 'b');
    assert(s2.size() == 50);
    assert(s2.capacity() >= 50);
    assert(s2.starts_with("bbbbb"));

    std::cout << "  [PASS] fill constructor\n";
}

void test_sized_constructor() {
    moo::string s("HelloWorld", 5);
    assert(s.size() == 5);
    assert(s == "Hello");

    std::cout << "  [PASS] sized pointer constructor\n";
}

void test_append_and_concat() {
    // 1. SSO append (stays in SSO)
    moo::string s("Hello");
    s.append(" World");
    assert(s == "Hello World");

    // 2. operator+= (char)
    s += '!';
    assert(s == "Hello World!");

    // 3. SSO -> Heap promotion via append
    s += " - this will force promotion to heap mode because length exceeds 30!";
    assert(s.size() > 30);
    assert(s.starts_with("Hello World!"));

    // 4. Self-append test (s.append(s))
    moo::string self("Repeat ");
    self.append(self);
    assert(self == "Repeat Repeat ");

    std::cout << "  [PASS] append and operator+=\n";
}

void test_pop_back() {
    // SSO pop_back
    moo::string s1("abc");
    s1.pop_back();
    assert(s1 == "ab");
    assert(s1.size() == 2);

    // Heap pop_back
    moo::string s2("A long string exceeding thirty characters to test pop_back!");
    std::size_t old_len = s2.size();
    s2.pop_back();
    assert(s2.size() == old_len - 1);
    assert(!s2.ends_with("!"));

    std::cout << "  [PASS] pop_back\n";
}

void test_thread_safe_string() {
    using ts_str = moo::basic_string_cow<char, std::allocator<char>, true>;
    ts_str s1("Thread-safe long string exceeding the SSO limit of 31 bytes!");
    ts_str s2 = s1;
    assert(s1.c_str() == s2.c_str());
    s1[0] = 'X';
    assert(s1.c_str() != s2.c_str());
    std::cout << "  [PASS] thread-safe ts_string basic operations\n";
}

int main() {
    std::cout << "=== basic_string_cow Test Suite ===\n\n";

    std::cout << "[Compile-time checks passed (static_assert)]\n\n";

    std::cout << "[Runtime tests]\n";
    test_default_constructor();
    test_cstring_constructor();
    test_fill_constructor();
    test_sized_constructor();
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
    test_pop_back();
    test_append_and_concat();
    test_sso_to_heap_promotion();
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
    test_thread_safe_string();

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
