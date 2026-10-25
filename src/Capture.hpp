#pragma once

#include "MessageStore.hpp"
#include "NameRegistry.hpp"

#include <string>

struct sd_bus;
struct sd_bus_message;

enum class CaptureResult {
    // Still receiving the replies to our own BecomeMonitor call.
    notMonitoringYet,
    // Traffic of the peer we must hide (our own helper connection).
    hidden,
    // Monitored, names tracked, but not stored because capture is paused.
    paused,
    recorded,
    disconnected,
};

// Turns the raw messages of a monitor connection into stored records.
class Capture {
public:
    Capture(NameRegistry& names, MessageStore& store, std::string monitorName,
            std::string hiddenPeer);

    CaptureResult handle(sd_bus_message* message, bool paused = false);
    void trackNameOwnerChange(sd_bus_message* message);
    bool isMonitoring() const { return monitoring_; }

private:
    NameRegistry& names_;
    MessageStore& store_;
    std::string monitorName_;
    std::string hiddenPeer_;
    bool monitoring_ = false;
};

std::string uniqueName(sd_bus* bus);
