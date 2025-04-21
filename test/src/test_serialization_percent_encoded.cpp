#include "di/container/string/string_view.h"
#include "di/format/to_string.h"
#include "di/random/distribution/uniform_int_distribution.h"
#include "di/random/prelude.h"
#include "di/serialization/json_deserializer.h"
#include "di/serialization/json_serializer.h"
#include "di/serialization/percent_encoded.h"
#include "di/test/prelude.h"
#include "di/vocab/array/array.h"

namespace serialization_percent_encoded {
struct Case {
    di::TransparentStringView decoded;
    di::StringView encoded;
};

constexpr auto cases = []() {
    return di::Array {
        Case(""_tsv, ""_sv),
        Case("asdf"_tsv, "asdf"_sv),
        Case("xx x"_tsv, "xx%20x"_sv),
        Case("qq\xf1\xd3"_tsv, "qq%F1%D3"_sv),
        Case("a:/?#[]@!$&'()*+,;="_tsv, "a%3A%2F%3F%23%5B%5D%40%21%24%26%27%28%29%2A%2B%2C%3B%3D"_sv),
    };
};

constexpr static void percent_encoded_encode() {
    for (auto const& [input, output] : cases()) {
        auto result = di::to_string(di::PercentEncodedView::from_raw_data(input.span()));
        ASSERT_EQ(result, output);
    }
}

constexpr static void percent_encoded_decode() {
    for (auto const& [output, input] : cases()) {
        auto result = di::parse_unchecked<di::PercentEncoded<>>(input);
        ASSERT_EQ(result.underlying_string(), output);
    }

    auto lower_case_digit_cases = di::Array {
        Case("qq\xf1\xd3\xc1\xb1\xa1\xee"_tsv, "qq%f1%d3%c1%b1%a1%ee"_sv),
    };

    for (auto const& [output, input] : lower_case_digit_cases) {
        auto result = di::parse_unchecked<di::PercentEncoded<>>(input);
        ASSERT_EQ(result.underlying_string(), output);
    }
}

constexpr static void percent_encoded_errors() {
    ASSERT(!di::parse<di::PercentEncoded<>>("%"_sv));
    ASSERT(!di::parse<di::PercentEncoded<>>("%a"_sv));
    ASSERT(!di::parse<di::PercentEncoded<>>("%an"_sv));
    ASSERT(!di::parse<di::PercentEncoded<>>("%;;"_sv));
    ASSERT(!di::parse<di::PercentEncoded<>>("%%20"_sv));
    ASSERT(!di::parse<di::PercentEncoded<>>("abc%20%F"_sv));
}

constexpr static void percent_encoded_json() {
    auto a = di::PercentEncodedView::from_raw_data("qwer !$"_tsv);
    auto v = di::to_json_string(a);
    ASSERT(v);
    ASSERT_EQ(v.value(), "\"qwer%20%21%24\""_sv);

    auto b = di::from_json_string<di::PercentEncoded<>>(v.value());
    ASSERT(b);
    ASSERT_EQ(b.value().underlying_string(), a.underlying_string());
}

constexpr static void percent_encoded_literal() {
    auto percent_encoded = "abc%20"_percent_encoded;
    ASSERT_EQ(percent_encoded.underlying_string(), "abc "_tsv);
    ASSERT_EQ(di::to_string(percent_encoded), "abc%20"_sv);
}

constexpr static void percent_encoded_roundtrip() {
    auto do_test = [](di::UniformRandomBitGenerator auto rng) {
        auto n = di::UniformIntDistribution(0, 100)(rng);
        auto bytes = di::TransparentString {};
        for (auto _ : di::range(n)) {
            bytes.push_back(char(di::UniformIntDistribution(0, 255)(rng)));
        }

        auto percent_encoded = di::PercentEncoded<>::from_raw_data(di::move(bytes));
        auto string = di::to_string(percent_encoded);
        auto result = di::parse_unchecked<di::PercentEncoded<>>(string);
        ASSERT_EQ(percent_encoded, result);
    };

    do_test(di::MinstdRand(1));

    if (!di::is_constant_evaluated()) {
        do_test(di::MinstdRand(2));
        do_test(di::MinstdRand(3));
        do_test(di::MinstdRand(4));
        do_test(di::MinstdRand(5));
        do_test(di::MinstdRand(6));
        do_test(di::MinstdRand(7));
        do_test(di::MinstdRand(8));
        do_test(di::MinstdRand(9));
        do_test(di::MinstdRand(10));
    }
}

TESTC(serialization_percent_encoded, percent_encoded_encode)
TESTC(serialization_percent_encoded, percent_encoded_decode)
TESTC(serialization_percent_encoded, percent_encoded_errors)
TESTC_CLANG(serialization_percent_encoded, percent_encoded_json)
TESTC(serialization_percent_encoded, percent_encoded_literal)
TESTC(serialization_percent_encoded, percent_encoded_roundtrip)
}
