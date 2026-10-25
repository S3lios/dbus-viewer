#pragma once

#include "ui/ListCursor.hpp"
#include "ui/Terminal.hpp"

#include <string>
#include <vector>

namespace ui {

// Scrollable framed text box drawn over the current view (message details, help).
class TextOverlay {
public:
    TextOverlay(std::string title, std::string_view text);

    void draw(const Rect& screen);
    // Returns false when the overlay must be closed.
    bool handleKey(int key);

private:
    std::string title_;
    std::vector<std::string> lines_;
    std::size_t scroll_ = 0;
    int horizontalScroll_ = 0;
    int visibleRows_ = 1;
};

} // namespace ui
