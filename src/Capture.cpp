#include "Capture.hpp"

#include "Bus.hpp"

Capture::Capture(NameRegistry& names, MessageStore& store, std::string monitorName,
                 std::string hiddenPeer)
    : names_{names}, store_{store}, monitorName_{std::move(monitorName)},
      hiddenPeer_{std::move(hiddenPeer)} {}

CaptureResult Capture::handle(sd_bus_message* message, bool paused) {
    if (sd_bus_message_is_signal(message, "org.freedesktop.DBus.Local", "Disconnected") > 0)
        return CaptureResult::disconnected;

    // Until the daemon strips our unique name, the messages are addressed to us, not monitored.
    if (!monitoring_) {
        const char* name = nullptr;
        if (sd_bus_message_is_signal(message, busDaemonName, "NameLost") > 0 &&
            sd_bus_message_read(message, "s", &name) >= 0 && monitorName_ == safeString(name))
            monitoring_ = true;
        return CaptureResult::notMonitoringYet;
    }

    if (!hiddenPeer_.empty()) {
        const auto sender = safeString(sd_bus_message_get_sender(message));
        const auto destination = safeString(sd_bus_message_get_destination(message));
        if (sender == hiddenPeer_ || destination == hiddenPeer_) return CaptureResult::hidden;
    }

    trackNameOwnerChange(message);
    if (paused) return CaptureResult::paused;

    store_.add(makeRecord(message));
    return CaptureResult::recorded;
}

void Capture::trackNameOwnerChange(sd_bus_message* message) {
    if (sd_bus_message_is_signal(message, busDaemonName, "NameOwnerChanged") <= 0) return;
    if (safeString(sd_bus_message_get_sender(message)) != busDaemonName) return;

    const char* name = nullptr;
    const char* oldOwner = nullptr;
    const char* newOwner = nullptr;
    if (sd_bus_message_read(message, "sss", &name, &oldOwner, &newOwner) >= 0)
        names_.onNameOwnerChanged(safeString(name), safeString(newOwner));
    sd_bus_message_rewind(message, 1);
}

std::string uniqueName(sd_bus* bus) {
    const char* name = nullptr;
    if (sd_bus_get_unique_name(bus, &name) < 0) return {};
    return safeString(name);
}
