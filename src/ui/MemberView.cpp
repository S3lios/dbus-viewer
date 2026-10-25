#include "ui/MemberView.hpp"

#include <algorithm>
#include <format>

namespace ui {

namespace {

constexpr int countWidth = 8;
constexpr int ageWidth = 9;
constexpr int rateWidth = 8;

attr_t memberAttr(const MemberStats& stats) {
    if (stats.signals > 0 && stats.calls == 0) return typeAttr(MessageType::signal);
    return typeAttr(MessageType::methodCall);
}

std::string countText(std::uint64_t value) {
    return value == 0 ? std::string{"·"} : std::to_string(value);
}

std::string rateText(double perSecond) {
    if (perSecond <= 0.0) return "·";
    return perSecond < 10.0 ? std::format("{:.2f}", perSecond) : std::format("{:.1f}", perSecond);
}

} // namespace

MemberView::MemberView(const MessageStore& store, const ViewFilter& filter, OpenMember onOpen)
    : store_{store}, filter_{filter}, onOpen_{std::move(onOpen)} {}

void MemberView::rebuildRows() {
    rows_.clear();
    for (const auto& [key, stats] : store_.members())
        if (containsIgnoreCase(key, filter_.text)) rows_.emplace_back(key, &stats);

    switch (sortMode_) {
    case SortMode::count:
        std::ranges::stable_sort(rows_, std::greater{},
                                 [](const Row& row) { return row.second->total(); });
        break;
    case SortMode::rate: {
        const auto now = std::chrono::system_clock::now();
        std::ranges::stable_sort(rows_, std::greater{}, [now](const Row& row) {
            return row.second->recentRate.perSecond(now);
        });
        break;
    }
    case SortMode::recent:
        std::ranges::stable_sort(rows_, std::greater{},
                                 [](const Row& row) { return row.second->lastSeen; });
        break;
    case SortMode::name: break; // std::map order
    }

    const auto selected =
        std::ranges::find(rows_, selectedKey_, [](const Row& row) { return row.first; });
    if (selected != rows_.end())
        cursor_.selected = static_cast<std::size_t>(selected - rows_.begin());
    cursor_.clamp(rows_.size(), visibleRows_);
    if (!rows_.empty()) selectedKey_ = rows_[cursor_.selected].first;
}

std::string_view MemberView::sortLabel() const {
    switch (sortMode_) {
    case SortMode::count: return "count";
    case SortMode::rate: return "rate";
    case SortMode::name: return "name";
    case SortMode::recent: return "recent";
    }
    return {};
}

void MemberView::draw(const Rect& area) {
    visibleRows_ = std::max(area.height - 1, 1);
    rebuildRows();

    int x = area.x;
    const auto header = [&](std::string_view title, int width) {
        drawText(area.y, x, width, title, A_BOLD | A_UNDERLINE);
        x += width + 1;
    };
    header("Total", countWidth);
    header("Calls", countWidth);
    header("Returns", countWidth);
    header("Errors", countWidth);
    header("Signals", countWidth);
    header("Rate/s", rateWidth);
    header("Avg/s", rateWidth);
    header("Last", ageWidth);
    header(std::format("Interface.Member (sorted by {})", sortLabel()), area.x + area.width - x);

    if (rows_.empty()) {
        drawText(area.y + 2, area.x + 2, area.width - 4, "No message yet.", A_DIM);
        return;
    }

    const auto now = std::chrono::system_clock::now();
    for (int row = 0; row < visibleRows_; ++row) {
        const auto index = cursor_.offset + static_cast<std::size_t>(row);
        if (index >= rows_.size()) break;
        const auto& [key, stats] = rows_[index];
        const int y = area.y + 1 + row;
        const attr_t base = index == cursor_.selected ? A_REVERSE : A_NORMAL;
        if (base != A_NORMAL) fillLine(y, area.x, area.width, base);

        x = area.x;
        const auto cell = [&](int width, std::string_view text, attr_t attr) {
            drawText(y, x, width, text, attr | base);
            x += width + 1;
        };
        cell(countWidth, std::to_string(stats->total()), A_BOLD);
        cell(countWidth, countText(stats->calls), typeAttr(MessageType::methodCall));
        cell(countWidth, countText(stats->returns), typeAttr(MessageType::methodReturn));
        cell(countWidth, countText(stats->errors), typeAttr(MessageType::error));
        cell(countWidth, countText(stats->signals), typeAttr(MessageType::signal));
        cell(rateWidth, rateText(stats->recentRate.perSecond(now)), A_BOLD);
        cell(rateWidth, rateText(stats->averageRate(now).value_or(0.0)), A_NORMAL);
        cell(ageWidth, formatAge(stats->lastSeen), A_DIM);
        cell(area.x + area.width - x, key, memberAttr(*stats));
    }
}

bool MemberView::handleKey(int key) {
    if (key == '\n' || key == '\r' || key == KEY_ENTER) {
        if (!rows_.empty()) onOpen_(rows_[cursor_.selected].first);
        return true;
    }
    if (key == 's') {
        switch (sortMode_) {
        case SortMode::count: sortMode_ = SortMode::rate; break;
        case SortMode::rate: sortMode_ = SortMode::name; break;
        case SortMode::name: sortMode_ = SortMode::recent; break;
        case SortMode::recent: sortMode_ = SortMode::count; break;
        }
        return true;
    }
    if (!cursor_.handleKey(key, rows_.size(), visibleRows_)) return false;
    if (!rows_.empty()) selectedKey_ = rows_[cursor_.selected].first;
    return true;
}

} // namespace ui
