#pragma once

#include "Options.hpp"

#include <systemd/sd-bus.h>

#include <expected>
#include <memory>
#include <string>
#include <vector>

struct BusDeleter {
    void operator()(sd_bus* bus) const { sd_bus_flush_close_unref(bus); }
};
struct MessageDeleter {
    void operator()(sd_bus_message* message) const { sd_bus_message_unref(message); }
};
struct SlotDeleter {
    void operator()(sd_bus_slot* slot) const { sd_bus_slot_unref(slot); }
};

using BusPtr = std::unique_ptr<sd_bus, BusDeleter>;
using MessagePtr = std::unique_ptr<sd_bus_message, MessageDeleter>;
using SlotPtr = std::unique_ptr<sd_bus_slot, SlotDeleter>;

inline constexpr const char* busDaemonName = "org.freedesktop.DBus";

std::string resolveBusAddress(const Options& options);

// Regular connection, used for name resolution and introspection.
std::expected<BusPtr, std::string> openClientBus(const std::string& address);

// Connection turned into an eavesdropping monitor via org.freedesktop.DBus.Monitoring.
// Once BecomeMonitor succeeds, the connection can no longer send anything.
// Filtering by `matchRules` is done by the daemon, so unmatched traffic never reaches us.
std::expected<BusPtr, std::string> openMonitorBus(const std::string& address,
                                                  const std::vector<std::string>& matchRules);

std::string safeString(const char* text);
