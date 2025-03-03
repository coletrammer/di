#pragma once

#include "di/any/container/any.h"
#include "di/cli/prelude.h"
#include "di/function/container/function.h"
#include "di/io/interface/writer.h"
#include "di/io/writer_println.h"
#include "di/test/test_case.h"

namespace di::test {
class TestManager {
public:
    static auto the() -> TestManager& {
        static TestManager s_the;
        return s_the;
    }

    void register_test_case(TestCase test_case) { m_test_cases.push_back(di::move(test_case)); }

    void set_writer(di::Any<di::Writer> writer) { m_writer = di::move(writer); }

    void set_did_finish(di::Function<void(i32)> did_finish) { m_did_finish = di::move(did_finish); }

    struct Args {
        bool list_simple { false };
        di::Optional<di::TransparentStringView> suite_name;
        di::Optional<di::TransparentStringView> case_name;
        bool help { false };

        constexpr static auto get_cli_parser() {
            return di::cli_parser<Args>("dius_test"_sv, "Dius Test Runner"_sv)
                .help()
                .option<&Args::list_simple>('L', "list-simple"_tsv,
                                            "Output a simple machine readable list of test cases"_sv)
                .option<&Args::suite_name>('s', "suite"_tsv, "Specifc test suite to run"_sv)
                .option<&Args::case_name>('t', "test-case"_tsv, "Specific case to run in the format ([suite:]case)"_sv);
        }
    };

    auto run_tests(Args& args) -> di::Result<void> {
        auto [list_simple, suite_name, case_name, _] = args;

        auto [first_to_remove, last_to_remove] = di::container::remove_if(m_test_cases, [&](auto&& test_case) {
            if (suite_name && *suite_name != test_case.suite_name()) {
                return true;
            }
            if (case_name) {
                auto [colon_it, colon_it_end] = case_name->find(':');
                if (colon_it != colon_it_end) {
                    return test_case.suite_name() != case_name->substr(case_name->begin(), colon_it) ||
                           test_case.case_name() != case_name->substr(colon_it_end);
                }
                return test_case.case_name() != *case_name;
            }
            return false;
        });
        m_test_cases.erase(first_to_remove, last_to_remove);

        if (m_test_cases.empty() && (suite_name || case_name)) {
            di::writer_println<di::StringView::Encoding>(
                m_writer, "No test cases match filter: [suite={}] [case={}]"_sv, suite_name, case_name);
            return di::Unexpected(di::BasicError::InvalidArgument);
        }

        if (list_simple) {
            for (auto& test_case : m_test_cases) {
                di::writer_println<di::StringView::Encoding>(m_writer, "{}:{}"_sv, test_case.suite_name(),
                                                             test_case.case_name());
            }
            return {};
        }

        execute_remaining_tests();

        return {};
    }

    auto is_test_application() const -> bool { return !m_test_cases.empty(); }
    void handle_assertion_failure() {
        print_failure_message();
        ++m_current_test_index;
        final_report();
    }

private:
    TestManager() = default;

    void print_failure_message() {
        auto& test_case = m_test_cases[m_current_test_index];
        di::writer_println<di::StringView::Encoding>(
            m_writer, "{}: {}: {}"_sv, di::Styled("FAIL"_sv, di::FormatEffect::Bold | di::FormatColor::Red),
            di::Styled(test_case.suite_name(), di::FormatEffect::Bold), test_case.case_name());
        ++m_fail_count;
    }

    void print_success_message() {
        auto& test_case = m_test_cases[m_current_test_index];

        di::writer_println<di::StringView::Encoding>(
            m_writer, "{}: {}: {}"_sv, di::Styled("PASS"_sv, di::FormatEffect::Bold | di::FormatColor::Green),
            di::Styled(test_case.suite_name(), di::FormatEffect::Bold), test_case.case_name());
        ++m_success_count;
    }

    void run_current_test() {
        auto& test_case = m_test_cases[m_current_test_index];
        test_case.execute();
        print_success_message();
    }

    void execute_remaining_tests() {
        while (m_current_test_index < m_test_cases.size()) {
            run_current_test();
            m_current_test_index++;
        }
        final_report();
    }

    void final_report() {
        auto tests_skipped = m_test_cases.size() - m_current_test_index;

        di::writer_print<di::StringView::Encoding>(m_writer, "\n{} / {} Test Passed"_sv,
                                                   di::Styled(m_success_count, di::FormatEffect::Bold),
                                                   di::Styled(m_test_cases.size(), di::FormatEffect::Bold));
        if (tests_skipped) {
            di::writer_print<di::StringView::Encoding>(m_writer, " ({} Failed {} Skipped)"_sv,
                                                       di::Styled(m_fail_count, di::FormatEffect::Bold),
                                                       di::Styled(tests_skipped, di::FormatEffect::Bold));
        }
        di::writer_println<di::StringView::Encoding>(
            m_writer, ": {}"_sv,
            m_fail_count ? di::Styled("Tests Failed"_sv, di::FormatEffect::Bold | di::FormatColor::Red)
                         : di::Styled("Tests Passed"_sv, di::FormatEffect::Bold | di::FormatColor::Green));

        auto result = int(m_fail_count > 0);
        DI_ASSERT(m_did_finish);
        m_did_finish(result);
    }

    di::Any<di::Writer> m_writer;
    di::Function<void(int)> m_did_finish;
    di::Vector<TestCase> m_test_cases;
    usize m_current_test_index { 0 };
    usize m_fail_count { 0 };
    usize m_success_count { 0 };
};
}
