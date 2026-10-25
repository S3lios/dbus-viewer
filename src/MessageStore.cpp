#include "MessageStore.hpp"

#include <format>

namespace {

// Calls never answered (e.g. the caller died) would otherwise accumulate forever.
constexpr std::size_t maxPendingCalls = 100'000;

std::int64_t epochSecond(std::chrono::system_clock::time_point time) {
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

std::string pendingKey(std::string_view caller, std::uint64_t cookie) {
    return std::format("{}#{}", caller, cookie);
}

} // namespace

void RateMeter::add(std::chrono::system_clock::time_point time) {
    const auto second = epochSecond(time);
    auto& bucket = buckets_[static_cast<std::size_t>(second % windowSeconds)];
    if (bucket.second != second) bucket = {second, 0};
    ++bucket.count;
}

double RateMeter::perSecond(std::chrono::system_clock::time_point now) const {
    const auto current = epochSecond(now);
    std::uint64_t count = 0;
    for (const auto& bucket : buckets_)
        if (current - bucket.second < windowSeconds) count += bucket.count;
    return static_cast<double>(count) / windowSeconds;
}

std::optional<double> MemberStats::averageRate(std::chrono::system_clock::time_point now) const {
    const auto span = std::chrono::duration<double>(now - firstSeen).count();
    if (span < 1.0) return std::nullopt;
    return static_cast<double>(total()) / span;
}

MessageStore::MessageStore(std::size_t capacity, const NameRegistry& names)
    : capacity_{capacity}, names_{names} {}

const MessageRecord& MessageStore::add(MessageRecord record) {
    record.id = nextId_++;
    record.senderService = names_.resolve(record.sender);
    record.destinationService =
        record.destination.empty() ? std::string{} : names_.resolve(record.destination);
    record.senderLabel = record.sender.empty() ? std::string{} : names_.label(record.senderService);
    record.destinationLabel =
        record.destination.empty() ? std::string{} : names_.label(record.destinationService);
    // A message addressed to a well-known name keeps that name: it is what the caller used.
    if (!record.destination.empty() && !isUniqueName(record.destination))
        record.destinationLabel = record.destination;
    record.memberKey = resolveMemberKey(record);

    updateStats(record);

    messages_.push_back(std::move(record));
    if (messages_.size() > capacity_) messages_.pop_front();
    return messages_.back();
}

void MessageStore::clear() {
    messages_.clear();
    members_.clear();
    services_.clear();
    pendingCalls_.clear();
    clearedAtId_ = nextId_;
    ++generation_;
}

const MessageRecord* MessageStore::find(std::uint64_t id) const {
    if (id < firstId() || id >= nextId_) return nullptr;
    return &messages_[id - firstId()];
}

std::string MessageStore::resolveMemberKey(const MessageRecord& record) {
    switch (record.type) {
    case MessageType::methodCall: {
        auto key = memberKeyOf(record.interface, record.member);
        if (record.expectsReply) {
            if (pendingCalls_.size() >= maxPendingCalls) pendingCalls_.clear();
            pendingCalls_[pendingKey(record.sender, record.cookie)] = key;
        }
        return key;
    }
    case MessageType::signal: return memberKeyOf(record.interface, record.member);
    case MessageType::methodReturn:
    case MessageType::error: {
        const auto node = pendingCalls_.extract(pendingKey(record.destination, record.replyCookie));
        return node ? std::move(node.mapped()) : std::string{"(unknown call)"};
    }
    case MessageType::unknown: break;
    }
    return "(unknown)";
}

void MessageStore::updateStats(const MessageRecord& record) {
    auto& member = members_[record.memberKey];
    if (member.total() == 0) member.firstSeen = record.time;
    member.lastSeen = record.time;
    member.recentRate.add(record.time);
    switch (record.type) {
    case MessageType::methodCall: ++member.calls; break;
    case MessageType::methodReturn: ++member.returns; break;
    case MessageType::error: ++member.errors; break;
    case MessageType::signal: ++member.signals; break;
    case MessageType::unknown: break;
    }

    const TypedMemberKey typedKey{record.type, record.memberKey};
    if (!record.senderService.empty()) {
        auto& sender = services_[record.senderService];
        sender.sent[typedKey].add(record.time);
        sender.sentTotal.add(record.time);
        if (record.type == MessageType::signal && !record.path.empty())
            sender.objectPaths.insert(record.path);
    }
    if (!record.destinationService.empty()) {
        auto& destination = services_[record.destinationService];
        destination.received[typedKey].add(record.time);
        destination.receivedTotal.add(record.time);
        if (record.type == MessageType::methodCall && !record.path.empty())
            destination.objectPaths.insert(record.path);
    }
}
