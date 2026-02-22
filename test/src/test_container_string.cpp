#include "di/container/interface/erase.h"
#include "di/container/string/encoding.h"
#include "di/container/string/prelude.h"
#include "di/container/string/string.h"
#include "di/test/prelude.h"

namespace container_string {
constexpr static void basic() {
    auto x = u8"QWER"_sv;
    (void) x;

    ASSERT_EQ(x.size_code_units(), 4U);

    int c = 0;
    for (auto xx : x) {
        (void) xx;
        c++;
    }
    ASSERT_EQ(c, 4);

    auto y = x.to_owned();
    ASSERT_EQ(x, y);
}

constexpr static void push_back() {
    auto x = di::String {};
    x.push_back('a');
    x.push_back('b');
    x.push_back('c');
    ASSERT_EQ(x.size_code_units(), 3U);
    ASSERT_EQ(x, u8"abc"_sv);

    x.append(u8"def"_sv);
    ASSERT_EQ(x, u8"abcdef"_sv);

    x += "ghj"_sv;
    ASSERT_EQ(x, u8"abcdefghj"_sv);

    auto y = di::container::string::StringImpl<di::container::string::Utf8Encoding,
                                               di::StaticVector<c8, di::Constexpr<64ZU>>> {};
    (void) y.push_back(U'a');

    ASSERT_EQ(y, "a"_sv);
}

constexpr static void mutation() {
    auto s = u8"Hello, 世界, Hello 友達!"_s;

    ASSERT_EQ(s.erase(s.rfind(U',').begin()), s.iterator_at_offset(13));
    ASSERT_EQ(s, u8"Hello, 世界 Hello 友達!"_sv);

    ASSERT_EQ(s.erase(s.find(U'世').begin(), s.find(U'界').end()), s.iterator_at_offset(7));
    ASSERT_EQ(s, u8"Hello,  Hello 友達!"_sv);

    ASSERT_EQ(s.pop_back(), U'!');
    ASSERT_EQ(s, u8"Hello,  Hello 友達"_sv);

    ASSERT_EQ(s.insert(s.find(U' ').begin(), U'!').begin(), *s.iterator_at_offset(6));
    ASSERT_EQ(s, u8"Hello,!  Hello 友達"_sv);

    ASSERT_EQ(s.insert(s.find(U' ').end(), "!!!"_sv).begin(), *s.iterator_at_offset(8));
    ASSERT_EQ(s, u8"Hello,! !!! Hello 友達"_sv);

    ASSERT_EQ(s.replace(s.find(U' ').begin(), s.find(U' ').end(), "World"_sv).begin(), *s.iterator_at_offset(7));
    ASSERT_EQ(s, u8"Hello,!World!!! Hello 友達"_sv);

    auto t = "Hello, World!"_ts;
    t[5] = '!';

    ASSERT_EQ(t, "Hello! World!"_tsv);

    t.erase(5);
    ASSERT_EQ(t, "Hello"_tsv);

    t.erase(1, 2);
    ASSERT_EQ(t, "Hlo"_tsv);

    ASSERT_EQ(t.pop_back(), 'o');
    ASSERT_EQ(t, "Hl"_tsv);

    t.insert(1, 'e');
    ASSERT_EQ(t, "Hel"_tsv);

    t.insert(3, "lo"_tsv);
    ASSERT_EQ(t, "Hello"_tsv);

    t.replace(1, 2, "i"_tsv);
    ASSERT_EQ(t, "Hilo"_tsv);

    auto w = u8"Hello, 世界, Hello 友達!"_s;
    ASSERT_EQ(di::erase_if(w,
                           [](auto c) {
                               return c == U'世' || c == U'界';
                           }),
              2ZU);
    ASSERT_EQ(w, u8"Hello, , Hello 友達!"_sv);
}

constexpr static void to() {
    auto x = u8"abc"_sv | di::to<di::String>();
    ASSERT_EQ(x, u8"abc"_sv);

    auto y = di::Array { u8'a', u8'b', u8'c' } | di::to<di::String>(di::encoding::assume_valid);
    ASSERT_EQ(y, "abc"_sv);

    auto z = di::Array { u8'a', u8'b', u8'c' } | di::to<di::String>();
    ASSERT_EQ(z, "abc"_sv);

    auto a = "abc"_tsv | di::to<di::TransparentString>();
    ASSERT_EQ(a, "abc"_tsv);

    auto b = di::Array { 'a', 'b', 'c' } | di::to<di::TransparentString>();
    ASSERT_EQ(b, "abc"_tsv);
}

constexpr static void erased() {
    di::ErasedString s = u8"abc"_sv;
    ASSERT_EQ(s, u8"abc"_sv);
}

static void erased_string() {
    di::ErasedString t = u8"def"_s;
    ASSERT_EQ(t, u8"def"_sv);

    auto q = di::move(t);
    ASSERT_EQ(t, u8""_sv);
    ASSERT_EQ(q, u8"def"_sv);
}

constexpr static void do_utf8_test(di::StringView view, di::Vector<char32_t> const& desired) {
    // Check the iteration produces the same results forwards and backwards.
    auto forwards = view | di::to<di::Vector>();
    auto backwards = view | di::reverse | di::to<di::Vector>();
    di::container::reverse(backwards);

    ASSERT_EQ(forwards, backwards);
    ASSERT_EQ(forwards, desired);
    ASSERT_EQ(backwards, desired);
};

constexpr static void utf8() {
    auto x = u8"$¢€𐍈"_sv;

    ASSERT(x.starts_with(u8"$"_sv));
    ASSERT(x.ends_with(u8"€𐍈"_sv));
    ASSERT(!x.ends_with(u8"¢"_sv));

    ASSERT_EQ(x.front(), U'$');
    ASSERT_EQ(x.back(), U'𐍈');

    ASSERT_EQ(x.size_bytes(), 10U);
    ASSERT_EQ(di::distance(x), 4);

    ASSERT(u8"¢€"_sv == x.substr(*x.iterator_at_offset(1), *x.iterator_at_offset(6)));
    ASSERT(!x.iterator_at_offset(2));
    ASSERT(!x.iterator_at_offset(4));

    ASSERT(x.substr(x.begin(), *x.iterator_at_offset(1)) == u8"$"_sv);
    ASSERT(x.substr(*x.iterator_at_offset(1)) == u8"¢€𐍈"_sv);

    auto s = di::String {};
    s.push_back(U'$');
    s.push_back(U'¢');
    s.push_back(U'€');
    s.push_back(U'𐍈');

    ASSERT_EQ(s, u8"$¢€𐍈"_sv);
    do_utf8_test(s.view(), di::Array {
                               U'\x24',
                               U'\xA2',
                               U'\x20AC',
                               U'\x10348',
                           } | di::to<di::Vector>());

    auto validate = [](c8 const* x, size_t n) {
        auto span = di::Span(x, n);
        auto encoding = di::container::string::Utf8Encoding {};
        return di::encoding::validate(encoding, span);
    };

    ASSERT(!validate(u8"\xFF", 1));
    ASSERT(!validate(u8"\xC0\xAF", 2));
    ASSERT(!validate(u8"\xE0\x0F\x80", 3));
    ASSERT(!validate(u8"\xF0\x82\x82\xAC", 4));
}

constexpr static void readonly_api() {
    auto s = u8"Hello, 世界, Hello 友達!"_sv;

    ASSERT(s.starts_with("Hel"_sv));
    ASSERT(s.starts_with(U'H'));

    ASSERT(s.ends_with(u8"友達!"_sv));
    ASSERT(s.ends_with(U'!'));

    ASSERT(s.contains(u8"世界"_sv));

    ASSERT_EQ(s.find("Hello"_sv).begin(), s.iterator_at_offset(0));
    ASSERT_EQ(s.find(U'界').begin(), s.iterator_at_offset(u8"Hello, 世"_sv.size_code_units()));

    ASSERT_EQ(s.rfind("llo"_sv).begin(), s.iterator_at_offset(u8"Hello, 世界, He"_sv.size_code_units()));
    ASSERT_EQ(s.rfind("llo"_sv).end(), s.iterator_at_offset(u8"Hello, 世界, Hello"_sv.size_code_units()));
    ASSERT_EQ(s.rfind(U'l').begin(), s.iterator_at_offset(u8"Hello, 世界, Hel"_sv.size_code_units()));

    ASSERT_EQ(*s.find_first_of(u8"o達"_sv), U'o');
    ASSERT_EQ(*s.find_last_of(u8"o達"_sv), U'達');

    ASSERT_EQ(*s.find_first_not_of(u8"o達!"_sv), U'H');
    ASSERT_EQ(*s.find_last_not_of(u8"o達!"_sv), U'友');

    auto t = "Asdf"_tsv;
    ASSERT_EQ(t[0], 'A');
    ASSERT_EQ(t.at(0), 'A');
    ASSERT_EQ(t.at(4), di::nullopt);
    ASSERT_EQ(t.substr(1, 2), "sd"_tsv);
    ASSERT_EQ(t.substr(4, 2), ""_tsv);
}

constexpr static void null_terminated() {
    auto s = di::TransparentString {};

    s.push_back('a');
    ASSERT_EQ(di::distance(di::ZCString { s.c_str() }), 1);

    s.push_back('b');
    ASSERT_EQ(di::distance(di::ZCString { s.c_str() }), 2);
}

TESTC(container_string, basic)
TESTC(container_string, push_back)
TESTC(container_string, mutation)
TESTC(container_string, to)
TESTC(container_string, erased)
TEST(container_string, erased_string)
TESTC(container_string, utf8)
TESTC(container_string, readonly_api)
TESTC(container_string, null_terminated)
}
