#include "ui/Terminal.hpp"

#include <catch2/catch_test_macros.hpp>

using ui::displayWidth;
using ui::fitWidth;

TEST_CASE("measures UTF-8 text in code points") {
    CHECK(displayWidth("") == 0);
    CHECK(displayWidth("abc") == 3);
    CHECK(displayWidth("é→…") == 3);
}

TEST_CASE("pads short text to the requested width") {
    CHECK(fitWidth("ab", 4) == "ab  ");
    CHECK(fitWidth("abcd", 4) == "abcd");
    CHECK(fitWidth("abc", 0).empty());
}

TEST_CASE("truncates long text with an ellipsis") {
    CHECK(fitWidth("abcdef", 4) == "abc…");
    CHECK(displayWidth(fitWidth("abcdef", 4)) == 4);
    // Never cuts a multi-byte character in half.
    CHECK(fitWidth("ééééé", 3) == "éé…");
}

TEST_CASE("replaces control characters") {
    CHECK(fitWidth("a\nb\tc", 5) == "a b c");
}
