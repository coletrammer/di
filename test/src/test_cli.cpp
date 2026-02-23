#include "di/cli/prelude.h"
#include "di/container/path/path.h"
#include "di/test/prelude.h"
#include "di/vocab/variant/holds_alternative.h"

namespace cli {
struct Args {
    bool enable { false };
    di::PathView input { "test.txt"_pv };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args>("test"_tsv, "Long Description"_sv)
            .option<&Args::enable>('e', "enable"_tsv, "D1"_sv)
            .option<&Args::input>('i', "input"_tsv, "D2"_sv);
    }
};

static void basic() {
    auto parser = di::get_cli_parser<Args>();
    auto help_writer = di::StringWriter<> {};

    {
        auto args = di::Array { "test"_tsv, "-e"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "test.txt"_pv);
    }

    {
        auto args = di::Array { "test"_tsv, "-e"_tsv, "-iinput.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "input.txt"_pv);
    }

    {
        auto args = di::Array { "test"_tsv, "-ei"_tsv, "input.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "input.txt"_pv);
    }

    {
        auto args = di::Array { "test"_tsv, "-e"_tsv, "-i"_tsv, "input.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "input.txt"_pv);
    }

    {
        auto args = di::Array { "test"_tsv, "--enable"_tsv, "--input"_tsv, "input.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "input.txt"_pv);
    }

    {
        auto args = di::Array { "test"_tsv, "--enable"_tsv, "--input=input.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "input.txt"_pv);
    }
}

struct Args2 {
    bool enable { false };
    di::PathView input { "test.txt"_pv };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args2>("test"_tsv, "Long Description"_sv)
            .option<&Args2::enable>('e', "enable"_tsv, "D1"_sv)
            .argument<&Args2::input>("FILE"_sv, "Input file"_sv);
    }
};

static void arguments() {
    auto parser = di::get_cli_parser<Args2>();
    auto help_writer = di::StringWriter<> {};

    {
        auto args = di::Array { "test"_tsv, "-e"_tsv, "input.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.input, "input.txt"_pv);
    }
}

struct Args3 {
    bool enable { false };
    di::Vector<di::PathView> inputs;

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args3>("test"_tsv, "Long Description"_sv)
            .option<&Args3::enable>('e', "enable"_tsv, "D1"_sv)
            .argument<&Args3::inputs>("FILES"_sv, "Input files"_sv);
    }
};

static void variadic_arguments() {
    auto parser = di::get_cli_parser<Args3>();
    auto help_writer = di::StringWriter<> {};

    {
        auto args = di::Array { "test"_tsv, "-e"_tsv, "input1.txt"_tsv, "input2.txt"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.enable);
        ASSERT_EQ(result.inputs.size(), 2);
        ASSERT_EQ(result.inputs[0], "input1.txt"_pv);
        ASSERT_EQ(result.inputs[1], "input2.txt"_pv);
    }
}

struct Sub1 {
    bool flag { false };

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Sub1>("sub1"_tsv, "Subcommand 1"_sv).option<&Sub1::flag>('f', "flag"_tsv, "Yeah"_sv);
    }
};

struct Sub2 {
    di::PathView path;

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Sub2>("sub2"_tsv, "Subcommand 2"_sv).option<&Sub2::path>('p', "path"_tsv, "path"_sv);
    }
};

struct Args4 {
    bool config { false };
    di::Variant<di::Void, Sub1, Sub2> command;

    constexpr static auto get_cli_parser() {
        return di::cli_parser<Args4>("args4"_tsv, "With subcommands 2"_sv)
            .option<&Args4::config>('c', "config"_tsv, "configuration"_sv)
            .subcommands<&Args4::command>();
    }
};

static void subcommands() {
    auto parser = di::get_cli_parser<Args4>();
    auto help_writer = di::StringWriter<> {};

    {
        auto args = di::Array { "args4"_tsv, "-c"_tsv, "sub1"_tsv, "-f"_tsv };
        auto result = *parser.parse(args, help_writer);
        ASSERT(result.config);
        ASSERT(di::holds_alternative<Sub1>(result.command));
        ASSERT_EQ(di::get<Sub1>(result.command).flag, true);
    }
}

static void help() {
    auto h1 = di::get_cli_parser<Args>().help_string();
    ASSERT_EQ(h1, R"~(NAME:
  test: Long Description

USAGE:
  test [OPTIONS]

OPTIONS:
  -e, --enable: D1
  -i, --input <VALUE>: D2
)~"_sv);

    auto h2 = di::get_cli_parser<Args2>().help_string();
    ASSERT_EQ(h2, R"~(NAME:
  test: Long Description

USAGE:
  test [OPTIONS] [FILE]

ARGUMENTS:
  [FILE]: Input file

OPTIONS:
  -e, --enable: D1
)~"_sv);

    auto h3 = di::get_cli_parser<Args3>().help_string();
    ASSERT_EQ(h3, R"~(NAME:
  test: Long Description

USAGE:
  test [OPTIONS] [FILES...]

ARGUMENTS:
  [FILES...]: Input files

OPTIONS:
  -e, --enable: D1
)~"_sv);

    auto h4 = di::get_cli_parser<Args4>().help_string();
    ASSERT_EQ(h4, R"~(NAME:
  args4: With subcommands 2

USAGE:
  args4 [OPTIONS] COMMAND

COMMANDS:
  sub1: Subcommand 1
  sub2: Subcommand 2

OPTIONS:
  -c, --config: configuration
)~"_sv);
}

TEST(cli, basic);
TEST(cli, arguments)
TEST(cli, variadic_arguments)
TEST(cli, subcommands)
TEST(cli, help)
}
