#pragma once

#include "MessageStore.hpp"
#include "ui/ListCursor.hpp"
#include "ui/View.hpp"

#include <cstdint>
#include <deque>
#include <functional>

namespace ui {

// Ungrouped, scrolling list of messages (newest at the bottom).
class StreamView final : public View {
public:
    using OpenMessage = std::function<void(std::uint64_t id)>;

    StreamView(const MessageStore& store, const ViewFilter& filter, OpenMessage onOpen);

    std::string_view name() const override { return "Stream"; }
    std::string_view hints() const override {
        return "↑↓ move  Enter details  End follow";
    }
    void draw(const Rect& area) override;
    bool handleKey(int key) override;

private:
    void sync();
    void drawRow(int y, const Rect& area, const MessageRecord& record, bool selected) const;

    const MessageStore& store_;
    const ViewFilter& filter_;
    OpenMessage onOpen_;

    std::deque<std::uint64_t> visibleIds_;
    std::uint64_t scannedUntil_ = 0;
    std::uint64_t generation_ = 0;
    std::string filterSignature_;

    ListCursor cursor_;
    bool follow_ = true;
    int visibleRows_ = 1;
};

} // namespace ui
