#include "ui/ListCursor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <curses.h>

using ui::ListCursor;

TEST_CASE("moves within bounds") {
    ListCursor cursor;
    CHECK(cursor.handleKey(KEY_UP, 5, 3));
    CHECK(cursor.selected == 0);

    cursor.handleKey(KEY_END, 5, 3);
    CHECK(cursor.selected == 4);
    cursor.handleKey(KEY_DOWN, 5, 3);
    CHECK(cursor.selected == 4);
    CHECK(cursor.isLast(5));

    CHECK_FALSE(cursor.handleKey('x', 5, 3));
}

TEST_CASE("scrolls to keep the selection visible") {
    ListCursor cursor;
    for (int i = 0; i < 6; ++i) cursor.handleKey(KEY_DOWN, 20, 4);
    CHECK(cursor.selected == 6);
    CHECK(cursor.offset == 3);

    cursor.handleKey(KEY_HOME, 20, 4);
    CHECK(cursor.offset == 0);
}

TEST_CASE("pages by the visible height") {
    ListCursor cursor;
    cursor.handleKey(KEY_NPAGE, 100, 10);
    CHECK(cursor.selected == 9);
    cursor.handleKey(KEY_PPAGE, 100, 10);
    CHECK(cursor.selected == 0);
}

TEST_CASE("stays valid when the list shrinks") {
    ListCursor cursor{.selected = 50, .offset = 45};
    cursor.clamp(10, 5);
    CHECK(cursor.selected == 9);
    CHECK(cursor.offset == 5);

    cursor.clamp(0, 5);
    CHECK(cursor.selected == 0);
    CHECK(cursor.offset == 0);
}
