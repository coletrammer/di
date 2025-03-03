#include <cstdlib>
#include <ostream>

#include "di/io/prelude.h"
#include "di/test/test_manager.h"
#include "di/vocab/error/prelude.h"

namespace {
struct StdWriter {
    auto write_some(di::Span<byte const> data) -> di::Result<usize> {
        iostream->write((char*) data.data(), (isize) data.size());
        if (!*iostream) {
            return di::Unexpected(di::BasicError::InvalidArgument);
        }
        return data.size();
    }

    auto flush() -> di::Result<> {
        iostream->flush();
        if (!*iostream) {
            return di::Unexpected(di::BasicError::InvalidArgument);
        }
        return {};
    }

    auto interactive_device() -> bool { return true; }

    std::ostream* iostream { nullptr };
};
}

auto main(int argc, char** argv) -> int {
    auto args = di::Vector<di::TransparentStringView> {};
    for (int i = 0; i < argc; i++) {
        char* arg = argv[i];
        size_t len = 0;
        while (arg[len] != '\0') {
            len++;
        }
        args.push_back({ arg, len });
    }

    auto writer = StdWriter(&std::cerr);

    auto as_span = args.span();
    auto parser = di::get_cli_parser<di::test::TestManager::Args>();
    auto result = parser.parse(as_span);
    if (!result) {
        di::writer_println<di::StringView::Encoding>(writer, "Failed to parse command line arguments."_sv);
        di::writer_println<di::StringView::Encoding>(writer, "Run with '--help' for all list of valid options."_sv);
        return 2;
    }

    if (result.value().help) {
        parser.write_help(writer);
        return 0;
    }

    auto& manager = di::test::TestManager::the();

    manager.set_did_finish([](i32 status) {
        std::exit(status);
    });

    manager.set_writer(writer);

    auto res = manager.run_tests(result.value());
    if (!res) {
        return 1;
    }

    return 0;
}
