#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

enum class BusKind { session, system, address };

struct Options {
    BusKind bus = BusKind::session;
    std::string address;
    // D-Bus match rules handed to the daemon; empty means every message.
    std::vector<std::string> matchRules;
    std::size_t maxMessages = 20'000;
    bool showHelp = false;
};

std::expected<Options, std::string> parseOptions(std::span<char* const> args);
std::string usage(std::string_view programName);
std::string busLabel(const Options& options);
