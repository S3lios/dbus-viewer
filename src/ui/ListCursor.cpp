#include "ui/ListCursor.hpp"

#include <curses.h>

#include <algorithm>

namespace ui {

bool ListCursor::handleKey(int key, std::size_t count, int visibleRows) {
    const auto page = static_cast<std::size_t>(std::max(visibleRows - 1, 1));
    switch (key) {
    case KEY_UP:
    case 'k':
        if (selected > 0) --selected;
        break;
    case KEY_DOWN:
    case 'j': ++selected; break;
    case KEY_PPAGE: selected = selected > page ? selected - page : 0; break;
    case KEY_NPAGE: selected += page; break;
    case KEY_HOME:
    case 'g': selected = 0; break;
    case KEY_END:
    case 'G': selected = count; break;
    default: return false;
    }
    clamp(count, visibleRows);
    return true;
}

void ListCursor::clamp(std::size_t count, int visibleRows) {
    const auto rows = static_cast<std::size_t>(std::max(visibleRows, 1));
    selected = count == 0 ? 0 : std::min(selected, count - 1);
    if (selected < offset) offset = selected;
    if (selected >= offset + rows) offset = selected + 1 - rows;
    // Avoid leaving empty rows at the bottom when the list shrinks.
    offset = std::min(offset, count > rows ? count - rows : 0);
}

} // namespace ui
