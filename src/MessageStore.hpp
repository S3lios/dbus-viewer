#pragma once

#include "MessageRecord.hpp"
#include "NameRegistry.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

// Message rate over a sliding window made of one-second buckets: constant memory
// whatever the traffic, unlike keeping every timestamp.
class RateMeter {
public:
    static constexpr int windowSeconds = 10;

    void add(std::chrono::system_clock::time_point time);
    double perSecond(std::chrono::system_clock::time_point now) const;

private:
    struct Bucket {
        std::int64_t second = -1;
        std::uint32_t count = 0;
    };
    std::array<Bucket, windowSeconds> buckets_{};
};

struct MemberStats {
    std::uint64_t calls = 0;
    std::uint64_t returns = 0;
    std::uint64_t errors = 0;
    std::uint64_t signals = 0;
    std::chrono::system_clock::time_point firstSeen;
    std::chrono::system_clock::time_point lastSeen;
    RateMeter recentRate;

    std::uint64_t total() const { return calls + returns + errors + signals; }
    // Average since the first occurrence; nullopt until the span is long enough to be meaningful.
    std::optional<double> averageRate(std::chrono::system_clock::time_point now) const;
};

using TypedMemberKey = std::pair<MessageType, std::string>;

struct TrafficStats {
    std::uint64_t count = 0;
    RateMeter rate;

    void add(std::chrono::system_clock::time_point time) {
        ++count;
        rate.add(time);
    }
};

struct ServiceStats {
    std::map<TypedMemberKey, TrafficStats> sent;
    std::map<TypedMemberKey, TrafficStats> received;
    TrafficStats sentTotal;
    TrafficStats receivedTotal;
    // Object paths where the service exposes objects (call targets, signal sources).
    std::set<std::string> objectPaths;
};

class MessageStore {
public:
    MessageStore(std::size_t capacity, const NameRegistry& names);

    const MessageRecord& add(MessageRecord record);
    void clear();

    const std::deque<MessageRecord>& messages() const { return messages_; }
    // Id of the oldest kept message; ids are contiguous from there.
    std::uint64_t firstId() const { return nextId_ - messages_.size(); }
    std::uint64_t nextId() const { return nextId_; }
    // Messages captured since the last clear(), including those evicted from the history.
    std::uint64_t capturedCount() const { return nextId_ - clearedAtId_; }
    const MessageRecord* find(std::uint64_t id) const;
    // Incremented on clear() so that views can drop their caches.
    std::uint64_t generation() const { return generation_; }

    const std::map<std::string, MemberStats>& members() const { return members_; }
    const std::map<std::string, ServiceStats>& services() const { return services_; }

private:
    std::string resolveMemberKey(const MessageRecord& record);
    void updateStats(const MessageRecord& record);

    std::size_t capacity_;
    const NameRegistry& names_;
    std::deque<MessageRecord> messages_;
    std::uint64_t nextId_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t clearedAtId_ = 0;

    std::map<std::string, MemberStats> members_;
    std::map<std::string, ServiceStats> services_;
    // "caller#cookie" -> member key, to attribute replies to the call they answer.
    std::unordered_map<std::string, std::string> pendingCalls_;
};
