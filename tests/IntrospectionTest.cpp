#include "Introspector.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parses interfaces and their members") {
    const auto interfaces = parseIntrospectionXml(R"(
        <!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
          "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
        <node>
          <interface name="org.example.Player">
            <method name="Play"/>
            <method name="Seek">
              <arg name="offset" type="x" direction="in"/>
            </method>
            <signal name="Seeked"><arg type="x"/></signal>
            <property name="Volume" type="d" access="readwrite"/>
          </interface>
          <interface name='org.example.Empty'/>
          <node name="child"/>
        </node>)");

    REQUIRE(interfaces.size() == 2);
    const auto& player = interfaces.at("org.example.Player");
    CHECK(player.methods == std::set<std::string>{"Play", "Seek"});
    CHECK(player.signals == std::set<std::string>{"Seeked"});
    CHECK(player.properties == std::set<std::string>{"Volume"});

    const auto& empty = interfaces.at("org.example.Empty");
    CHECK(empty.methods.empty());
}

TEST_CASE("ignores members outside interfaces and malformed input") {
    CHECK(parseIntrospectionXml(R"(<node><method name="Stray"/></node>)").empty());
    CHECK(parseIntrospectionXml("").empty());
    CHECK(parseIntrospectionXml("<interface name=\"unterminated").empty());
}
