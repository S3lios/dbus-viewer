#include "Options.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

std::expected<Options, std::string> parse(std::vector<std::string> args) {
    args.insert(args.begin(), "dbus-viewer");
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(arg.data());
    return parseOptions(argv);
}

} // namespace

TEST_CASE("defaults to the session bus") {
    const auto options = parse({});
    REQUIRE(options);
    CHECK(options->bus == BusKind::session);
    CHECK(options->matchRules.empty());
    CHECK(options->maxMessages == 20'000);
}

TEST_CASE("selects the bus") {
    CHECK(parse({"--system"})->bus == BusKind::system);
    CHECK(parse({"-s"})->bus == BusKind::system);
    CHECK(parse({"--system", "--session"})->bus == BusKind::session);

    const auto custom = parse({"--address", "unix:path=/tmp/bus"});
    REQUIRE(custom);
    CHECK(custom->bus == BusKind::address);
    CHECK(custom->address == "unix:path=/tmp/bus");
    CHECK(busLabel(*custom) == "unix:path=/tmp/bus");
}

TEST_CASE("collects repeated match rules") {
    const auto options = parse({"--match", "type='signal'", "-m", "member='Ping'"});
    REQUIRE(options);
    CHECK(options->matchRules == std::vector<std::string>{"type='signal'", "member='Ping'"});
}

TEST_CASE("parses the history size") {
    CHECK(parse({"-n", "500"})->maxMessages == 500);
    CHECK_FALSE(parse({"-n", "0"}));
    CHECK_FALSE(parse({"-n", "12x"}));
    CHECK_FALSE(parse({"--max-messages", "-3"}));
}

TEST_CASE("reports invalid command lines") {
    CHECK(parse({"--bogus"}).error() == "unknown option: --bogus");
    CHECK(parse({"--match"}).error() == "missing value for --match");
}

TEST_CASE("recognizes help and version flags") {
    CHECK(parse({"-h"})->showHelp);
    CHECK(parse({"--version"})->showVersion);
}
