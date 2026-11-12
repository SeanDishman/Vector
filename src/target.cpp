#include "vector/model.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vectorapp {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \r\n\t");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \r\n\t") - first + 1);
}
std::string printable(const std::string& s, size_t limit) {
    std::string out;
    for (unsigned char c : s) {
        if (out.size() >= limit) break;
        if (c >= 32 && c < 127) out += static_cast<char>(c);
        else if (c == '\t' || c == '\n' || c == '\r') out += ' ';
        else out += '.';
    }
    return trim(out);
}
static std::string part(CURLU* u, CURLUPart key) {
    char* value = nullptr;
    if (curl_url_get(u, key, &value, 0) != CURLUE_OK) return {};
    std::string result(value);
    curl_free(value);
    return result;
}
Target parse_target(const std::string& input) {
    Target t;
    t.input = trim(input);
    if (t.input.empty()) throw std::invalid_argument("Enter an IP address, hostname, or HTTP(S) URL.");
    if (t.input.size() > 4096) throw std::invalid_argument("Target is too long (maximum 4096 characters).");
    for (unsigned char c : t.input)
        if (std::isspace(c) || c < 32 || c == 127 || c == '\\')
            throw std::invalid_argument("Target cannot contain whitespace, control characters, or backslashes.");
    t.explicit_scheme = t.input.find("://") != std::string::npos;
    std::string url = t.input;
    if (!t.explicit_scheme) {
        if (std::count(url.begin(), url.end(), ':') > 1 && url[0] != '[' && url.find('/') == std::string::npos)
            url = "[" + url + "]";
        url = "http://" + url;
    }
    std::unique_ptr<CURLU, decltype(&curl_url_cleanup)> u(curl_url(), curl_url_cleanup);
    if (!u || curl_url_set(u.get(), CURLUPART_URL, url.c_str(), 0) != CURLUE_OK)
        throw std::invalid_argument("Invalid target. Try host.example, host:port, or https://host/path.");
    t.scheme = lower(part(u.get(), CURLUPART_SCHEME));
    if (t.scheme != "http" && t.scheme != "https") throw std::invalid_argument("Only HTTP and HTTPS URL schemes are supported. Use a bare host for service inspection.");
    const auto authority_start=url.find("://")+3;
    const auto authority_end=url.find_first_of("/?#",authority_start);
    const auto at=url.find('@',authority_start);
    if (!part(u.get(), CURLUPART_USER).empty() || !part(u.get(), CURLUPART_PASSWORD).empty() ||
        (at!=std::string::npos&&(authority_end==std::string::npos||at<authority_end)))
        throw std::invalid_argument("Credentials in targets are not supported.");
    t.host = lower(part(u.get(), CURLUPART_HOST));
    if (t.host.empty()) throw std::invalid_argument("Target has no hostname.");
    if (t.host.front() == '[' && t.host.back() == ']') t.host = t.host.substr(1, t.host.size() - 2);
    if (t.host.find('%') != std::string::npos || !part(u.get(), CURLUPART_ZONEID).empty())
        throw std::invalid_argument("IPv6 zone identifiers are not supported yet.");
    t.is_ip = t.host.find(':') != std::string::npos || std::all_of(t.host.begin(), t.host.end(), [](unsigned char c) { return std::isdigit(c) || c == '.'; });
    const auto p = part(u.get(), CURLUPART_PORT);
    t.explicit_port = !p.empty();
    if (t.explicit_port) {
        auto n = std::stoul(p);
        if (n == 0 || n > 65535) throw std::invalid_argument("Port must be between 1 and 65535.");
        t.port = static_cast<uint16_t>(n);
    } else t.port = t.scheme == "https" ? 443 : 80;
    if (!t.explicit_scheme && (t.port == 443 || t.port == 8443)) t.scheme = "https";
    t.path = part(u.get(), CURLUPART_PATH);
    const auto query = part(u.get(), CURLUPART_QUERY);
    if (!query.empty()) t.path += "?" + query;
    return t;
}
std::string Target::authority() const {
    return host.find(':') != std::string::npos ? "[" + host + "]" : host;
}
std::string Target::url() const {
    return scheme + "://" + authority() + ":" + std::to_string(port) + path;
}
bool same_website_host(const Target& original, const Target& destination) {
    auto first=lower(original.host), second=lower(destination.host);
    if(first.ends_with('.'))first.pop_back();
    if(second.ends_with('.'))second.pop_back();
    if(first==second)return true;
    if(original.is_ip||destination.is_ip||first.empty()||second.empty())return false;
    return first=="www."+second || second=="www."+first;
}
std::vector<uint16_t> default_ports() {
    return {21,22,23,25,53,80,110,135,139,143,443,445,554,631,993,995,1883,3389,5000,5900,5985,5986,8000,8080,8443,9100};
}
TcpCoverage plan_tcp_scan(const Target& target, const ScanOptions& options) {
    TcpCoverage plan;
    if (!options.ports.empty()) {
        if (options.ports.size() > 256) throw std::invalid_argument("Custom port lists support at most 256 ports.");
        if (std::find(options.ports.begin(),options.ports.end(),0) != options.ports.end())
            throw std::invalid_argument("Ports must be between 1 and 65535.");
        plan.profile = "custom";
        plan.selected_ports = options.ports;
    } else if (options.web_only || target.explicit_port || target.explicit_scheme || target.path != "/") {
        plan.profile = "endpoint";
        plan.selected_ports = {target.port};
    } else if (target.is_ip) {
        plan.profile = "deep_ip";
        plan.max_concurrent = 32;
        plan.selected_ports = default_ports();
        const uint16_t higher[] = {1080,1099,1433,1521,1723,2049,2082,2083,2086,2087,2181,2375,2376,
            3000,3128,3268,3269,3306,3690,4369,5001,5060,5061,5222,5269,5432,5671,5672,5901,6000,
            6379,6443,6667,7001,8008,8009,8081,8088,8090,8181,8888,9000,9042,9090,9092,9200,9300,
            9418,9999,10000,11211,15672,27017,27018,28017,50000};
        plan.selected_ports.insert(plan.selected_ports.end(),std::begin(higher),std::end(higher));
        for (uint16_t port = 1; port <= 1024; ++port) plan.selected_ports.push_back(port);
        std::set<uint16_t> seen;
        std::erase_if(plan.selected_ports,[&](uint16_t port) { return !seen.insert(port).second; });
        return plan;
    } else {
        plan.profile = "common";
        plan.selected_ports = default_ports();
    }
    std::sort(plan.selected_ports.begin(),plan.selected_ports.end());
    plan.selected_ports.erase(std::unique(plan.selected_ports.begin(),plan.selected_ports.end()),plan.selected_ports.end());
    return plan;
}
std::vector<uint16_t> parse_ports(const std::string& input) {
    if (trim(input).empty()) return {};
    std::set<uint16_t> ports;
    std::istringstream stream(input);
    std::string token;
    auto number = [](const std::string& s) -> int {
        if (s.empty() || s.size() > 5 || !std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); }))
            throw std::invalid_argument("Ports must be numbers or ranges, e.g. 22,80,443,8000-8010.");
        int n = std::stoi(s);
        if (n < 1 || n > 65535) throw std::invalid_argument("Ports must be between 1 and 65535.");
        return n;
    };
    if (input.back() == ',') throw std::invalid_argument("Port list ends with an empty entry.");
    while (std::getline(stream, token, ',')) {
        token = trim(token);
        auto dash = token.find('-');
        int start = number(token.substr(0, dash));
        int end = dash == std::string::npos ? start : number(token.substr(dash + 1));
        if (end < start) throw std::invalid_argument("Port range must be ascending.");
        if (end - start >= 256) throw std::invalid_argument("Custom port lists support at most 256 ports.");
        for (int p = start; p <= end; ++p) ports.insert(static_cast<uint16_t>(p));
        if (ports.size() > 256) throw std::invalid_argument("Custom port lists support at most 256 ports.");
    }
    return {ports.begin(), ports.end()};
}
std::string utc_now() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    std::ostringstream s;
    s << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return s.str();
}
}
