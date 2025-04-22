#include "di/io/vector_reader.h"
#include "di/io/vector_writer.h"
#include "di/reflect/prelude.h"
#include "di/serialization/binary_deserializer.h"
#include "di/serialization/binary_serializer.h"
#include "di/serialization/json_deserializer.h"
#include "di/serialization/json_value.h"
#include "di/test/prelude.h"
#include "di/util/uuid.h"

namespace deserialization {
constexpr static void json_value() {
    auto x = di::json::Value {};
    ASSERT(x.is_null());
    auto r1 = di::visit(di::overload(
                            [](di::json::Null) {
                                return 1;
                            },
                            [](auto&&) {
                                return 0;
                            }),
                        di::as_const(x));
    ASSERT_EQ(r1, 1);

    x["hello"_sv] = 42;
    ASSERT(x.is_object());
    ASSERT_EQ(x.at("hello"_sv), 42);

    x.push_back(42);
    ASSERT(x.is_array());
    ASSERT_EQ(x.at(0), 42);

    ASSERT_LT(x[0], 43);
    ASSERT_EQ(x.size(), 1);
    ASSERT(!x.empty());

    x.insert_or_assign("hello"_sv, 43);
    ASSERT(x["hello"_sv].is_number());
    ASSERT_EQ(x.at("hello"_sv), 43);

    x.try_emplace("world"_sv, 44);
    ASSERT_EQ(x.at("world"_sv), 44);

    x["world"_sv] = di::create<di::json::Value>("value"_sv);
    x["world"_sv] = "value"_sv.to_owned();
    ASSERT_EQ(*x.at("world"_sv), "value"_sv);

    ASSERT_EQ(x.size(), 2);
    ASSERT(!x.empty());

    x.clear();
    ASSERT(x.is_object());
    ASSERT(x.empty());

    x["hello"_sv] = 42;
    x["world"_sv] = 43;
    ASSERT_EQ(di::to_string(x), R"({
    "hello": 42,
    "world": 43
})"_sv);
}

constexpr static void json_escaped_string() {
    struct Case {
        di::StringView input;
        di::StringView expected;
    };

    auto tests = di::Array {
        Case(R"("\"\\\/\b\f\n\r\t\u0000\u12Ef")"_sv, u8"\"\\/\b\f\n\r\t\0\u12EF"_sv),
        Case(R"("abc\ufFa9c")"_sv, u8"abc\uFFA9c"_sv),
        Case(R"("a\uD834\uDD1Eb")"_sv, u8"a𝄞b"_sv),
    };

    for (auto [input, expected] : tests) {
        auto result = di::from_json_string<di::String>(input);
        ASSERT(result);
        ASSERT_EQ(*result, expected);
    }

    struct NegativeCase {
        di::StringView input;
    };

    auto negative_tests = di::Array {
        // Non-sense one letter escape
        NegativeCase(R"("\c")"_sv),
        // Non-terminating esacpe
        NegativeCase(R"("\c")"_sv),
        NegativeCase(R"("\")"_sv),
        NegativeCase(R"("\u")"_sv),
        NegativeCase(R"("\ua")"_sv),
        NegativeCase(R"("\uaa")"_sv),
        NegativeCase(R"("\uaaa")"_sv),
        // Invalid hex digits
        NegativeCase(R"("\ua"aa")"_sv),
        NegativeCase(R"("\ua,aa")"_sv),
        NegativeCase(R"("\ua\aa")"_sv),
        NegativeCase(R"("\uuaaa")"_sv),
        // Unparied high surrogate
        NegativeCase(R"("\uD801")"_sv),
        NegativeCase(R"("\uD801\n")"_sv),
        NegativeCase(R"("\uD801a")"_sv),
        NegativeCase(R"("\uD801\uD801")"_sv),
        // Unpaired low surrogate
        NegativeCase(R"("\uDC01a")"_sv),
    };

    for (auto [input] : negative_tests) {
        auto result = di::from_json_string<di::String>(input);
        ASSERT(!result);
    }
}

constexpr static void json_literal() {
    auto object = R"( {
    "hello" : 32 , "world" : [ "x" , null ]
} )"_json;

    ASSERT_EQ(object["hello"_sv], 32);
    ASSERT_EQ(object["world"_sv][0], "x"_sv);
    ASSERT_EQ(object["world"_sv][1], di::json::null);

    {
        auto r = *di::from_json_string<di::json::Array>(R"([1, 2, 3])"_sv);
        ASSERT_EQ(r[0], 1);
        ASSERT_EQ(r[1], 2);
        ASSERT_EQ(r[2], 3);
    }
    {
        auto r = *di::from_json_string<di::json::Object>(R"({"hello": 42, "world": 43})"_sv);
        ASSERT_EQ(r["hello"_sv], 42);
        ASSERT_EQ(r["world"_sv], 43);
    }
}

struct MyType {
    int x;
    int y;
    int z;
    bool w;
    di::String a;

