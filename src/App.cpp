#include "App.hpp"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <format>

namespace {

using namespace std::chrono_literals;

constexpr auto redrawInterval = 50ms;
// Ages ("3s ago") must keep moving even when the bus is quiet.
constexpr auto idleRedrawInterval = 1s;
// Bounds the time spent draining the bus so that the UI stays responsive under floods.
constexpr int maxMessagesPerBatch = 2000;
constexpr int escapeKey = 27;

constexpr std::string_view helpText = R"(Views
  1 / 2 / 3     Stream / Members (interface.member) / Services
  Tab           Next view

Navigation
  ↑ ↓  j k      Move selection
  PgUp PgDn     Page up / down
  Home End g G  First / last (End re-enables auto-follow in the stream)
  Enter         Stream: message details
                Members: show the messages of this member
                Services: fold/unfold, or show the messages of a member
  ← →  h l      Services: fold / unfold

Filtering
  /             Edit the text filter (sender, destination, path, member, arguments)
  Esc           Clear the filter and the drill-down

Other
  s             Members: change sort order (count, rate, name, recent)
  d             Services: hide / show disconnected services
  p             Pause / resume capture
  c             Clear history and statistics
  ?             This help
  q             Quit

Notes
  Services are identified by their unique connection name (":1.42"); well-known
  names are resolved when the message is received. Interfaces are introspected
  lazily, only for the object paths the service was seen using, and without
  auto-starting anything.)";

std::string utf8Pop(std::string text) {
    while (!text.empty()) {
        const auto byte = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((byte & 0xC0) != 0x80) break;
    }
    return text;
}

} // namespace

App::App(Options options, BusPtr client, BusPtr monitor)
    : options_{std::move(options)}, client_{std::move(client)}, monitor_{std::move(monitor)},
      store_{options_.maxMessages, names_},
      // Our own helper connection is hidden: its traffic is an artefact of the tool.
      capture_{names_, store_, uniqueName(monitor_.get()), uniqueName(client_.get())},
      introspector_{client_.get()},
      streamView_{store_, filter_, [this](std::uint64_t id) { openMessage(id); }},
      memberView_{store_, filter_, [this](const std::string& key) { openMember(key); }},
      serviceView_{store_, names_, introspector_, filter_,
                   [this](const std::string& service, const std::string& key) {
                       openServiceMember(service, key);
                   }},
      views_{&streamView_, &memberView_, &serviceView_} {
    names_.loadFrom(client_.get());

    sd_bus_slot* slot = nullptr;
    if (sd_bus_match_signal(client_.get(), &slot, busDaemonName, "/org/freedesktop/DBus",
                            "org.freedesktop.DBus", "NameOwnerChanged", &App::onNameOwnerChanged,
                            this) >= 0)
        nameOwnerWatch_.reset(slot);
}

int App::onNameOwnerChanged(sd_bus_message* message, void* userdata, sd_bus_error*) {
    static_cast<App*>(userdata)->capture_.trackNameOwnerChange(message);
    return 0;
}

int App::run() {
    std::optional<ui::Terminal> terminal{std::in_place};

    while (running_) {
        const bool moreWork = processBuses();

        for (int key = getch(); key != ERR && running_; key = getch()) {
            if (key == KEY_RESIZE) {
                dirty_ = true;
                continue;
            }
            handleKey(key);
            dirty_ = true;
        }

        if (introspector_.takeChanged()) dirty_ = true;
        const auto now = std::chrono::steady_clock::now();
        if ((dirty_ && now - lastDraw_ >= redrawInterval) || now - lastDraw_ >= idleRedrawInterval)
            draw();

        if (!running_) break;
        std::array<pollfd, 3> fds{{
            {STDIN_FILENO, POLLIN, 0},
            {sd_bus_get_fd(monitor_.get()), static_cast<short>(sd_bus_get_events(monitor_.get())),
             0},
            {sd_bus_get_fd(client_.get()), static_cast<short>(sd_bus_get_events(client_.get())), 0},
        }};
        if (poll(fds.data(), fds.size(), pollTimeoutMs(moreWork)) < 0 && errno != EINTR) {
            fatalError_ = std::format("poll failed: {}", std::strerror(errno));
            running_ = false;
        }
    }

    terminal.reset();
    return fatalError_.empty() ? 0 : 1;
}

