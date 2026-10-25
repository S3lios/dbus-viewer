#pragma once

#include "MessageStore.hpp"
#include "ui/ListCursor.hpp"
#include "ui/View.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ui {

// One row per interface.member, aggregating calls, replies, errors and signals.
class MemberView final : public View {
public:
    using OpenMember = std::function<void(const std::string& memberKey)>;

    MemberView(const MessageStore& store, const ViewFilter& filter, OpenMember onOpen);

    std::string_view name() const override { return "Members"; }
    std::string_view hints() const override { return "↑↓ move  Enter show messages  s sort"; }
    void draw(const Rect& area) override;
    bool handleKey(int key) override;

private:
    enum class SortMode { count, rate, name, recent };
    // Keys are copied: rows outlive a store clear until the next redraw.
    using Row = std::pair<std::string, const MemberStats*>;

    void rebuildRows();
    std::string_view sortLabel() const;

    const MessageStore& store_;
    const ViewFilter& filter_;
    OpenMember onOpen_;

    std::vector<Row> rows_;
    // Selection is tracked by key so that it survives re-sorting as counters change.
    std::string selectedKey_;
    SortMode sortMode_ = SortMode::count;
    ListCursor cursor_;
    int visibleRows_ = 1;
};

} // namespace ui
