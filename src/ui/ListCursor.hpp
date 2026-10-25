#pragma once

#include <cstddef>

namespace ui {

// Selection + scroll offset of a vertical list.
struct ListCursor {
    std::size_t selected = 0;
    std::size_t offset = 0;

    // Handles navigation keys; returns true if the key was one of them.
    bool handleKey(int key, std::size_t count, int visibleRows);
    // Keeps the selection inside the list and visible.
    void clamp(std::size_t count, int visibleRows);
    bool isLast(std::size_t count) const { return count == 0 || selected + 1 >= count; }
};

} // namespace ui