    auto operator==(MyType const& other) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<MyType>) {
        return di::make_fields<"MyType">(di::field<"x", &MyType::x>, di::field<"y", &MyType::y>,
                                         di::field<"z", &MyType::z>, di::field<"w", &MyType::w>,
                                         di::field<"a", &MyType::a>);
    }
};

enum class MyEnum { Foo, Bar, Baz };

constexpr static auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<MyEnum>) {
    using enum MyEnum;
    return di::make_enumerators<"MyEnum">(di::enumerator<"Foo", Foo>, di::enumerator<"Bar", Bar>,
                                          di::enumerator<"Baz", Baz>);
}

struct MySuperType {
    MyType my_type;
    di::Vector<int> array;
    di::TreeMap<di::String, int> map;
    MyEnum my_enum;
    di::Optional<int> optional;
    di::Tuple<i32, di::String> tuple;
    di::Variant<MyEnum, MyType> variant;
    di::Box<i32> box;
    di::UUID uuid;

    auto operator==(MySuperType const&) const -> bool = default;

    constexpr friend auto tag_invoke(di::Tag<di::reflect>, di::InPlaceType<MySuperType>) {
        return di::make_fields<"MySuperType">(
            di::field<"my_type", &MySuperType::my_type>, di::field<"array", &MySuperType::array>,
            di::field<"map", &MySuperType::map>, di::field<"my_enum", &MySuperType::my_enum>,
            di::field<"optional", &MySuperType::optional>, di::field<"tuple", &MySuperType::tuple>,
            di::field<"variant", &MySuperType::variant>, di::field<"box", &MySuperType::box>,
            di::field<"uuid", &MySuperType::uuid>);
    }
};

constexpr static void json_reflect() {
    {
        auto r = *di::from_json_string<MyEnum>(R"("Foo")"_sv);
        auto e = MyEnum::Foo;
        ASSERT_EQ(r, e);
    }
    {
        auto r = *di::from_json_string<MyEnum>(R"("Bar")"_sv);
        auto e = MyEnum::Bar;
        ASSERT_EQ(r, e);
    }
    {
        auto r = *di::from_json_string<MyEnum>(R"("Baz")"_sv);
        auto e = MyEnum::Baz;
        ASSERT_EQ(r, e);
    }
    {
        auto r = *di::from_json_string<MyType>(R"({
        "x": 1,
        "y": 2,
        "z": 3,
        "w": true,
        "a": "hello"
    })"_sv);

        auto e = MyType { 1, 2, 3, true, "hello"_sv.to_owned() };
        ASSERT_EQ(r, e);
    }
    {
        auto r = *di::from_json_string<MySuperType>(R"({
        "my_type": {
            "x": 1,
            "y": 2,
            "z": 3,
            "w": true,
            "a": "hello"
        },
        "array": [1, 2, 3],
        "map": {
            "hello": 1,
            "world": 2
        },
        "my_enum": "Bar",
        "optional": 4,
        "tuple": [
            5,
            "y"
        ],
        "variant": {
            "MyEnum": "Foo"
        },
        "box": null,
        "uuid": "dfa22e02-ec78-4000-9257-c0b7d69264dd"
    })"_sv);

        auto e = MySuperType {
            MyType { 1, 2, 3, true, "hello"_sv.to_owned() },
            di::Array { 1, 2, 3 } | di::to<di::Vector>(),
            di::Array { di::Tuple { "hello"_sv.to_owned(), 1 }, di::Tuple { "world"_sv.to_owned(), 2 } } |
                di::as_rvalue | di::to<di::TreeMap>(),
            MyEnum::Bar,
            4,
            di::Tuple { 5, "y"_s },
            MyEnum::Foo,
            nullptr,
            "dfa22e02-ec78-4000-9257-c0b7d69264dd"_uuid,
        };

        ASSERT_EQ(r, e);
    }

    {
        auto r = *di::from_json_string<di::Box<int>>("4"_sv);
        ASSERT(r);
        ASSERT_EQ(*r, 4);
    }
}

constexpr static void binary() {
    auto do_test = []<di::concepts::EqualityComparable T>(T const& value) {
        auto writer = di::VectorWriter<>();
        ASSERT(di::serialize_binary(writer, value));
        auto reader = di::VectorReader(di::move(writer).vector());
        auto result = di::deserialize_binary<T>(reader);
        ASSERT(result);
        ASSERT_EQ(*result, value);
    };

    do_test(42);
    do_test(false);
    do_test("hello"_s);
    do_test(di::make_tuple(1, 2, 3));

    auto variant = di::Variant<int, di::String> { 42 };
    do_test(variant);

    variant = "hello"_s;
    do_test(variant);

    auto mytype = MyType { 1, 2, 3, true, "hello"_s };
    do_test(mytype);
}

TESTC(deserialization, json_value)
TEST(deserialization, json_escaped_string)
TESTC_CLANG(deserialization, json_literal)
TESTC_CLANG(deserialization, json_reflect)
TESTC_CLANG(deserialization, binary)
}
