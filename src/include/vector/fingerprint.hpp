#pragma once
#include "vector/model.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace vectorapp {
std::string tls_client_hello();
std::string ja4s_from_tls(const std::string& records);
std::string ja4x_from_der(const std::string& der);
std::int32_t favicon_hash(const std::string& bytes);
std::string header_order_fingerprint(const std::vector<std::pair<std::string, std::string>>& headers);
void note_http_signals(const HttpObservation& observation, std::vector<Finding>& findings);
void note_tls_banner(Service& service, std::vector<Finding>& findings);
}
