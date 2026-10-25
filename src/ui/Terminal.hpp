#pragma once

#include "MessageRecord.hpp"

#include <curses.h>

#include <chrono>
#include <string>
#include <string_view>

namespace ui {

enum ColorPair : short {
    callColor = 1,
    returnColor,
    errorColor,
    signalColor,
    titleColor,
    accentColor,
    footerColor,
};

struct Rect {
    int y = 0;
    int x = 0;
    int height = 0;
    int width = 0;
};

// RAII owner of the curses screen.
class Terminal {
public:
    Terminal();
    ~Terminal();
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
};

attr_t colorAttr(ColorPair pair);
attr_t typeAttr(MessageType type);

// Number of terminal columns, assuming one column per code point.
int displayWidth(std::string_view text);
// Truncates (with an ellipsis) or pads with spaces to exactly `width` columns.
// Control characters are replaced so that they cannot break the layout.
std::string fitWidth(std::string_view text, int width);

// Draws `text` clipped to `width` columns; returns the number of columns used.
int drawText(int y, int x, int width, std::string_view text, attr_t attr = A_NORMAL);
void fillLine(int y, int x, int width, attr_t attr);

std::string formatTime(std::chrono::system_clock::time_point time);
std::string formatAge(std::chrono::system_clock::time_point time);

} // namespace ui
