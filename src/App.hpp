#pragma once

#include "Bus.hpp"
#include "Capture.hpp"
#include "Introspector.hpp"
#include "MessageStore.hpp"
#include "NameRegistry.hpp"
#include "Options.hpp"
#include "ui/MemberView.hpp"
#include "ui/ServiceView.hpp"
#include "ui/StreamView.hpp"
#include "ui/TextOverlay.hpp"

#include <array>
#include <chrono>
#include <optional>
#include <string>

class App {
public:
    App(Options options, BusPtr client, BusPtr monitor);

    // Runs until the user quits. Returns the process exit code.
    int run();
    const std::string& fatalError() const { return fatalError_; }

private:
    bool processBuses();
    void handleMonitorMessage(sd_bus_message* message);
    static int onNameOwnerChanged(sd_bus_message* message, void* userdata, sd_bus_error* error);

    void handleKey(int key);
    void handleFilterInputKey(int key);
    void switchView(std::size_t index);
    void openMessage(std::uint64_t id);
    void openMember(const std::string& memberKey);
    void openServiceMember(const std::string& service, const std::string& memberKey);
    void showHelp();

    void draw();
    void drawTitleBar(int width);
    void drawFooter(int y, int width);
    int pollTimeoutMs(bool moreWork) const;

    Options options_;
    BusPtr client_;
    BusPtr monitor_;
    // Match rules may exclude NameOwnerChanged from the monitor, so the client follows it too.
    SlotPtr nameOwnerWatch_;

    NameRegistry names_;
    MessageStore store_;
    Capture capture_;
    Introspector introspector_;
    ui::ViewFilter filter_;

    ui::StreamView streamView_;
    ui::MemberView memberView_;
    ui::ServiceView serviceView_;
    std::array<ui::View*, 3> views_;
    std::size_t activeView_ = 0;
    std::optional<ui::TextOverlay> overlay_;

    bool editingFilter_ = false;
    std::string filterInput_;

    bool running_ = true;
    bool paused_ = false;
    bool dirty_ = true;
    std::uint64_t ignoredWhilePaused_ = 0;
    std::string fatalError_;
    std::chrono::steady_clock::time_point lastDraw_;
};
