#pragma once

#include "Bus.hpp"

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct InterfaceInfo {
    std::set<std::string> methods;
    std::set<std::string> signals;
    std::set<std::string> properties;
};

struct ServiceIntrospection {
    std::set<std::string> requestedPaths;
    std::size_t pendingCalls = 0;
    std::map<std::string, InterfaceInfo> interfaces;
    std::vector<std::string> errors;
};

// Asynchronously introspects the object paths a service was seen using.
class Introspector {
public:
    explicit Introspector(sd_bus* client) : client_{client} {}

    // Only paths not already requested for this service trigger a call.
    void request(const std::string& service, const std::set<std::string>& paths);
    const ServiceIntrospection* result(const std::string& service) const;
    void clear();

    // Set whenever a reply arrives, so that the UI knows it must redraw.
    bool takeChanged();

private:
    struct PendingCall {
        Introspector* owner;
        std::string service;
        std::string path;
        SlotPtr slot;
        bool done = false;
    };

    static int onReply(sd_bus_message* reply, void* userdata, sd_bus_error* error);
    void finish(PendingCall* call, sd_bus_message* reply);

    sd_bus* client_;
    std::map<std::string, ServiceIntrospection> results_;
    std::list<std::unique_ptr<PendingCall>> pending_;
    bool changed_ = false;
};

std::map<std::string, InterfaceInfo> parseIntrospectionXml(std::string_view xml);
