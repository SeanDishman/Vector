#pragma once
#include <cstdint>
#include <filesystem>
#include <string>

namespace vectorapp {
struct ProxyConfig {
    bool enabled = false;
    std::string scheme = "http", host, username, password;
    uint16_t port = 8080;
};
struct ProxyObservation {
    bool enabled = false, authenticated = false;
    std::string endpoint;
};
ProxyConfig parse_proxy(const std::string& input);
void validate_proxy(const ProxyConfig& proxy);
std::string proxy_endpoint(const ProxyConfig& proxy);
std::filesystem::path proxy_settings_path();
ProxyConfig load_proxy_settings(const std::filesystem::path& path);
void save_proxy_settings(const ProxyConfig& proxy, const std::filesystem::path& path);
}
