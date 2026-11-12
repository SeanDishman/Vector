#pragma once
#include "vector/model.hpp"

namespace vectorapp {
struct ApiEndpoint {
    std::string method, url, host, path, scope, basis, provider, label;
    long status = 0;
    std::vector<Evidence> evidence;
};
struct ApiEndpointList {
    std::vector<ApiEndpoint> rows;
    bool truncated = false;
    static constexpr size_t row_limit = 256, evidence_limit = 8, reference_limit = 768, references_per_source = 64;
};
void collect_api_references(const std::string& script_url, const std::string& code, ScanReport& report, const std::string& label = "Script");
void collect_api_references(const HttpObservation& page, ScanReport& report, const std::string& source_prefix = {});
ApiEndpointList api_endpoint_list(const ScanReport& report);
std::vector<ApiEndpoint> api_endpoints(const ScanReport& report);
std::string api_endpoint_tag(const ApiEndpoint& endpoint);
std::string api_endpoint_meaning(const ApiEndpoint& endpoint);
}
