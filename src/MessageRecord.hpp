#pragma once

#include "Bus.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

enum class MessageType : std::uint8_t { methodCall, methodReturn, error, signal, unknown };

std::string_view shortTypeName(MessageType type);
std::string_view longTypeName(MessageType type);

struct MessageRecord {
    std::uint64_t id = 0;
    std::chrono::system_clock::time_point time;
    MessageType type = MessageType::unknown;
    std::uint64_t cookie = 0;
    std::uint64_t replyCookie = 0;
    bool expectsReply = false;

    std::string sender;
    std::string destination;
    std::string path;
    std::string interface;
    std::string member;
    std::string signature;
    std::string errorName;
    std::string argsSummary;

    // Filled by MessageStore: "interface.member", resolved through reply cookies for returns/errors.
    std::string memberKey;
    // Unique names the message was attributed to (well-known names resolved at reception time).
    std::string senderService;
    std::string destinationService;
    // Human-readable labels captured at reception time, since name ownership changes later.
    std::string senderLabel;
    std::string destinationLabel;

    // Kept to render the full dump on demand.
    MessagePtr raw;
};

MessageRecord makeRecord(sd_bus_message* message);

std::string memberKeyOf(std::string_view interface, std::string_view member);

// Full human-readable dump (header + body) as produced by sd_bus_message_dump.
std::string dumpMessage(sd_bus_message* message);
