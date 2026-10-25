#include "ui/TextOverlay.hpp"

#include <algorithm>
#include <cctype>
#include <format>

namespace ui {

namespace {

constexpr int horizontalStep = 8;

// Removes ANSI escape sequences and expands tabs.
std::string sanitize(std::string_view line) {
    std::string out;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\x1b') {
            while (i < line.size() && !std::isalpha(static_cast<unsigned char>(line[i]))) ++i;
            continue;
        }
        if (line[i] == '\t') {
            out.append(4 - displayWidth(out) % 4, ' ');
            continue;
        }
        out += line[i];
    }
    return out;
}

std::string_view skipColumns(std::string_view text, int columns) {
    std::size_t i = 0;
    while (columns > 0 && i < text.size()) {
        ++i;
        while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) ++i;
        --columns;
    }
    return text.substr(i);
}

} // namespace

TextOverlay::TextOverlay(std::string title, std::string_view text) : title_{std::move(title)} {
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        lines_.push_back(sanitize(text.substr(start, end - start)));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    while (!lines_.empty() && lines_.back().empty()) lines_.pop_back();
}

void TextOverlay::draw(const Rect& screen) {
    const Rect box{screen.y + 1, screen.x + 2, screen.height - 2, screen.width - 4};
    if (box.height < 3 || box.width < 10) return;
    visibleRows_ = box.height - 2;
    const auto maxScroll =
        lines_.size() > static_cast<std::size_t>(visibleRows_) ? lines_.size() - visibleRows_ : 0;
    scroll_ = std::min(scroll_, maxScroll);

    for (int y = box.y; y < box.y + box.height; ++y) fillLine(y, box.x, box.width, A_NORMAL);
    std::string horizontal;
    for (int i = 0; i < box.width - 2; ++i) horizontal += "─";
    const int right = box.x + box.width - 1;
    drawText(box.y, box.x, box.width, std::format("┌{}┐", horizontal));
    drawText(box.y + box.height - 1, box.x, box.width, std::format("└{}┘", horizontal));
    for (int y = box.y + 1; y < box.y + box.height - 1; ++y) {
        drawText(y, box.x, 1, "│");
        drawText(y, right, 1, "│");
    }

    drawText(box.y, box.x + 2, box.width - 4, std::format(" {} ", title_),
             colorAttr(accentColor) | A_BOLD);
    const auto position =
        std::format(" {}/{}  ↑↓ ←→ scroll  q/Esc close ",
                    std::min(scroll_ + visibleRows_, lines_.size()), lines_.size());
    const int positionWidth = displayWidth(position);
    drawText(box.y + box.height - 1, box.x + box.width - 2 - positionWidth, positionWidth, position,
             A_DIM);

    for (int row = 0; row < visibleRows_; ++row) {
        const auto index = scroll_ + static_cast<std::size_t>(row);
        if (index >= lines_.size()) break;
        drawText(box.y + 1 + row, box.x + 2, box.width - 4,
                 skipColumns(lines_[index], horizontalScroll_));
    }
}

bool TextOverlay::handleKey(int key) {
    const auto page = static_cast<std::size_t>(std::max(visibleRows_ - 1, 1));
    switch (key) {
    case 'q':
    case 27:
    case '\n':
    case '\r':
    case KEY_ENTER: return false;
    case KEY_UP:
    case 'k':
        if (scroll_ > 0) --scroll_;
        break;
    case KEY_DOWN:
    case 'j': ++scroll_; break;
    case KEY_PPAGE: scroll_ = scroll_ > page ? scroll_ - page : 0; break;
    case KEY_NPAGE:
    case ' ': scroll_ += page; break;
    case KEY_HOME:
    case 'g': scroll_ = 0; break;
    case KEY_END:
    case 'G': scroll_ = lines_.size(); break;
    case KEY_LEFT:
    case 'h': horizontalScroll_ = std::max(horizontalScroll_ - horizontalStep, 0); break;
    case KEY_RIGHT:
    case 'l': horizontalScroll_ += horizontalStep; break;
    default: break;
    }
    return true;
}

} // namespace ui
