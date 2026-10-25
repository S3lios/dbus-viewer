#pragma once

#include "Introspector.hpp"
#include "MessageStore.hpp"
#include "NameRegistry.hpp"
#include "ui/ListCursor.hpp"
#include "ui/View.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace ui {

// Tree: service > sent / received members, plus the interfaces it exposes (introspected).
class ServiceView final : public View {
public:
    using OpenServiceMember =
        std::function<void(const std::string& service, const std::string& memberKey)>;

    ServiceView(const MessageStore& store, const NameRegistry& names, Introspector& introspector,
                const ViewFilter& filter, OpenServiceMember onOpen);

    std::string_view name() const override { return "Services"; }
    std::string_view hints() const override {
        return "↑↓ move  ←→/Enter fold  Enter on member: messages  d disconnected";
    }
    void draw(const Rect& area) override;
    bool handleKey(int key) override;

private:
    struct Segment {
        std::string text;
        attr_t attr = A_NORMAL;
    };
    // Everything is copied: rows outlive a store clear until the next redraw.
    struct Row {
        std::string id;
        int depth = 0;
        bool expandable = false;
        bool expanded = false;
        std::vector<Segment> segments;
        std::string service;
        std::string memberKey;
    };

    void rebuildRows();
    bool matchesFilter(const std::string& service, const ServiceStats& stats) const;
    void addService(const std::string& service, const ServiceStats& stats);
    void addMemberGroup(const std::string& parentId, std::string_view title,
                        const std::map<TypedMemberKey, TrafficStats>& members,
                        const TrafficStats& total, const std::string& service);
    void addInterfaces(const std::string& parentId, const std::string& service,
                       const ServiceStats& stats);
    Row& addRow(std::string id, int depth, bool expandable);
    void setExpanded(const Row& row, bool expanded);
    void selectParent();

    const MessageStore& store_;
    const NameRegistry& names_;
    Introspector& introspector_;
    const ViewFilter& filter_;
    OpenServiceMember onOpen_;

    std::vector<Row> rows_;
    std::set<std::string> expanded_;
    std::string selectedId_;
    bool hideDisconnected_ = false;
    ListCursor cursor_;
    int visibleRows_ = 1;
};

} // namespace ui
