#include "Options.hpp"

#include <charconv>
#include <format>

namespace {

std::expected<std::size_t, std::string> parseCount(std::string_view text) {
  std::size_t value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value == 0)
    return std::unexpected(std::format("invalid message count: '{}'", text));
  return value;
}

} // namespace

std::expected<Options, std::string> parseOptions(std::span<char *const> args) {
  Options options;
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string_view arg = args[i];
    const auto nextValue =
        [&]() -> std::expected<std::string_view, std::string> {
      if (i + 1 >= args.size())
        return std::unexpected(std::format("missing value for {}", arg));
      return std::string_view{args[++i]};
    };

    if (arg == "-h" || arg == "--help") {
      options.showHelp = true;
    } else if (arg == "--system" || arg == "-s") {
      options.bus = BusKind::system;
    } else if (arg == "--session" || arg == "--user") {
      options.bus = BusKind::session;
    } else if (arg == "--address") {
      const auto value = nextValue();
      if (!value)
        return std::unexpected(value.error());
      options.bus = BusKind::address;
      options.address = *value;
    } else if (arg == "-m" || arg == "--match") {
      const auto value = nextValue();
      if (!value)
        return std::unexpected(value.error());
      options.matchRules.emplace_back(*value);
    } else if (arg == "-n" || arg == "--max-messages") {
      const auto value = nextValue();
      if (!value)
        return std::unexpected(value.error());
      const auto count = parseCount(*value);
      if (!count)
        return std::unexpected(count.error());
      options.maxMessages = *count;
    } else {
      return std::unexpected(std::format("unknown option: {}", arg));
    }
  }
  return options;
}

std::string usage(std::string_view programName) {
  return std::format(
      "Usage: {} [OPTIONS]\n"
      "\n"
      "Interactive terminal monitor for D-Bus messages.\n"
      "\n"
      "Options:\n"
      "  --session             Monitor the session (user) bus (default)\n"
      "  -s, --system          Monitor the system bus (usually requires "
      "root)\n"
      "  --address ADDRESS     Monitor the bus at a custom D-Bus address\n"
      "  -m, --match RULE      Only monitor messages matching a D-Bus match "
      "rule;\n"
      "                        repeatable, a message matching any rule is "
      "shown.\n"
      "                        Example: --match "
      "\"type='signal',interface='org.foo.Bar'\"\n"
      "  -n, --max-messages N  Number of messages kept in history (default "
      "20000)\n"
      "  -h, --help            Show this help\n",
      programName);
}

std::string busLabel(const Options &options) {
  switch (options.bus) {
  case BusKind::session:
    return "session";
  case BusKind::system:
    return "system";
  case BusKind::address:
    return options.address;
  }
  return {};
}
