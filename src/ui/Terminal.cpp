#include "ui/Terminal.hpp"

#include <langinfo.h>

#include <clocale>
#include <format>

namespace ui {

namespace {

bool isContinuationByte(char byte) {
    return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
}

const std::chrono::time_zone* localZone() {
    static const std::chrono::time_zone* zone = [] -> const std::chrono::time_zone* {
        try {
            return std::chrono::current_zone();
        } catch (const std::exception&) {
            return nullptr;
        }
    }();
    return zone;
}

} // namespace

Terminal::Terminal() {
    // ncurses needs a UTF-8 locale to measure multi-byte characters; an unset or
    // uninstalled locale would silently fall back to "C" and corrupt the display.
    const char* locale = std::setlocale(LC_ALL, "");
    if (!locale || std::string_view{nl_langinfo(CODESET)} != "UTF-8")
        std::setlocale(LC_CTYPE, "C.UTF-8");
    initscr();
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(callColor, COLOR_CYAN, -1);
        init_pair(returnColor, COLOR_GREEN, -1);
        init_pair(errorColor, COLOR_RED, -1);
        init_pair(signalColor, COLOR_YELLOW, -1);
        init_pair(titleColor, COLOR_BLACK, COLOR_CYAN);
        init_pair(accentColor, COLOR_MAGENTA, -1);
        init_pair(footerColor, COLOR_BLACK, COLOR_WHITE);
    }
}

Terminal::~Terminal() {
    endwin();
}

attr_t colorAttr(ColorPair pair) {
    return has_colors() ? static_cast<attr_t>(COLOR_PAIR(pair)) : A_NORMAL;
}

attr_t typeAttr(MessageType type) {
    switch (type) {
    case MessageType::methodCall: return colorAttr(callColor);
    case MessageType::methodReturn: return colorAttr(returnColor);
    case MessageType::error: return colorAttr(errorColor) | A_BOLD;
    case MessageType::signal: return colorAttr(signalColor);
    case MessageType::unknown: break;
    }
    return A_NORMAL;
}

int displayWidth(std::string_view text) {
    int width = 0;
    for (const char byte : text)
        if (!isContinuationByte(byte)) ++width;
    return width;
}

std::string fitWidth(std::string_view text, int width) {
    if (width <= 0) return {};
    const bool truncated = displayWidth(text) > width;
    const int budget = truncated ? width - 1 : width;

    std::string out;
    int used = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char byte = text[i];
        if (!isContinuationByte(byte)) {
            if (used == budget) break;
            ++used;
        }
        const auto code = static_cast<unsigned char>(byte);
        out += (code < 0x20 || code == 0x7F) ? ' ' : byte;
    }
    if (truncated) {
        out += "…";
        ++used;
    }
    out.append(static_cast<std::size_t>(width - used), ' ');
    return out;
}

int drawText(int y, int x, int width, std::string_view text, attr_t attr) {
    if (width <= 0) return 0;
    const int used = std::min(displayWidth(text), width);
    const auto fitted = fitWidth(text, used);
    attron(attr);
    mvaddstr(y, x, fitted.c_str());
    attroff(attr);
    return used;
}

void fillLine(int y, int x, int width, attr_t attr) {
    if (width <= 0) return;
    attron(attr);
    mvhline(y, x, ' ', width);
    attroff(attr);
}

std::string formatTime(std::chrono::system_clock::time_point time) {
    const auto rounded = std::chrono::floor<std::chrono::milliseconds>(time);
    if (const auto* zone = localZone()) return std::format("{:%H:%M:%S}", zone->to_local(rounded));
    return std::format("{:%H:%M:%S}", rounded);
}

std::string formatAge(std::chrono::system_clock::time_point time) {
    using namespace std::chrono;
    const auto seconds = duration_cast<std::chrono::seconds>(system_clock::now() - time).count();
    if (seconds < 60) return std::format("{}s ago", std::max<long long>(seconds, 0));
    if (seconds < 3600) return std::format("{}m ago", seconds / 60);
    return std::format("{}h ago", seconds / 3600);
}

} // namespace ui
