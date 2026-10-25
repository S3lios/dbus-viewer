#include "MessageStore.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;
using Clock = std::chrono::system_clock;

namespace {
const Clock::time_point origin = Clock::time_point{} + 1'000'000s;
}

TEST_CASE("averages over the sliding window") {
    RateMeter meter;
    for (int i = 0; i < 5; ++i) meter.add(origin);
    for (int i = 0; i < 5; ++i) meter.add(origin + 3s);

    CHECK(meter.perSecond(origin + 3s) == Catch::Approx(1.0));
    CHECK(meter.perSecond(origin + 9s) == Catch::Approx(1.0));
}

TEST_CASE("forgets events older than the window") {
    RateMeter meter;
    meter.add(origin);
    meter.add(origin + 5s);

    CHECK(meter.perSecond(origin + 10s) == Catch::Approx(0.1));
    CHECK(meter.perSecond(origin + 15s) == Catch::Approx(0.0));
}

TEST_CASE("reuses a bucket once its second has expired") {
    RateMeter meter;
    for (int i = 0; i < 4; ++i) meter.add(origin);
    // Same bucket index, ten seconds later: the old count must not leak in.
    meter.add(origin + 10s);

    CHECK(meter.perSecond(origin + 10s) == Catch::Approx(0.1));
}

TEST_CASE("computes the average rate since the first occurrence") {
    MemberStats stats;
    stats.firstSeen = origin;
    stats.signals = 20;

    CHECK_FALSE(stats.averageRate(origin + 500ms));
    REQUIRE(stats.averageRate(origin + 10s));
    CHECK(*stats.averageRate(origin + 10s) == Catch::Approx(2.0));
}