int App::pollTimeoutMs(bool moreWork) const {
    if (moreWork) return 0;

    using namespace std::chrono;
    const auto now = steady_clock::now();
    auto deadline = lastDraw_ + (dirty_ ? redrawInterval : idleRedrawInterval);

    // sd-bus timeouts are absolute CLOCK_MONOTONIC values, which is what steady_clock uses on
    // Linux.
    for (sd_bus* bus : {monitor_.get(), client_.get()}) {
        std::uint64_t usec = 0;
        if (sd_bus_get_timeout(bus, &usec) > 0 && usec != UINT64_MAX)
            deadline = std::min(deadline, steady_clock::time_point{microseconds{usec}});
    }
    const auto remaining = duration_cast<milliseconds>(deadline - now).count();
    return static_cast<int>(std::clamp<long long>(remaining + 1, 0, 1000));
}

bool App::processBuses() {
    for (int i = 0; i < maxMessagesPerBatch; ++i) {
        sd_bus_message* raw = nullptr;
        const int r = sd_bus_process(monitor_.get(), &raw);
        MessagePtr message{raw};
        if (r < 0) {
            fatalError_ = std::format("monitor connection failed: {}", std::strerror(-r));
            running_ = false;
            return false;
        }
        if (message) handleMonitorMessage(message.get());
        if (r == 0) break;
        if (i + 1 == maxMessagesPerBatch) return true;
    }

    // Replies to our own requests (introspection) are dispatched to their callbacks here.
    while (true) {
        const int r = sd_bus_process(client_.get(), nullptr);
        if (r < 0) {
            fatalError_ = std::format("client connection failed: {}", std::strerror(-r));
            running_ = false;
            return false;
        }
        if (r == 0) break;
    }
    return false;
}

void App::handleMonitorMessage(sd_bus_message* message) {
    switch (capture_.handle(message, paused_)) {
    case CaptureResult::disconnected:
        fatalError_ = "disconnected from the bus";
        running_ = false;
        return;
    case CaptureResult::paused:
        ++ignoredWhilePaused_;
        dirty_ = true;
        return;
    case CaptureResult::recorded: dirty_ = true; return;
    case CaptureResult::notMonitoringYet:
    case CaptureResult::hidden: return;
    }
}

void App::handleKey(int key) {
    if (overlay_) {
        if (!overlay_->handleKey(key)) overlay_.reset();
        return;
    }
    if (editingFilter_) {
        handleFilterInputKey(key);
        return;
    }

    switch (key) {
    case 'q': running_ = false; return;
    case '1': switchView(0); return;
    case '2': switchView(1); return;
    case '3': switchView(2); return;
    case '\t': switchView((activeView_ + 1) % views_.size()); return;
    case '?': showHelp(); return;
    case 'p':
        paused_ = !paused_;
        ignoredWhilePaused_ = 0;
        return;
    case 'c':
        store_.clear();
        introspector_.clear();
        return;
    case '/':
        editingFilter_ = true;
        filterInput_ = filter_.text;
        return;
    case escapeKey: filter_ = {}; return;
    default: views_[activeView_]->handleKey(key); return;
    }
}

void App::handleFilterInputKey(int key) {
    switch (key) {
    case escapeKey: editingFilter_ = false; return;
    case '\n':
    case '\r':
    case KEY_ENTER:
        filter_.text = filterInput_;
        editingFilter_ = false;
        return;
    case KEY_BACKSPACE:
    case 127:
    case '\b': filterInput_ = utf8Pop(std::move(filterInput_)); return;
    default:
        // Multi-byte UTF-8 characters arrive one byte per getch() call.
        if (key >= 0x20 && key <= 0xFF && key != 127) filterInput_ += static_cast<char>(key);
        return;
    }
}

void App::switchView(std::size_t index) {
    activeView_ = index;
}

