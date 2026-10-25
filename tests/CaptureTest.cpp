// Runs against a private bus started by dbus-run-session (see tests/CMakeLists.txt).
#include "Capture.hpp"
#include "Bus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace std::chrono_literals;

constexpr const char* serviceName = "org.example.TestService";
constexpr const char* interfaceName = "org.example.Test";

BusPtr connect() {
    auto bus = openClientBus(resolveBusAddress(Options{}));
    REQUIRE(bus);
    return std::move(*bus);
}

// Minimal service answering Echo(s) -> s on /test.
int onServiceCall(sd_bus_message* message, void*, sd_bus_error*) {
    if (sd_bus_message_is_method_call(message, interfaceName, "Echo") <= 0) return 0;
    const char* text = nullptr;
    sd_bus_message_read(message, "s", &text);
    return sd_bus_reply_method_return(message, "s", text);
}

// A null callback would send the call with NO_REPLY_EXPECTED, and the service would not answer.
int ignoreReply(sd_bus_message*, void*, sd_bus_error*) {
    return 0;
}

struct Fixture {
    NameRegistry names;
    MessageStore store{1000, names};
    BusPtr monitor;
    std::optional<Capture> capture;

    explicit Fixture(const std::vector<std::string>& rules = {}) {
        auto bus = openMonitorBus(resolveBusAddress(Options{}), rules);
        REQUIRE(bus);
        monitor = std::move(*bus);
        capture.emplace(names, store, uniqueName(monitor.get()), std::string{});
    }

    // Drives every connection until `done` holds; all of them live in this single thread.
    void pumpUntil(const std::vector<sd_bus*>& peers, const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!done()) {
            REQUIRE(std::chrono::steady_clock::now() < deadline);
            for (sd_bus* peer : peers)
                while (sd_bus_process(peer, nullptr) > 0) {
                }
            for (;;) {
                sd_bus_message* raw = nullptr;
                const int r = sd_bus_process(monitor.get(), &raw);
                MessagePtr message{raw};
                REQUIRE(r >= 0);
                if (message) capture->handle(message.get());
                if (r == 0) break;
            }
            sd_bus_wait(monitor.get(), 10'000);
        }
    }

    const MessageRecord* findRecord(MessageType type, const std::string& memberKey) const {
        for (const auto& record : store.messages())
            if (record.type == type && record.memberKey == memberKey) return &record;
        return nullptr;
    }
};

struct Service {
    BusPtr bus = connect();
    SlotPtr object;
    std::string unique = uniqueName(bus.get());

    Service() {
        sd_bus_slot* slot = nullptr;
        REQUIRE(sd_bus_add_object(bus.get(), &slot, "/test", onServiceCall, nullptr) >= 0);
        object.reset(slot);
        REQUIRE(sd_bus_request_name(bus.get(), serviceName, 0) >= 0);
    }
};

} // namespace

TEST_CASE("captures a call, its reply and attributes both to the service") {
    Fixture fixture;
    Service service;
    auto caller = connect();

    REQUIRE(sd_bus_call_method_async(caller.get(), nullptr, serviceName, "/test", interfaceName,
                                     "Echo", ignoreReply, nullptr, "s", "hello") >= 0);

    const auto key = std::string{interfaceName} + ".Echo";
    fixture.pumpUntil({service.bus.get(), caller.get()},
                      [&] { return fixture.findRecord(MessageType::methodReturn, key); });

    const auto* call = fixture.findRecord(MessageType::methodCall, key);
    REQUIRE(call);
    CHECK(call->destinationLabel == serviceName);
    CHECK(call->destinationService == service.unique);
    CHECK(call->path == "/test");
    CHECK(call->argsSummary == "\"hello\"");

    // The reply carries no member: it must be matched to the call through its reply cookie.
    const auto* reply = fixture.findRecord(MessageType::methodReturn, key);
    REQUIRE(reply);
    CHECK(reply->senderService == service.unique);
    CHECK(reply->senderLabel == serviceName);

    const auto& member = fixture.store.members().at(key);
    CHECK(member.calls == 1);
    CHECK(member.returns == 1);

    const auto& stats = fixture.store.services().at(service.unique);
    CHECK(stats.received.contains({MessageType::methodCall, key}));
    CHECK(stats.sent.contains({MessageType::methodReturn, key}));
    CHECK(stats.objectPaths.contains("/test"));
}

TEST_CASE("captures signals with their arguments") {
    Fixture fixture;
    Service service;

    REQUIRE(sd_bus_emit_signal(service.bus.get(), "/test", interfaceName, "Tick", "ias", 7, 2, "a",
                               "b") >= 0);

    const auto key = std::string{interfaceName} + ".Tick";
    fixture.pumpUntil({service.bus.get()},
                      [&] { return fixture.findRecord(MessageType::signal, key); });

    const auto* signal = fixture.findRecord(MessageType::signal, key);
    REQUIRE(signal);
    CHECK(signal->argsSummary == R"(7, ["a", "b"])");
    CHECK(signal->destination.empty());
}

TEST_CASE("lets the daemon filter with match rules") {
    Fixture fixture{{"type='signal',interface='org.example.Test'"}};
    Service service;
    auto caller = connect();

    REQUIRE(sd_bus_call_method_async(caller.get(), nullptr, serviceName, "/test", interfaceName,
                                     "Echo", ignoreReply, nullptr, "s", "ignored") >= 0);
    REQUIRE(sd_bus_emit_signal(service.bus.get(), "/test", interfaceName, "Tick", "i", 1) >= 0);

    const auto key = std::string{interfaceName} + ".Tick";
    fixture.pumpUntil({service.bus.get(), caller.get()},
                      [&] { return fixture.findRecord(MessageType::signal, key); });

    for (const auto& record : fixture.store.messages()) {
        CHECK(record.type == MessageType::signal);
        CHECK(record.interface == interfaceName);
    }
}
