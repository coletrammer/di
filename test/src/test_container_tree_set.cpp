#include "di/container/algorithm/prelude.h"
#include "di/container/interface/erase.h"
#include "di/container/tree/prelude.h"
#include "di/container/view/prelude.h"
#include "di/math/prelude.h"
#include "di/random/prelude.h"
#include "di/test/prelude.h"

namespace container_tree_set {
constexpr static void basic() {
    di::TreeSet<int> x;
    x.clear();
    ASSERT_EQ(di::distance(x), 0);

    x.insert_container(di::Array { 1, 2, 5, 0, 4, -6, 6, 3 });

    auto [it, did_insert] = x.insert(3);
    ASSERT_EQ(*it, 3);
    ASSERT(!did_insert);

    ASSERT_EQ(di::distance(x), 8);
    ASSERT_EQ(di::sum(x), 15);

    ASSERT(di::is_sorted(x));

    auto y = di::TreeSet<int> {};
    y.insert(1);
    y.insert(2);
    y.insert(3);
    y.insert(4);

    ASSERT(y.contains(4));
}

constexpr static void accessors() {
    auto x = di::range(1, 6) | di::to<di::TreeSet>(di::compare);

    auto const& y = x;
    ASSERT_EQ(*y.lower_bound(3), 3);
    ASSERT_EQ(*y.upper_bound(3), 4);
    ASSERT_EQ(*y.find(3), 3);
    ASSERT_EQ(*y.at(3), 3);
    ASSERT_EQ(y.at(6), di::nullopt);
    ASSERT_EQ(*y.front(), 1);
    ASSERT_EQ(*y.back(), 5);
    ASSERT_EQ(y.count(3), 1U);
    ASSERT(y.lower_bound(10) == y.end());
    ASSERT(y.upper_bound(10) == y.end());
    ASSERT_EQ(y.count(10), 0U);
}

constexpr static void erase() {
    auto x = di::to<di::TreeSet>(di::range(1, 6));

    ASSERT_EQ(*x.erase(x.find(2)), 3);
    ASSERT_EQ(*x.erase(x.find(3), x.find(5)), 5);
    ASSERT_EQ(x.erase(1), 1U);
    ASSERT_EQ(x.erase(6), 0U);

    auto y = di::to<di::TreeSet>(di::range(1, 10));
    ASSERT_EQ(di::erase_if(y,
                           [](auto x) {
                               return x % 2 == 0;
                           }),
              4U);
    ASSERT_EQ(di::distance(y), 5);
}

constexpr static void property() {
    auto do_test = [](di::UniformRandomBitGenerator auto rng) {
        auto x = di::TreeMultiSet<unsigned int> {};

        auto distribution = di::UniformIntDistribution<> { 1, 1000 };

        auto iterations = di::is_constant_evaluated() ? 99 : 345;
        for (auto i : di::range(iterations)) {
            auto new_value = distribution(rng);
            x.insert(new_value);
            ASSERT(di::is_sorted(x));
            ASSERT(di::is_sorted(di::reverse(x), di::compare_backwards));
            ASSERT_EQ(di::distance(x.begin(), x.end()), i + 1);
            ASSERT_EQ(di::distance(di::reverse(x).begin(), di::reverse(x).end()), i + 1);
            ASSERT_EQ(di::size(x), di::to_unsigned(i) + 1);
        }
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

constexpr static void property2() {
    auto do_test = [](di::UniformRandomBitGenerator auto rng) {
        auto x = di::TreeSet<unsigned int> {};

        auto iterations = di::is_constant_evaluated() ? 99 : 345;
        auto vector = di::range(0, iterations) | di::to<di::Vector>();

        rng.discard(1);
        di::shuffle(vector, rng);

        for (auto [i, new_value] : di::enumerate(vector)) {
            x.insert(new_value);
            ASSERT(di::is_sorted(x));
            ASSERT(di::is_sorted(di::reverse(x), di::compare_backwards));
            ASSERT_EQ(di::to_unsigned(di::distance(x.begin(), x.end())), i + 1);
            ASSERT_EQ(di::to_unsigned(di::distance(di::reverse(x).begin(), di::reverse(x).end())), i + 1);
            ASSERT_EQ(di::size(x), di::to_unsigned(i) + 1);
        }
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

constexpr static void property3() {
    auto do_test = [](di::UniformRandomBitGenerator auto rng) {
        auto x = di::TreeSet<unsigned int> {};

        auto iterations = di::is_constant_evaluated() ? 99 : 345;
        auto vector = di::range(0, iterations) | di::to<di::Vector>();

        for (auto i : vector) {
            x.insert(i);
        }
        ASSERT_EQ(di::size(x), di::to_unsigned(iterations));

        rng.discard(1);
        di::shuffle(vector, rng);

        for (auto [i, value] : di::enumerate(vector)) {
            x.erase(value);
            x.insert(value);
            ASSERT(di::is_sorted(x));
            ASSERT(di::is_sorted(di::reverse(x), di::compare_backwards));
            ASSERT_EQ(di::size(x), di::to_unsigned(iterations));
        }
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

TESTC(container_tree_set, basic)
TESTC(container_tree_set, accessors)
TESTC(container_tree_set, erase)
TESTC(container_tree_set, property)
TESTC(container_tree_set, property2)
TESTC(container_tree_set, property3)
}
