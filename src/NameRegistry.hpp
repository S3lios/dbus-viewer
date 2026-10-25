#pragma once

#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

struct sd_bus;

// Tracks which unique connection name (":1.42") owns which well-known names,
// so that messages can be attributed to a connection whatever name they use.
class NameRegistry {
public:
    // Seeds the registry with the names currently present on the bus.
    void loadFrom(sd_bus* client);

    void onNameOwnerChanged(const std::string& name, const std::string& newOwner);

    // Maps a well-known name to its current owner; unknown or unique names are returned unchanged.
    std::string resolve(std::string_view name) const;

    // "org.example.Foo" (or the unique name if it never owned a well-known name).
    std::string label(const std::string& unique) const;
    // Every well-known name currently or formerly owned by the connection.
    std::set<std::string> namesOf(const std::string& unique) const;
    bool isDisconnected(const std::string& unique) const;

private:
    std::unordered_map<std::string, std::string> ownerOf_;
    // Never shrinks: keeps labels meaningful for connections that went away.
    std::unordered_map<std::string, std::set<std::string>> namesOf_;
    std::unordered_set<std::string> disconnected_;
};

inline bool isUniqueName(std::string_view name) {
    return name.starts_with(':');
}
