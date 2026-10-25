#include "ui/StreamView.hpp"

#include <algorithm>
#include <format>

namespace ui {

namespace {

struct Columns {
    int time = 12;
    int type = 4;
    int route = 0;
    int member = 0;
    int path = 0;
    int args = 0;
};

Columns layoutColumns(int width) {
    Columns columns;
    int rest = width - columns.time - columns.type - 2;
    columns.path = width >= 160 ? 32 : 0;
    rest -= columns.path ? columns.path + 1 : 0;
    columns.route = std::clamp(rest * 35 / 100, 16, 56);
    columns.member = std::clamp(rest * 35 / 100, 16, 64);
    columns.args = std::max(rest - columns.route - columns.member - 2, 0);
    return columns;
}

std::string routeOf(const MessageRecord& record) {
    const auto& destination = record.destinationLabel.empty() ? std::string{"*"}
                                                               : record.destinationLabel;
    return std::format("{} → {}", record.senderLabel, destination);
}

std::string detailsOf(const MessageRecord& record) {
    if (record.type == MessageType::error)
        return record.argsSummary.empty() ? record.errorName
                                          : std::format("{}: {}", record.errorName, record.argsSummary);
    return record.argsSummary;
}

} // namespace

StreamView::StreamView(const MessageStore& store, const ViewFilter& filter, OpenMessage onOpen)
    : store_{store}, filter_{filter}, onOpen_{std::move(onOpen)} {}

void StreamView::sync() {
    const auto signature = filter_.signature();
    if (generation_ != store_.generation() || signature != filterSignature_) {
        generation_ = store_.generation();
        filterSignature_ = signature;
        visibleIds_.clear();
        scannedUntil_ = store_.firstId();
        cursor_ = {};
        follow_ = true;
    }

    // Messages evicted from the store's history.
    while (!visibleIds_.empty() && visibleIds_.front() < store_.firstId()) {
        visibleIds_.pop_front();
        if (cursor_.selected > 0) --cursor_.selected;
        if (cursor_.offset > 0) --cursor_.offset;
    }

    for (auto id = std::max(scannedUntil_, store_.firstId()); id < store_.nextId(); ++id)
        if (matches(*store_.find(id), filter_)) visibleIds_.push_back(id);
    scannedUntil_ = store_.nextId();

    if (follow_ && !visibleIds_.empty()) cursor_.selected = visibleIds_.size() - 1;
    cursor_.clamp(visibleIds_.size(), visibleRows_);
}

void StreamView::draw(const Rect& area) {
    visibleRows_ = std::max(area.height - 1, 1);
    sync();

    const auto columns = layoutColumns(area.width);
    int x = area.x;
    const auto header = [&](std::string_view title, int width) {
        if (width <= 0) return;
        drawText(area.y, x, width, title, A_BOLD | A_UNDERLINE);
        x += width + 1;
    };
    header("Time", columns.time);
    header("Type", columns.type);
    header("Sender → Destination", columns.route);
    header("Member", columns.member);
    header("Path", columns.path);
    header("Arguments", columns.args);

    if (visibleIds_.empty()) {
        drawText(area.y + 2, area.x + 2, area.width - 4,
                 filter_.empty() ? "Waiting for messages…" : "No message matches the filter.",
                 A_DIM);
        return;
    }

    for (int row = 0; row < visibleRows_; ++row) {
        const auto index = cursor_.offset + static_cast<std::size_t>(row);
        if (index >= visibleIds_.size()) break;
        if (const auto* record = store_.find(visibleIds_[index]))
            drawRow(area.y + 1 + row, area, *record, index == cursor_.selected);
    }
}

void StreamView::drawRow(int y, const Rect& area, const MessageRecord& record, bool selected) const {
    const auto columns = layoutColumns(area.width);
    const attr_t base = selected ? A_REVERSE : A_NORMAL;
    if (selected) fillLine(y, area.x, area.width, base);

    int x = area.x;
    const auto cell = [&](int width, std::string_view text, attr_t attr) {
        if (width <= 0) return;
        drawText(y, x, width, text, attr | base);
        x += width + 1;
    };
    cell(columns.time, formatTime(record.time), A_DIM);
    cell(columns.type, shortTypeName(record.type), typeAttr(record.type));
    cell(columns.route, routeOf(record), A_NORMAL);
    cell(columns.member, record.memberKey, typeAttr(record.type));
    cell(columns.path, record.path, A_DIM);
    cell(columns.args, detailsOf(record), A_NORMAL);
}

bool StreamView::handleKey(int key) {
    if (key == '\n' || key == '\r' || key == KEY_ENTER) {
        if (cursor_.selected < visibleIds_.size()) onOpen_(visibleIds_[cursor_.selected]);
        return true;
    }
    if (!cursor_.handleKey(key, visibleIds_.size(), visibleRows_)) return false;
    follow_ = cursor_.isLast(visibleIds_.size());
    return true;
}

} // namespace ui
