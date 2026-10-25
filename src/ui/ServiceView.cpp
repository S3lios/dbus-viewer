#include "ui/ServiceView.hpp"

#include <algorithm>
#include <format>
#include <tuple>

namespace ui {

namespace {

std::set<std::string> observedMemberKeys(const ServiceStats& stats) {
    std::set<std::string> keys;
    for (const auto& [typedKey, traffic] : stats.sent) keys.insert(typedKey.second);
    for (const auto& [typedKey, traffic] : stats.received) keys.insert(typedKey.second);
    return keys;
}

std::string rateText(const TrafficStats& stats) {
    const auto perSecond = stats.rate.perSecond(std::chrono::system_clock::now());
    if (perSecond <= 0.0) return {};
    return perSecond < 10.0 ? std::format("  {:.2f}/s", perSecond)
                            : std::format("  {:.1f}/s", perSecond);
}

std::string joinNames(const std::set<std::string>& names, const std::string& skip) {
    std::string out;
    for (const auto& name : names) {
        if (name == skip) continue;
        if (!out.empty()) out += ", ";
        out += name;
    }
    return out;
}

} // namespace

ServiceView::ServiceView(const MessageStore& store, const NameRegistry& names,
                         Introspector& introspector, const ViewFilter& filter,
                         OpenServiceMember onOpen)
    : store_{store}, names_{names}, introspector_{introspector}, filter_{filter},
      onOpen_{std::move(onOpen)} {}

ServiceView::Row& ServiceView::addRow(std::string id, int depth, bool expandable) {
    auto& row = rows_.emplace_back();
    row.expanded = expandable && expanded_.contains(id);
    row.id = std::move(id);
    row.depth = depth;
    row.expandable = expandable;
    return row;
}

bool ServiceView::matchesFilter(const std::string& service, const ServiceStats& stats) const {
    if (filter_.text.empty() || containsIgnoreCase(service, filter_.text)) return true;
    for (const auto& name : names_.namesOf(service))
        if (containsIgnoreCase(name, filter_.text)) return true;
    return std::ranges::any_of(observedMemberKeys(stats), [&](const std::string& key) {
        return containsIgnoreCase(key, filter_.text);
    });
}

void ServiceView::rebuildRows() {
    rows_.clear();

    std::vector<std::pair<std::string, const ServiceStats*>> services;
    for (const auto& [service, stats] : store_.services()) {
        if (hideDisconnected_ && names_.isDisconnected(service)) continue;
        if (matchesFilter(service, stats)) services.emplace_back(service, &stats);
    }
    // Named services first, alphabetically; anonymous connections (":1.42") last.
    std::ranges::sort(services, {}, [&](const auto& entry) {
        auto label = names_.label(entry.first);
        return std::tuple{isUniqueName(label), std::move(label)};
    });

    for (const auto& [service, stats] : services) addService(service, *stats);

    const auto selected = std::ranges::find(rows_, selectedId_, &Row::id);
    if (selected != rows_.end())
        cursor_.selected = static_cast<std::size_t>(selected - rows_.begin());
    cursor_.clamp(rows_.size(), visibleRows_);
    if (!rows_.empty()) selectedId_ = rows_[cursor_.selected].id;
}

void ServiceView::addService(const std::string& service, const ServiceStats& stats) {
    const auto id = std::format("s:{}", service);
    const bool disconnected = names_.isDisconnected(service);
    const auto label = names_.label(service);

    auto& row = addRow(id, 0, true);
    row.service = service;
    row.segments.push_back({label, disconnected ? A_DIM : attr_t{A_BOLD}});
    if (label != service) row.segments.push_back({std::format(" ({})", service), A_DIM});
    row.segments.push_back({std::format("  sent {}", stats.sentTotal.count), A_NORMAL});
    row.segments.push_back({rateText(stats.sentTotal), A_BOLD});
    row.segments.push_back({std::format("  received {}", stats.receivedTotal.count), A_NORMAL});
    row.segments.push_back({rateText(stats.receivedTotal), A_BOLD});
    if (disconnected) row.segments.push_back({"  disconnected", colorAttr(errorColor) | A_DIM});
    if (const auto aliases = joinNames(names_.namesOf(service), label); !aliases.empty())
        row.segments.push_back({std::format("  also: {}", aliases), A_DIM});

    if (!row.expanded) return;
    addMemberGroup(id + "/sent", "Sent (as sender)", stats.sent, stats.sentTotal, service);
    addMemberGroup(id + "/recv", "Received (as destination)", stats.received,
                   stats.receivedTotal, service);
    addInterfaces(id + "/if", service, stats);
}

void ServiceView::addMemberGroup(const std::string& parentId, std::string_view title,
                                 const std::map<TypedMemberKey, TrafficStats>& members,
                                 const TrafficStats& total, const std::string& service) {
    auto& header = addRow(parentId, 1, !members.empty());
    header.segments.push_back({std::string{title}, colorAttr(accentColor) | A_BOLD});
    header.segments.push_back(
        {std::format("  {} messages, {} members", total.count, members.size()), A_DIM});
    header.segments.push_back({rateText(total), A_BOLD});
    if (!header.expanded) return;

    std::vector<std::pair<const TypedMemberKey*, const TrafficStats*>> sorted;
    for (const auto& [typedKey, stats] : members) sorted.emplace_back(&typedKey, &stats);
    std::ranges::sort(sorted, {}, [](const auto& entry) {
        return std::tie(entry.first->second, entry.first->first);
    });

    for (const auto& [typedKeyPtr, stats] : sorted) {
        const auto& typedKey = *typedKeyPtr;
        const auto& [type, memberKey] = typedKey;
        auto& leaf = addRow(std::format("{}/{}:{}", parentId, shortTypeName(type), memberKey), 2,
                            false);
        leaf.service = service;
        leaf.memberKey = memberKey;
        leaf.segments.push_back({fitWidth(shortTypeName(type), 5), typeAttr(type)});
        leaf.segments.push_back({memberKey, typeAttr(type)});
        leaf.segments.push_back({std::format("  ×{}", stats->count), A_DIM});
        leaf.segments.push_back({rateText(*stats), A_BOLD});
    }
}

void ServiceView::addInterfaces(const std::string& parentId, const std::string& service,
                                const ServiceStats& stats) {
    auto& header = addRow(parentId, 1, true);
    header.segments.push_back({"Interfaces (introspection)", colorAttr(accentColor) | A_BOLD});
    header.segments.push_back(
        {std::format("  {} object paths observed", stats.objectPaths.size()), A_DIM});
    if (!header.expanded) return;

    // Introspection is lazy: only services the user looks at are queried.
    introspector_.request(service, stats.objectPaths);
    const auto* result = introspector_.result(service);
    const auto observed = observedMemberKeys(stats);

    if (stats.objectPaths.empty()) {
        addRow(parentId + "/none", 2, false)
            .segments.push_back({"No object path observed yet for this service.", A_DIM});
    }
    if (!result) return;

    if (result->pendingCalls > 0) {
        addRow(parentId + "/pending", 2, false)
            .segments.push_back(
                {std::format("Introspecting… ({} pending)", result->pendingCalls), A_DIM});
    }

    for (const auto& [interface, info] : result->interfaces) {
        const auto interfaceId = std::format("{}/{}", parentId, interface);
        // Counted from the introspected members: a prefix match on the observed keys would also
        // count nested interfaces ("org.foo" vs "org.foo.Bar") and non-existent members.
        const auto isUsed = [&](const std::string& member) {
            return observed.contains(memberKeyOf(interface, member));
        };
        const auto usedCount = std::ranges::count_if(info.methods, isUsed) +
                               std::ranges::count_if(info.signals, isUsed) +
                               std::ranges::count_if(info.properties, isUsed);

        auto& node = addRow(interfaceId, 2, true);
        node.segments.push_back({interface, A_BOLD});
        node.segments.push_back({std::format("  {} methods, {} signals, {} properties",
                                             info.methods.size(), info.signals.size(),
                                             info.properties.size()),
                                 A_DIM});
        if (usedCount > 0)
            node.segments.push_back(
                {std::format("  ● {} used", usedCount), colorAttr(returnColor)});
        if (!node.expanded) continue;

        const auto addMembers = [&](const std::set<std::string>& members, std::string_view kind,
                                    attr_t attr) {
            for (const auto& member : members) {
                const auto memberKey = memberKeyOf(interface, member);
                const bool used = observed.contains(memberKey);
                auto& leaf = addRow(std::format("{}/{}:{}", interfaceId, kind, member), 3, false);
                leaf.segments.push_back({fitWidth(kind, 7), attr});
                leaf.segments.push_back({member, used ? attr_t{A_BOLD} : attr_t{A_NORMAL}});
                if (used) {
                    leaf.segments.push_back({"  ● used", colorAttr(returnColor)});
                    leaf.service = service;
                    leaf.memberKey = memberKey;
                }
            }
        };
        addMembers(info.methods, "method", typeAttr(MessageType::methodCall));
        addMembers(info.signals, "signal", typeAttr(MessageType::signal));
        addMembers(info.properties, "prop", A_DIM);
    }

    for (const auto& error : result->errors) {
        addRow(std::format("{}/error:{}", parentId, error), 2, false)
            .segments.push_back({std::format("⚠ {}", error), colorAttr(errorColor) | A_DIM});
    }
}

void ServiceView::draw(const Rect& area) {
    visibleRows_ = std::max(area.height - 1, 1);
    rebuildRows();

    drawText(area.y, area.x, area.width,
             std::format("Services ({}){}", std::ranges::count(rows_, 0, &Row::depth),
                         hideDisconnected_ ? "  — disconnected hidden" : ""),
             A_BOLD | A_UNDERLINE);

    if (rows_.empty()) {
        drawText(area.y + 2, area.x + 2, area.width - 4, "No service seen yet.", A_DIM);
        return;
    }

    for (int line = 0; line < visibleRows_; ++line) {
        const auto index = cursor_.offset + static_cast<std::size_t>(line);
        if (index >= rows_.size()) break;
        const auto& row = rows_[index];
        const int y = area.y + 1 + line;
        const attr_t base = index == cursor_.selected ? A_REVERSE : A_NORMAL;
        if (base != A_NORMAL) fillLine(y, area.x, area.width, base);

        int x = area.x + row.depth * 2;
        const auto right = area.x + area.width;
        const auto marker = row.expandable ? (row.expanded ? "▾ " : "▸ ") : "  ";
        x += drawText(y, x, right - x, marker, base);
        for (const auto& segment : row.segments) {
            if (x >= right) break;
            x += drawText(y, x, right - x, segment.text, segment.attr | base);
        }
    }
}

void ServiceView::setExpanded(const Row& row, bool expanded) {
    if (expanded)
        expanded_.insert(row.id);
    else
        expanded_.erase(row.id);
}

void ServiceView::selectParent() {
    const auto depth = rows_[cursor_.selected].depth;
    for (auto index = cursor_.selected; index-- > 0;) {
        if (rows_[index].depth < depth) {
            cursor_.selected = index;
            return;
        }
    }
}

bool ServiceView::handleKey(int key) {
    if (key == 'd') {
        hideDisconnected_ = !hideDisconnected_;
        return true;
    }
    if (rows_.empty()) return cursor_.handleKey(key, 0, visibleRows_);

    const auto& row = rows_[cursor_.selected];
    switch (key) {
    case '\n':
    case '\r':
    case KEY_ENTER:
    case ' ':
        if (row.expandable)
            setExpanded(row, !row.expanded);
        else if (!row.memberKey.empty())
            onOpen_(row.service, row.memberKey);
        return true;
    case KEY_RIGHT:
    case 'l':
        if (row.expandable) setExpanded(row, true);
        return true;
    case KEY_LEFT:
    case 'h':
        if (row.expandable && row.expanded)
            setExpanded(row, false);
        else
            selectParent();
        cursor_.clamp(rows_.size(), visibleRows_);
        selectedId_ = rows_[cursor_.selected].id;
        return true;
    default: break;
    }

    if (!cursor_.handleKey(key, rows_.size(), visibleRows_)) return false;
    selectedId_ = rows_[cursor_.selected].id;
    return true;
}

} // namespace ui
