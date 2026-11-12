#pragma once
#include "vector/model.hpp"

namespace vectorapp {


DnsQuery parse_dns_json(const std::string& body, const std::string& name, const std::string& type);
DnsObservation collect_dns_records(const std::string& host, std::atomic_bool& cancel,
    const std::string& resolver = "https://dns.google/resolve", int timeout_ms = 3000, const ProxyConfig& proxy = {});
}