void App::openMessage(std::uint64_t id) {
    const auto* record = store_.find(id);
    if (!record) return;

    const auto describe = [this](const std::string& name, const std::string& service) {
        if (name.empty()) return std::string{"(broadcast)"};
        const auto aliases = names_.namesOf(service);
        std::string text = name;
        if (service != name) text += std::format("  [owner {}]", service);
        for (const auto& alias : aliases)
            if (alias != name) text += std::format("  {}", alias);
        return text;
    };

    auto text = std::format("Type:        {}\n"
                            "Time:        {}\n"
                            "Member:      {}\n"
                            "Sender:      {}\n"
                            "Destination: {}\n\n",
                            longTypeName(record->type), ui::formatTime(record->time),
                            record->memberKey, describe(record->sender, record->senderService),
                            describe(record->destination, record->destinationService));
    text += dumpMessage(record->raw.get());
    overlay_.emplace(std::format("Message #{}", record->id), text);
}

void App::openMember(const std::string& memberKey) {
    filter_.memberKey = memberKey;
    filter_.service.clear();
    switchView(0);
}

void App::openServiceMember(const std::string& service, const std::string& memberKey) {
    filter_.memberKey = memberKey;
    filter_.service = service;
    switchView(0);
}

void App::showHelp() {
    overlay_.emplace("Help", helpText);
}

void App::draw() {
    lastDraw_ = std::chrono::steady_clock::now();
    dirty_ = false;

    erase();
    int height = 0;
    int width = 0;
    getmaxyx(stdscr, height, width);
    if (height < 5 || width < 40) {
        ui::drawText(0, 0, width, "Terminal too small");
        refresh();
        return;
    }

    drawTitleBar(width);
    views_[activeView_]->draw({1, 0, height - 2, width});
    drawFooter(height - 1, width);
    if (overlay_) overlay_->draw({0, 0, height, width});
    refresh();
}

void App::drawTitleBar(int width) {
    const auto title = ui::colorAttr(ui::titleColor);
    ui::fillLine(0, 0, width, title);

    const auto rules = options_.matchRules.size();
    const auto filtering = rules == 0   ? std::string{}
                           : rules == 1 ? std::string{" (1 match rule)"}
                                        : std::format(" ({} match rules)", rules);
    int x = ui::drawText(0, 0, width,
                         std::format(" dbus-viewer │ {} bus{} │ ", busLabel(options_), filtering),
                         title | A_BOLD);
    for (std::size_t i = 0; i < views_.size(); ++i) {
        const auto tab = std::format(" {} {} ", i + 1, views_[i]->name());
        const attr_t attr = i == activeView_ ? (title | A_REVERSE | A_BOLD) : title;
        x += ui::drawText(0, x, width - x, tab, attr);
    }

    // Longest variant that fits: the pause state matters more than the counters.
    const auto messages = std::format("{} messages ", store_.capturedCount());
    const std::array candidates =
        paused_ ? std::array{std::format("PAUSED ({} ignored) │ {}", ignoredWhilePaused_, messages),
                             std::string{"PAUSED "}}
                : std::array{messages, std::string{}};
    for (const auto& status : candidates) {
        const int statusWidth = ui::displayWidth(status);
        if (status.empty() || x + statusWidth >= width) continue;
        ui::drawText(0, width - statusWidth, statusWidth, status,
                     paused_ ? (title | A_BOLD | A_BLINK) : title);
        break;
    }
}

void App::drawFooter(int y, int width) {
    const auto footer = ui::colorAttr(ui::footerColor);
    ui::fillLine(y, 0, width, footer);

    if (editingFilter_) {
        const auto prompt = std::format(" Filter: {}", filterInput_);
        const int used = ui::drawText(y, 0, width, prompt, footer | A_BOLD);
        if (used < width) ui::drawText(y, used, 1, " ", A_REVERSE);
        ui::drawText(y, used + 2, width - used - 2, "Enter apply  Esc cancel", footer);
        return;
    }

    std::string text;
    if (!filter_.empty()) {
        text += " Filter:";
        if (!filter_.service.empty())
            text += std::format(" service={}", names_.label(filter_.service));
        if (!filter_.memberKey.empty()) text += std::format(" member={}", filter_.memberKey);
        if (!filter_.text.empty()) text += std::format(" \"{}\"", filter_.text);
        text += " (Esc clears) │";
    }
    text += std::format(" {}  / filter  p pause  c clear  ? help  q quit",
                        views_[activeView_]->hints());
    ui::drawText(y, 0, width, text, footer);
}
