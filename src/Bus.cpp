#include "Bus.hpp"

#include <cstdlib>
#include <cstring>
#include <format>

namespace {

std::string errnoMessage(std::string_view step, int result) {
    return std::format("{}: {}", step, std::strerror(-result));
}

std::expected<BusPtr, std::string> newBus(const std::string& address, bool monitor) {
    sd_bus* raw = nullptr;
    if (const int r = sd_bus_new(&raw); r < 0)
        return std::unexpected(errnoMessage("cannot allocate bus", r));
    BusPtr bus{raw};

    if (const int r = sd_bus_set_address(bus.get(), address.c_str()); r < 0)
        return std::unexpected(errnoMessage(std::format("invalid bus address '{}'", address), r));

    sd_bus_set_bus_client(bus.get(), 1);
    if (monitor) {
        sd_bus_set_monitor(bus.get(), 1);
        // Without fd passing support, the daemon drops messages carrying file descriptors.
        sd_bus_negotiate_fds(bus.get(), 1);
    }

    if (const int r = sd_bus_start(bus.get()); r < 0)
        return std::unexpected(errnoMessage(std::format("cannot connect to '{}'", address), r));
    return bus;
}

} // namespace

std::string resolveBusAddress(const Options& options) {
    switch (options.bus) {
    case BusKind::address: return options.address;
    case BusKind::system:
        if (const char* env = std::getenv("DBUS_SYSTEM_BUS_ADDRESS"); env && *env) return env;
        return "unix:path=/run/dbus/system_bus_socket";
    case BusKind::session:
        if (const char* env = std::getenv("DBUS_SESSION_BUS_ADDRESS"); env && *env) return env;
        if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime && *runtime)
            return std::format("unix:path={}/bus", runtime);
        return {};
    }
    return {};
}

std::expected<BusPtr, std::string> openClientBus(const std::string& address) {
    return newBus(address, false);
}

std::expected<BusPtr, std::string> openMonitorBus(const std::string& address,
                                                  const std::vector<std::string>& matchRules) {
    auto bus = newBus(address, true);
    if (!bus) return bus;

    sd_bus_message* raw = nullptr;
    if (const int r = sd_bus_message_new_method_call(bus->get(), &raw, busDaemonName,
                                                     "/org/freedesktop/DBus",
                                                     "org.freedesktop.DBus.Monitoring",
                                                     "BecomeMonitor");
        r < 0)
        return std::unexpected(errnoMessage("cannot build BecomeMonitor call", r));
    MessagePtr call{raw};

    // An empty rule list means every message on the bus.
    int r = sd_bus_message_open_container(call.get(), 'a', "s");
    for (const auto& rule : matchRules)
        if (r >= 0) r = sd_bus_message_append_basic(call.get(), 's', rule.c_str());
    if (r >= 0) r = sd_bus_message_close_container(call.get());
    if (r >= 0) r = sd_bus_message_append(call.get(), "u", 0u);
    if (r < 0) return std::unexpected(errnoMessage("cannot build BecomeMonitor call", r));

    sd_bus_error error{};
    sd_bus_message* reply = nullptr;
    r = sd_bus_call(bus->get(), call.get(), 0, &error, &reply);
    MessagePtr replyGuard{reply};
    if (r < 0) {
        auto message = std::format("BecomeMonitor failed: {}",
                                   error.message ? error.message : std::strerror(-r));
        if (sd_bus_error_has_name(&error, SD_BUS_ERROR_ACCESS_DENIED))
            message += "\n(monitoring the system bus usually requires root: try with sudo)";
        sd_bus_error_free(&error);
        return std::unexpected(message);
    }
    return bus;
}

std::string safeString(const char* text) {
    return text ? std::string{text} : std::string{};
}
