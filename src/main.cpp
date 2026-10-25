#include "App.hpp"
#include "Bus.hpp"
#include "Options.hpp"

#include <cstdlib>
#include <print>
#include <span>

int main(int argc, char** argv) {
    const std::span<char* const> args{argv, static_cast<std::size_t>(argc)};
    const auto options = parseOptions(args);
    if (!options) {
        std::println(stderr, "dbus-viewer: {}\n\n{}", options.error(), usage(args[0]));
        return 2;
    }
    if (options->showHelp) {
        std::print("{}", usage(args[0]));
        return 0;
    }

    // sd_bus_message_dump() would otherwise emit ANSI colors into the details buffer.
    setenv("SYSTEMD_COLORS", "0", 1);

    const auto address = resolveBusAddress(*options);
    if (address.empty()) {
        std::println(stderr, "dbus-viewer: cannot determine the session bus address "
                             "(DBUS_SESSION_BUS_ADDRESS and XDG_RUNTIME_DIR are unset)");
        return 1;
    }

    // The client connection is opened first so that its traffic can be told apart.
    auto client = openClientBus(address);
    if (!client) {
        std::println(stderr, "dbus-viewer: {}", client.error());
        return 1;
    }
    auto monitor = openMonitorBus(address, options->matchRules);
    if (!monitor) {
        std::println(stderr, "dbus-viewer: {}", monitor.error());
        return 1;
    }

    App app{*options, std::move(*client), std::move(*monitor)};
    const int code = app.run();
    if (!app.fatalError().empty()) std::println(stderr, "dbus-viewer: {}", app.fatalError());
    return code;
}
