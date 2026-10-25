#pragma once

#include "MessageRecord.hpp"
#include "ui/Terminal.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ui {

// Filter shared by every view. `text` applies everywhere; `memberKey` and `service`
// are set when drilling down from a grouped view into the message stream.
struct ViewFilter {
    std::string text;
    std::string memberKey;
    std::string service;

    bool empty() const { return text.empty() && memberKey.empty() && service.empty(); }
    std::string signature() const { return text + '\x1f' + memberKey + '\x1f' + service; }
};

inline bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    const auto lower = [](char c) { return std::tolower(static_cast<unsigned char>(c)); };
    return !std::ranges::search(haystack, needle, {}, lower, lower).empty();
}

inline bool matches(const MessageRecord& record, const ViewFilter& filter) {
    if (!filter.memberKey.empty() && record.memberKey != filter.memberKey) return false;
    if (!filter.service.empty() && record.senderService != filter.service &&
        record.destinationService != filter.service)
        return false;
    if (filter.text.empty()) return true;
    for (const std::string_view field :
         {std::string_view{record.senderLabel}, std::string_view{record.destinationLabel},
          std::string_view{record.sender}, std::string_view{record.destination},
          std::string_view{record.path}, std::string_view{record.memberKey},
          std::string_view{record.errorName}, std::string_view{record.argsSummary}})
        if (containsIgnoreCase(field, filter.text)) return true;
    return false;
}

class View {
public:
    virtual ~View() = default;
    virtual std::string_view name() const = 0;
    // Short key reminder shown in the footer.
    virtual std::string_view hints() const = 0;
    virtual void draw(const Rect& area) = 0;
    // Returns true if the key was consumed.
    virtual bool handleKey(int key) = 0;
};

} // namespace ui
