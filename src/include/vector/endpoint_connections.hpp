#pragma once
#include "vector/model.hpp"

namespace vectorapp {
struct EndpointHost {
    std::string host, relation;
    std::vector<std::string> providers, roles;
    std::vector<Evidence> evidence;
};
std::vector<EndpointHost> endpoint_hosts(const ScanReport& report);
std::vector<std::string> dns_record_providers(const ScanReport& report, const DnsRecord& record);
}
