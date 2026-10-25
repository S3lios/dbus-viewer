#include "Introspector.hpp"

#include <algorithm>
#include <cstring>
#include <format>

namespace {

constexpr std::uint64_t introspectTimeoutUsec = 2'000'000;

std::string attributeValue(std::string_view tag, std::string_view attribute) {
    const auto needle = std::format("{}=", attribute);
    const auto position = tag.find(needle);
    if (position == std::string_view::npos) return {};
    const auto quoteStart = position + needle.size();
    if (quoteStart >= tag.size()) return {};
    const char quote = tag[quoteStart];
    const auto quoteEnd = tag.find(quote, quoteStart + 1);
    if (quoteEnd == std::string_view::npos) return {};
    return std::string{tag.substr(quoteStart + 1, quoteEnd - quoteStart - 1)};
}

} // namespace

std::map<std::string, InterfaceInfo> parseIntrospectionXml(std::string_view xml) {
    std::map<std::string, InterfaceInfo> interfaces;
    InterfaceInfo* current = nullptr;

    for (std::size_t open = xml.find('<'); open != std::string_view::npos;
         open = xml.find('<', open + 1)) {
        const auto close = xml.find('>', open);
        if (close == std::string_view::npos) break;
        const auto tag = xml.substr(open + 1, close - open - 1);
        const auto nameEnd = tag.find_first_of(" \t\r\n/");
        const auto tagName = tag.substr(0, nameEnd);

        if (tagName == "interface") {
            current = &interfaces[attributeValue(tag, "name")];
            if (tag.ends_with('/')) current = nullptr;
        } else if (tagName == "/interface") {
            current = nullptr;
        } else if (current) {
            if (tagName == "method")
                current->methods.insert(attributeValue(tag, "name"));
            else if (tagName == "signal")
                current->signals.insert(attributeValue(tag, "name"));
            else if (tagName == "property")
                current->properties.insert(attributeValue(tag, "name"));
        }
        open = close;
    }
    return interfaces;
}

void Introspector::request(const std::string& service, const std::set<std::string>& paths) {
    auto& result = results_[service];
    for (const auto& path : paths) {
        if (!result.requestedPaths.insert(path).second) continue;

        sd_bus_message* raw = nullptr;
        if (sd_bus_message_new_method_call(client_, &raw, service.c_str(), path.c_str(),
                                           "org.freedesktop.DBus.Introspectable",
                                           "Introspect") < 0)
            continue;
        MessagePtr message{raw};
        // Never start a service just because we looked at it.
        sd_bus_message_set_auto_start(message.get(), 0);

        auto call = std::make_unique<PendingCall>(this, service, path, nullptr);
        sd_bus_slot* slot = nullptr;
        const int r = sd_bus_call_async(client_, &slot, message.get(), &Introspector::onReply,
                                        call.get(), introspectTimeoutUsec);
        if (r < 0) {
            result.errors.push_back(std::format("{}: {}", path, std::strerror(-r)));
            continue;
        }
        call->slot.reset(slot);
        pending_.push_back(std::move(call));
        ++result.pendingCalls;
    }
}

const ServiceIntrospection* Introspector::result(const std::string& service) const {
    const auto it = results_.find(service);
    return it == results_.end() ? nullptr : &it->second;
}

void Introspector::clear() {
    pending_.clear();
    results_.clear();
    changed_ = true;
}

int Introspector::onReply(sd_bus_message* reply, void* userdata, sd_bus_error*) {
    auto* call = static_cast<PendingCall*>(userdata);
    call->owner->finish(call, reply);
    return 1;
}

void Introspector::finish(PendingCall* call, sd_bus_message* reply) {
    auto& result = results_[call->service];
    if (result.pendingCalls > 0) --result.pendingCalls;

    if (const sd_bus_error* error = sd_bus_message_get_error(reply)) {
        result.errors.push_back(
            std::format("{}: {}", call->path, error->message ? error->message : error->name));
    } else {
        const char* xml = nullptr;
        if (sd_bus_message_read(reply, "s", &xml) >= 0 && xml) {
            for (auto& [name, info] : parseIntrospectionXml(xml)) {
                auto& merged = result.interfaces[name];
                merged.methods.merge(info.methods);
                merged.signals.merge(info.signals);
                merged.properties.merge(info.properties);
            }
        }
    }
    changed_ = true;
    // Released later by takeChanged(): the slot must not be freed from within its own callback.
    call->done = true;
}

bool Introspector::takeChanged() {
    std::erase_if(pending_, [](const auto& call) { return call->done; });
    return std::exchange(changed_, false);
}
