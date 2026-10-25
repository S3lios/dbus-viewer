#include "MessageRecord.hpp"

#include <cstdio>
#include <cstdlib>
#include <format>

namespace {

constexpr std::size_t maxSummaryLength = 240;
constexpr std::size_t maxStringLength = 64;

MessageType typeOf(sd_bus_message* message) {
    std::uint8_t type = 0;
    if (sd_bus_message_get_type(message, &type) < 0) return MessageType::unknown;
    switch (type) {
    case SD_BUS_MESSAGE_METHOD_CALL: return MessageType::methodCall;
    case SD_BUS_MESSAGE_METHOD_RETURN: return MessageType::methodReturn;
    case SD_BUS_MESSAGE_METHOD_ERROR: return MessageType::error;
    case SD_BUS_MESSAGE_SIGNAL: return MessageType::signal;
    default: return MessageType::unknown;
    }
}

void appendQuoted(std::string& out, std::string_view text) {
    out += '"';
    if (text.size() > maxStringLength) {
        out += text.substr(0, maxStringLength);
        out += "…";
    } else {
        out += text;
    }
    out += '"';
}

bool appendBasic(sd_bus_message* message, char type, std::string& out) {
    union {
        std::uint8_t byte;
        int boolean;
        std::int16_t int16;
        std::uint16_t uint16;
        std::int32_t int32;
        std::uint32_t uint32;
        std::int64_t int64;
        std::uint64_t uint64;
        double floating;
        const char* string;
        int fd;
    } value{};
    if (sd_bus_message_read_basic(message, type, &value) <= 0) return false;

    switch (type) {
    case SD_BUS_TYPE_BYTE: out += std::to_string(value.byte); break;
    case SD_BUS_TYPE_BOOLEAN: out += value.boolean ? "true" : "false"; break;
    case SD_BUS_TYPE_INT16: out += std::to_string(value.int16); break;
    case SD_BUS_TYPE_UINT16: out += std::to_string(value.uint16); break;
    case SD_BUS_TYPE_INT32: out += std::to_string(value.int32); break;
    case SD_BUS_TYPE_UINT32: out += std::to_string(value.uint32); break;
    case SD_BUS_TYPE_INT64: out += std::to_string(value.int64); break;
    case SD_BUS_TYPE_UINT64: out += std::to_string(value.uint64); break;
    case SD_BUS_TYPE_DOUBLE: out += std::format("{}", value.floating); break;
    case SD_BUS_TYPE_UNIX_FD: out += std::format("fd:{}", value.fd); break;
    case SD_BUS_TYPE_OBJECT_PATH:
    case SD_BUS_TYPE_SIGNATURE: out += safeString(value.string); break;
    default: appendQuoted(out, safeString(value.string)); break;
    }
    return true;
}

bool appendValues(sd_bus_message* message, std::string& out, std::string_view separator);

bool appendContainer(sd_bus_message* message, char type, const char* contents, std::string& out) {
    if (sd_bus_message_enter_container(message, type, contents) <= 0) return false;

    const bool isDict = type == SD_BUS_TYPE_ARRAY && contents && contents[0] == '{';
    switch (type) {
    case SD_BUS_TYPE_ARRAY: out += isDict ? "{" : "["; break;
    case SD_BUS_TYPE_STRUCT: out += "("; break;
    default: break;
    }

    const bool complete = appendValues(message, out, type == SD_BUS_TYPE_DICT_ENTRY ? ": " : ", ");

    switch (type) {
    case SD_BUS_TYPE_ARRAY: out += isDict ? "}" : "]"; break;
    case SD_BUS_TYPE_STRUCT: out += ")"; break;
    default: break;
    }
    return complete && sd_bus_message_exit_container(message) >= 0;
}

// Returns false once the summary is long enough (or on parse error) to stop walking.
bool appendValues(sd_bus_message* message, std::string& out, std::string_view separator) {
    bool first = true;
    for (;;) {
        if (out.size() > maxSummaryLength) return false;

        char type = 0;
        const char* contents = nullptr;
        const int r = sd_bus_message_peek_type(message, &type, &contents);
        if (r < 0) return false;
        if (r == 0) return true;

        if (!first) out += separator;
        first = false;

        const bool isContainer = type == SD_BUS_TYPE_ARRAY || type == SD_BUS_TYPE_VARIANT ||
                                 type == SD_BUS_TYPE_STRUCT || type == SD_BUS_TYPE_DICT_ENTRY;
        const bool ok = isContainer ? appendContainer(message, type, contents, out)
                                    : appendBasic(message, type, out);
        if (!ok) return false;
    }
}

std::string summarizeArgs(sd_bus_message* message) {
    std::string out;
    if (sd_bus_message_rewind(message, 1) < 0) return out;
    if (!appendValues(message, out, ", ") && out.size() > maxSummaryLength) out += " …";
    sd_bus_message_rewind(message, 1);
    return out;
}

} // namespace

std::string_view shortTypeName(MessageType type) {
    switch (type) {
    case MessageType::methodCall: return "call";
    case MessageType::methodReturn: return "ret";
    case MessageType::error: return "err";
    case MessageType::signal: return "sig";
    case MessageType::unknown: break;
    }
    return "?";
}

std::string_view longTypeName(MessageType type) {
    switch (type) {
    case MessageType::methodCall: return "method call";
    case MessageType::methodReturn: return "method return";
    case MessageType::error: return "error";
    case MessageType::signal: return "signal";
    case MessageType::unknown: break;
    }
    return "unknown";
}

std::string memberKeyOf(std::string_view interface, std::string_view member) {
    if (interface.empty()) return std::string{member};
    return std::format("{}.{}", interface, member);
}

MessageRecord makeRecord(sd_bus_message* message) {
    MessageRecord record;
    record.time = std::chrono::system_clock::now();
    record.type = typeOf(message);
    sd_bus_message_get_cookie(message, &record.cookie);
    sd_bus_message_get_reply_cookie(message, &record.replyCookie);
    record.expectsReply = record.type == MessageType::methodCall &&
                          sd_bus_message_get_expect_reply(message) > 0;

    record.sender = safeString(sd_bus_message_get_sender(message));
    record.destination = safeString(sd_bus_message_get_destination(message));
    record.path = safeString(sd_bus_message_get_path(message));
    record.interface = safeString(sd_bus_message_get_interface(message));
    record.member = safeString(sd_bus_message_get_member(message));
    record.signature = safeString(sd_bus_message_get_signature(message, 1));
    if (const sd_bus_error* error = sd_bus_message_get_error(message))
        record.errorName = safeString(error->name);

    record.argsSummary = summarizeArgs(message);
    record.raw.reset(sd_bus_message_ref(message));
    return record;
}

std::string dumpMessage(sd_bus_message* message) {
    char* buffer = nullptr;
    std::size_t size = 0;
    FILE* stream = open_memstream(&buffer, &size);
    if (!stream) return "cannot allocate dump buffer";

    sd_bus_message_dump(message, stream, SD_BUS_MESSAGE_DUMP_WITH_HEADER);
    std::fclose(stream);

    std::string text{buffer, size};
    std::free(buffer);
    sd_bus_message_rewind(message, 1);
    return text;
}
