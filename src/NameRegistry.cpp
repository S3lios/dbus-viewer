#include "NameRegistry.hpp"

#include "Bus.hpp"

#include <cstdlib>

void NameRegistry::loadFrom(sd_bus* client) {
    sd_bus_message* reply = nullptr;
    if (sd_bus_call_method(client, busDaemonName, "/org/freedesktop/DBus", "org.freedesktop.DBus",
                           "ListNames", nullptr, &reply, "") < 0)
        return;
    MessagePtr replyGuard{reply};

    char** names = nullptr;
    if (sd_bus_message_read_strv(reply, &names) < 0) return;

    for (char** name = names; name && *name; ++name) {
        const std::string current{*name};
        if (!isUniqueName(current) && current != busDaemonName) {
            sd_bus_message* ownerReply = nullptr;
            if (sd_bus_call_method(client, busDaemonName, "/org/freedesktop/DBus",
                                   "org.freedesktop.DBus", "GetNameOwner", nullptr, &ownerReply,
                                   "s", current.c_str()) >= 0) {
                MessagePtr ownerGuard{ownerReply};
                const char* owner = nullptr;
                if (sd_bus_message_read(ownerReply, "s", &owner) >= 0 && owner)
                    onNameOwnerChanged(current, owner);
            }
        }
        std::free(*name);
    }
    std::free(names);
}

void NameRegistry::onNameOwnerChanged(const std::string& name, const std::string& newOwner) {
    if (isUniqueName(name)) {
        if (newOwner.empty())
            disconnected_.insert(name);
        else
            disconnected_.erase(name);
        return;
    }

    if (newOwner.empty())
        ownerOf_.erase(name);
    else {
        ownerOf_[name] = newOwner;
        namesOf_[newOwner].insert(name);
    }
}

std::string NameRegistry::resolve(std::string_view name) const {
    if (isUniqueName(name)) return std::string{name};
    if (const auto it = ownerOf_.find(std::string{name}); it != ownerOf_.end()) return it->second;
    return std::string{name};
}

std::string NameRegistry::label(const std::string& unique) const {
    if (const auto it = namesOf_.find(unique); it != namesOf_.end() && !it->second.empty())
        return *it->second.begin();
    return unique;
}

std::set<std::string> NameRegistry::namesOf(const std::string& unique) const {
    if (const auto it = namesOf_.find(unique); it != namesOf_.end()) return it->second;
    return {};
}

bool NameRegistry::isDisconnected(const std::string& unique) const {
    return disconnected_.contains(unique);
}
