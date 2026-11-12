#include "vector/model.hpp"
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <memory>
#include <regex>
#include <set>
#include <sstream>

namespace vectorapp {
namespace {
struct BodyBuffer {
    std::string body;
    std::atomic_bool* cancel = nullptr;
    size_t limit = 65536;
};
size_t receive_body(char* data, size_t size, size_t count, void* context) {
    auto& buffer = *static_cast<BodyBuffer*>(context);
    if (buffer.cancel && *buffer.cancel) return 0;
    const size_t bytes = size * count;
    if (buffer.body.size() + bytes > buffer.limit) return 0;
    buffer.body.append(data, bytes);
    return bytes;
}
int transfer_progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return *static_cast<std::atomic_bool*>(context) ? 1 : 0;
}
std::string xml_tag(const std::string& xml, const char* name) {
    std::smatch match;
    const std::regex pattern(std::string("<(?:[a-zA-Z0-9._-]+:)?") + name + R"(\b[^>]*>\s*([^<]{0,256})\s*</(?:[a-zA-Z0-9._-]+:)?)" + name + R"(>)", std::regex::icase);
    if (!std::regex_search(xml, match, pattern)) return {};
    return trim(match[1].str());
}
bool looks_like_ip(const std::string& value) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    char extra = 0;
    return std::sscanf(value.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) == 4 && a < 256 && b < 256 && c < 256 && d < 256;
}
std::string join_url(const std::string& base, const std::string& path) {
    if (path.empty()) return {};
    if (path.find("://") != std::string::npos) return path;
    Target root;
    try { root = parse_target(base); }
    catch (const std::exception&) { return {}; }
    if (path.starts_with('/')) {
        root.path = path;
        return root.url();
    }
    auto dir = root.path;
    const auto slash = dir.find_last_of('/');
    dir = slash == std::string::npos ? "/" : dir.substr(0, slash + 1);
    root.path = dir + path;
    return root.url();
}
struct HttpResult {
    long status = 0;
    std::string body;
    std::string error;
};
HttpResult http_exchange(const std::string& url, const std::string& address, const std::string& method, const std::string& body,
                         const std::vector<std::pair<std::string, std::string>>& headers, int timeout_ms, std::atomic_bool& cancel) {
    HttpResult out;
    if (cancel) { out.error = "cancelled"; return out; }
    Target target;
    try { target = parse_target(url); }
    catch (const std::exception& e) { out.error = e.what(); return out; }
    if (looks_like_ip(target.host) && lower(target.host) != lower(address)) {
        out.error = "host table URL points at a different host";
        return out;
    }
    auto* raw = curl_easy_init();
    if (!raw) { out.error = "curl init failed"; return out; }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, curl_easy_cleanup);
    BodyBuffer buffer{{}, &cancel, 65536};
    curl_slist* header_list = nullptr;
    for (const auto& [name, value] : headers)
        header_list = curl_slist_append(header_list, (name + ": " + value).c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> owned_headers(header_list, curl_slist_free_all);
    const int port = target.port ? target.port : (target.scheme == "https" ? 443 : 80);
    const auto resolve = target.host + ":" + std::to_string(port) + ":" + address;
    curl_slist* resolve_list = curl_slist_append(nullptr, resolve.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> owned_resolve(resolve_list, curl_slist_free_all);
    curl_easy_setopt(raw, CURLOPT_URL, url.c_str());
    curl_easy_setopt(raw, CURLOPT_PROXY, "");
    curl_easy_setopt(raw, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(raw, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(raw, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(raw, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(raw, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(raw, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::clamp(timeout_ms, 200, 3000)));
    curl_easy_setopt(raw, CURLOPT_TIMEOUT_MS, static_cast<long>(std::clamp(timeout_ms, 200, 3000)));
    curl_easy_setopt(raw, CURLOPT_USERAGENT, "Vector/0.1 (router host table)");
    curl_easy_setopt(raw, CURLOPT_ACCEPT_ENCODING, "identity");
    curl_easy_setopt(raw, CURLOPT_WRITEFUNCTION, receive_body);
    curl_easy_setopt(raw, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(raw, CURLOPT_XFERINFOFUNCTION, transfer_progress);
    curl_easy_setopt(raw, CURLOPT_XFERINFODATA, &cancel);
    curl_easy_setopt(raw, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(raw, CURLOPT_RESOLVE, resolve_list);
    if (header_list) curl_easy_setopt(raw, CURLOPT_HTTPHEADER, header_list);
    if (method == "POST") {
        curl_easy_setopt(raw, CURLOPT_POST, 1L);
        curl_easy_setopt(raw, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(raw, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }
    const auto code = curl_easy_perform(raw);
    curl_easy_getinfo(raw, CURLINFO_RESPONSE_CODE, &out.status);
    if (cancel) { out.error = "cancelled"; return out; }
    if (code != CURLE_OK) { out.error = curl_easy_strerror(code); return out; }
    out.body = std::move(buffer.body);
    return out;
}
bool hosts_service_type(const std::string& type) {
    const auto lowered = lower(trim(type));
    return lowered == "urn:dslforum-org:service:hosts:1" || lowered == "urn:dslforum-org:service:hosts:2";
}
struct HostRow {
    std::string address, mac, name, active;
};
bool walk_hosts(const std::string& control, const std::string& service_type, const std::string& address, int timeout_ms,
                std::atomic_bool& cancel, ScanReport& report, const ScanObserver& observer) {
    if (observer) observer({ScanStage::Discovery, ActivityState::Running, 0, "Router host table", "Reading TR-064 GetGenericHostEntry"});
    std::set<std::string> seen;
    bool started = false;
    constexpr int kMax = 32;
    for (int index = 0; index < kMax && !cancel; ++index) {
        const auto soap = router_host_soap(index, service_type);
        const std::vector<std::pair<std::string, std::string>> headers = {
            {"Content-Type", "text/xml; charset=\"utf-8\""},
            {"SOAPAction", "\"" + service_type + "#GetGenericHostEntry\""}
        };
        auto reply = http_exchange(control, address, "POST", soap, headers, timeout_ms, cancel);
        if (cancel) return false;
        HostRow row;
        const bool ok = reply.status == 200 && router_host_from_soap(reply.body, row.address, row.mac, row.name, row.active);
        if (!ok) {
            if (!started && index == 0) continue;
            break;
        }
        started = true;
        if (!seen.insert(row.address).second) continue;
        std::string evidence = row.address;
        if (!row.mac.empty()) evidence += " " + row.mac;
        if (!row.name.empty()) evidence += " " + row.name;
        if (!row.active.empty()) evidence += " active=" + row.active;
        add_finding(report.findings, {row.address, "LAN host (TR-064)", {}, "Medium",
            {{"TR-064 GetGenericHostEntry | " + control, printable(evidence, 240)}}});
    }
    if (observer) observer({ScanStage::Discovery, cancel ? ActivityState::Cancelled : ActivityState::Complete, 0, "Router host table",
        cancel ? "Interrupted by cancellation" : (seen.empty() ? "No host-table rows" : std::to_string(seen.size()) + " host-table row(s)")});
    if (!seen.empty())
        report.notes.push_back("TR-064 Hosts listed " + std::to_string(seen.size()) +
            " address(es) from the gateway host table. That is not a complete subnet sweep.");
    else if (started)
        report.notes.push_back("TR-064 Hosts answered, but published no host-table addresses.");
    else
        report.notes.push_back("TR-064 Hosts control URL did not return GetGenericHostEntry rows.");
    return !seen.empty();
}
}

std::string router_hosts_control_url(const std::string& description_xml, const std::string& description_url, std::string* service_type_out) {
    const auto url_base = xml_tag(description_xml, "URLBase");
    const auto root = url_base.empty() ? description_url : url_base;
    const std::regex service_block(R"(<service\b[\s\S]{0,2000}?</service>)", std::regex::icase);
    for (auto it = std::sregex_iterator(description_xml.begin(), description_xml.end(), service_block); it != std::sregex_iterator(); ++it) {
        const auto block = it->str();
        const auto service_type = xml_tag(block, "serviceType");
        if (!hosts_service_type(service_type)) continue;
        const auto control = xml_tag(block, "controlURL");
        if (control.empty()) continue;
        if (service_type_out) *service_type_out = trim(service_type);
        return join_url(root, control);
    }
    return {};
}

std::string router_host_soap(int index, const std::string& service_type) {
    const auto type = hosts_service_type(service_type) ? trim(service_type) : std::string("urn:dslforum-org:service:Hosts:1");
    std::ostringstream body;
    body << "<?xml version=\"1.0\"?>"
         << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
         << "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
         << "<s:Body><u:GetGenericHostEntry xmlns:u=\"" << type << "\">"
         << "<NewIndex>" << index << "</NewIndex>"
         << "</u:GetGenericHostEntry></s:Body></s:Envelope>";
    return body.str();
}

bool router_host_from_soap(const std::string& soap, std::string& address, std::string& mac, std::string& hostname, std::string& active) {
    address.clear();
    mac.clear();
    hostname.clear();
    active.clear();
    if (soap.find("Fault") != std::string::npos || soap.find("errorCode") != std::string::npos ||
        soap.find("SpecifiedArrayIndexInvalid") != std::string::npos)
        return false;
    address = xml_tag(soap, "NewIPAddress");
    if (!looks_like_ip(address)) return false;
    mac = xml_tag(soap, "NewMACAddress");
    hostname = xml_tag(soap, "NewHostName");
    active = xml_tag(soap, "NewActive");
    return true;
}

void discover_router_hosts(ScanReport& report, int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer) {
    if (report.address.empty() || cancel) return;
    std::vector<std::string> urls;
    auto add_url = [&](const std::string& candidate) {
        const auto url = trim(candidate);
        if (url.empty() || urls.size() >= 3) return;
        if (std::find(urls.begin(), urls.end(), url) != urls.end()) return;
        try {
            const auto target = parse_target(url);
            if (target.scheme != "http" && target.scheme != "https") return;
            if (looks_like_ip(target.host) && lower(target.host) != lower(report.address)) return;
        } catch (const std::exception&) { return; }
        urls.push_back(url);
    };
    for (const auto& service : report.services) {
        if (service.port != 1900 || service.protocol != "udp" || service.banner.empty()) continue;
        const std::regex location_line(R"((?:^|\r?\n)LOCATION:\s*([^\r\n]+))", std::regex::icase);
        for (auto it = std::sregex_iterator(service.banner.begin(), service.banner.end(), location_line); it != std::sregex_iterator(); ++it)
            add_url((*it)[1].str());
    }
    if (urls.empty()) return;

    for (const auto& url : urls) {
        if (cancel) return;
        auto description = http_exchange(url, report.address, "GET", {}, {}, timeout_ms, cancel);
        if (cancel) return;
        if (description.status != 200 || description.body.empty()) continue;
        std::string service_type;
        const auto control = router_hosts_control_url(description.body, url, &service_type);
        if (control.empty() || service_type.empty()) continue;
        walk_hosts(control, service_type, report.address, timeout_ms, cancel, report, observer);
        return;
    }
}
}
