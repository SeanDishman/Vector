#include "vector/model.hpp"
#include <curl/curl.h>
#include <algorithm>
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
    bool limited = false;
};
size_t receive_body(char* data, size_t size, size_t count, void* context) {
    auto& buffer = *static_cast<BodyBuffer*>(context);
    if (buffer.cancel && *buffer.cancel) return 0;
    const size_t bytes = size * count;
    if (bytes > buffer.limit - buffer.body.size()) {
        buffer.limited = true;
        return 0;
    }
    buffer.body.append(data, bytes);
    return bytes;
}
int transfer_progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return *static_cast<std::atomic_bool*>(context) ? 1 : 0;
}
std::string header_line(const std::string& text, const char* name) {
    std::smatch match;
    if (!std::regex_search(text, match, std::regex(std::string("(?:^|\\r?\\n)") + name + R"(:\s*([^\r\n]+))", std::regex::icase))) return {};
    return trim(match[1].str());
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
    return sscanf(value.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) == 4 && a < 256 && b < 256 && c < 256 && d < 256;
}
std::string join_url(const std::string& base, const std::string& path) {
    if (path.empty()) return base;
    if (path.find("://") != std::string::npos) return path;
    if (path.starts_with("//")) {
        const auto scheme_end = base.find("://");
        if (scheme_end == std::string::npos) return "http:" + path;
        return base.substr(0, scheme_end + 1) + path;
    }
    Target root;
    try {
        root = parse_target(base);
    } catch (...) {
        return {};
    }
    if (path.starts_with('/')) {
        Target next = root;
        next.path = path;
        return next.url();
    }
    auto dir = root.path;
    const auto slash = dir.find_last_of('/');
    dir = slash == std::string::npos ? "/" : dir.substr(0, slash + 1);
    Target next = root;
    next.path = dir + path;
    return next.url();
}
struct HttpResult {
    long status = 0;
    std::string body;
    std::string error;
};
HttpResult http_exchange(const std::string& url, const std::string& address, const std::string& method, const std::string& body,
                         const std::vector<std::pair<std::string, std::string>>& headers, int timeout_ms, std::atomic_bool& cancel) {
    HttpResult out;
    if (cancel) {
        out.error = "cancelled";
        return out;
    }
    Target target;
    try {
        target = parse_target(url);
    } catch (const std::exception& e) {
        out.error = e.what();
        return out;
    }
    if (looks_like_ip(target.host) && lower(target.host) != lower(address)) {
        out.error = "UPnP URL points at a different host.";
        return out;
    }
    auto* raw = curl_easy_init();
    if (!raw) {
        out.error = "curl init failed";
        return out;
    }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, curl_easy_cleanup);
    BodyBuffer buffer{{}, &cancel, 65536};
    curl_slist* header_list = nullptr;
    for (const auto& [name, value] : headers)
        header_list = curl_slist_append(header_list, (name + ": " + value).c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> owned_headers(header_list, curl_slist_free_all);
    const auto resolve = target.host + ":" + std::to_string(target.port ? target.port : (target.scheme == "https" ? 443 : 80)) + ":" + address;
    curl_slist* resolve_list = curl_slist_append(nullptr, resolve.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> owned_resolve(resolve_list, curl_slist_free_all);
    curl_easy_setopt(raw, CURLOPT_URL, url.c_str());
    curl_easy_setopt(raw, CURLOPT_PROXY, "");
    curl_easy_setopt(raw, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(raw, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(raw, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(raw, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(raw, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(raw, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::clamp(timeout_ms, 100, 5000)));
    curl_easy_setopt(raw, CURLOPT_TIMEOUT_MS, static_cast<long>(std::clamp(timeout_ms, 100, 5000)));
    curl_easy_setopt(raw, CURLOPT_USERAGENT, "Vector/0.1 (UPnP IGD read)");
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
    if (cancel) {
        out.error = "cancelled";
        return out;
    }
    if (buffer.limited) {
        out.error = "UPnP response exceeded 64 KiB.";
        return out;
    }
    if (code != CURLE_OK) {
        out.error = curl_easy_strerror(code);
        return out;
    }
    out.body = std::move(buffer.body);
    return out;
}
}

std::string ssdp_location_url(const std::string& reply) {
    return header_line(reply, "LOCATION");
}

std::string upnp_service_control_url(const std::string& description_xml, const std::string& base_url, std::string* service_type_out) {
    static const char* types[] = {
        "urn:schemas-upnp-org:service:WANIPConnection:1",
        "urn:schemas-upnp-org:service:WANIPConnection:2",
        "urn:schemas-upnp-org:service:WANPPPConnection:1",
        "urn:schemas-upnp-org:service:WANPPPConnection:2"
    };
    const auto url_base = xml_tag(description_xml, "URLBase");
    const auto root = !url_base.empty() ? url_base : base_url;
    for (const char* wanted : types) {
        const std::string needle = lower(wanted);
        std::regex service_block(R"(<service\b[\s\S]{0,1200}?</service>)", std::regex::icase);
        for (auto it = std::sregex_iterator(description_xml.begin(), description_xml.end(), service_block); it != std::sregex_iterator(); ++it) {
            const auto block = it->str();
            const auto service_type = xml_tag(block, "serviceType");
            if (lower(service_type) != needle) continue;
            const auto control = xml_tag(block, "controlURL");
            if (control.empty()) continue;
            if (service_type_out) *service_type_out = service_type;
            return join_url(root, control);
        }
    }
    return {};
}

std::string upnp_port_mapping_soap(uint16_t index, const std::string& service_type) {
    std::ostringstream body;
    body << "<?xml version=\"1.0\"?>"
         << "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
         << "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
         << "<s:Body>"
         << "<u:GetGenericPortMappingEntry xmlns:u=\"" << service_type << "\">"
         << "<NewPortMappingIndex>" << index << "</NewPortMappingIndex>"
         << "</u:GetGenericPortMappingEntry>"
         << "</s:Body></s:Envelope>";
    return body.str();
}

bool upnp_mapping_from_soap(const std::string& soap, std::string& internal_client, std::string& protocol,
                            std::string& external_port, std::string& internal_port, std::string& description) {
    if (soap.find("SpecifiedArrayIndexInvalid") != std::string::npos || soap.find(">713<") != std::string::npos)
        return false;
    internal_client = xml_tag(soap, "NewInternalClient");
    protocol = xml_tag(soap, "NewProtocol");
    external_port = xml_tag(soap, "NewExternalPort");
    internal_port = xml_tag(soap, "NewInternalPort");
    description = xml_tag(soap, "NewPortMappingDescription");
    return looks_like_ip(internal_client);
}

void enumerate_upnp_nat_map(const std::string& description_xml, const std::string& location_url, ScanReport& report,
                            int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer) {
    if (description_xml.empty() || location_url.empty() || report.address.empty() || cancel) return;
    std::string service_type;
    const auto control = upnp_service_control_url(description_xml, location_url, &service_type);
    if (control.empty() || service_type.empty()) {
        report.notes.push_back("UPnP description had no WANIPConnection/WANPPPConnection control URL; no NAT map to read.");
        return;
    }
    if (observer) observer({ScanStage::Discovery, ActivityState::Running, 0, "UPnP NAT map", "Reading GetGenericPortMappingEntry rows"});
    std::set<std::string> hosts;
    size_t rows = 0;
    constexpr size_t kMaxRows = 24;
    for (uint16_t index = 0; index < kMaxRows && !cancel; ++index) {
        const auto soap = upnp_port_mapping_soap(index, service_type);
        const std::vector<std::pair<std::string, std::string>> headers = {
            {"Content-Type", "text/xml; charset=\"utf-8\""},
            {"SOAPAction", "\"" + service_type + "#GetGenericPortMappingEntry\""}
        };
        auto reply = http_exchange(control, report.address, "POST", soap, headers, timeout_ms, cancel);
        if (cancel) return;
        if (reply.status == 500 || reply.body.find("SpecifiedArrayIndexInvalid") != std::string::npos || reply.body.find(">713<") != std::string::npos)
            break;
        if (reply.status != 200) {
            if (index == 0) report.notes.push_back("UPnP GetGenericPortMappingEntry failed on the first index; NAT map unavailable.");
            break;
        }
        std::string client, protocol, external_port, internal_port, description_text;
        if (!upnp_mapping_from_soap(reply.body, client, protocol, external_port, internal_port, description_text))
            break;
        ++rows;
        hosts.insert(client);
        const auto evidence = printable(protocol + " " + external_port + " -> " + client + ":" + internal_port +
                                            (description_text.empty() ? "" : " (" + description_text + ")"),
                                        300);
        add_finding(report.findings, {client, "LAN host (UPnP)", {}, "Medium", {{"UPnP GetGenericPortMappingEntry | " + control, evidence}}});
    }
    if (observer) observer({ScanStage::Discovery, ActivityState::Complete, 0, "UPnP NAT map",
                            rows ? std::to_string(rows) + " mapping rows; " + std::to_string(hosts.size()) + " internal addresses"
                                 : "No port mappings returned"});
    if (rows)
        report.notes.push_back("UPnP IGD listed " + std::to_string(rows) + " NAT port mapping(s) across " + std::to_string(hosts.size()) +
                               " internal address(es). That is the gateway's published forward table, not a complete subnet scan.");
    else
        report.notes.push_back("UPnP IGD control URL answered but published no port mappings (empty table or unsupported).");
}
}
