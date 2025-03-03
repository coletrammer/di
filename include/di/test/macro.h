#pragma once

#include "di/platform/compiler.h"
#include "di/test/test_manager.h"

#define DI_TEST(suite_name, case_name)                                                                  \
    static void suite_name##_##case_name();                                                             \
    [[gnu::constructor]] static void register_##suite_name##_##case_name() {                            \
        di::test::TestManager::the().register_test_case(                                                \
            di::test::TestCase("" #suite_name ""_tsv, "" #case_name ""_tsv, suite_name##_##case_name)); \
    }                                                                                                   \
    static void suite_name##_##case_name() {                                                            \
        case_name();                                                                                    \
    }

#define DI_TESTC(suite_name, case_name)                                                                 \
    static void suite_name##_##case_name();                                                             \
    [[gnu::constructor]] static void register_##suite_name##_##case_name() {                            \
        di::test::TestManager::the().register_test_case(                                                \
            di::test::TestCase("" #suite_name ""_tsv, "" #case_name ""_tsv, suite_name##_##case_name)); \
    }                                                                                                   \
    static void suite_name##_##case_name() {                                                            \
        [[maybe_unused]] constexpr int exec = [] {                                                      \
            case_name();                                                                                \
            return 0;                                                                                   \
        }();                                                                                            \
        case_name();                                                                                    \
    }

#ifdef DI_CLANG
#define DI_TESTC_CLANG     DI_TESTC
#define DI_TESTC_GCC       DI_TEST
#define DI_TESTC_GCC_NOSAN DI_TESTC
#else
#define DI_TESTC_CLANG DI_TEST
#define DI_TESTC_GCC   DI_TESTC
#ifdef DI_SANITIZER
#define DI_TESTC_GCC_NOSAN DI_TEST
#else
#define DI_TESTC_GCC_NOSAN DI_TESTC
#endif
#endif

#define TEST            DI_TEST
#define TESTC           DI_TESTC
#define TESTC_CLANG     DI_TESTC_CLANG
#define TESTC_GCC       DI_TESTC_GCC
#define TESTC_GCC_NOSAN DI_TESTC_GCC_NOSAN
