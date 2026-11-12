#pragma once
#include "vector/proxy.hpp"
#include "vector/progress.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace vectorapp {
struct Target {
    std::string input, host, scheme, path = "/";
    uint16_t port = 0;
    bool explicit_scheme = false, explicit_port = false, is_ip = false;
    std::string authority() const;
    std::string url() const;
};
struct Evidence {
    std::string source, value;
};
struct Finding {
    std::string name, category, version, confidence;
    std::vector<Evidence> evidence;
};
struct Service {
    uint16_t port = 0;
    std::string state, protocol = "tcp", name = "Unknown", expected, banner, error;
    double latency_ms = 0;
    bool active_probe = false;
};
struct HttpObservation {
    std::string url, address, title, body, error, redirect, http_version;
    long status = 0;
    double elapsed_ms = 0;
    bool body_truncated = false, tls_verified = false;
    std::vector<std::pair<std::string, std::string>> headers, certificate;
};
struct ScriptObservation {
    std::string url, referenced_by, state, reason;
    long status = 0;
    size_t body_bytes = 0;
    bool analyzed = false, body_truncated = false;
    std::string collection = "static";
};
struct BrowserRequest {
    std::string url, page, initiator, type, method, state = "pending", error, address, mime;
    long status = 0;
    size_t transferred_bytes = 0;
    std::string collection = "network";
};
struct BrowserPage {
    std::string url, final_url, state = "not_run", reason, title;
    long status = 0;
    double elapsed_ms = 0;
    bool javascript_enabled = false, dom_truncated = false;
    size_t scripts_analyzed = 0;
    std::string document_source = "network";
    std::string purpose = "endpoint";
};
struct BrowserObservation {
    std::string state = "not_run", engine, reason;
    std::vector<BrowserPage> pages;
    std::vector<BrowserRequest> requests;
    bool requests_truncated = false;
};
struct WebCoverage {
    static constexpr size_t page_limit = 12, script_limit = 40;
    static constexpr int time_limit_ms = 30000;
    size_t page_requests = 0, script_requests = 0;
    double elapsed_ms = 0;
    bool started = false, limited = false;
    std::vector<std::string> reasons;
};
struct VendorRelationship {
    std::string name, category, domain, relationship, evidence_level, confidence;
    std::string capability, attribution_source;
    std::vector<Evidence> evidence;
    std::string published_by = "", recorded_at = "";
};
struct DisclosureSource {
    std::string url, title, state, reason, checked_at;
    long status = 0;
    size_t providers = 0;
    std::string requested_url = "";
    std::string discovered_from = "", discovery_label = "";
    std::vector<Evidence> discovery_chain;
    size_t depth = 0;
};
struct DisclosureCandidate {
    std::string url, discovered_from, label;
    std::vector<Evidence> chain;
    size_t depth = 0;
    int priority = 0;
};
struct RegistryContact {
    std::string role, name, email, phone;
};
struct RegistryRecord {
    std::string scope, source, handle, name, type, country, cidr, range, asn, status, org, abuse_email, description, error;
    std::vector<RegistryContact> contacts;
};
struct ScanOptions {
    std::vector<uint16_t> ports;
    int timeout_ms = 1600;
    int http_timeout_ms = 6000;
    bool web_only = false;
    bool allow_untrusted_tls = false;
    bool browser_runtime = true;
    int browser_timeout_ms = 12000;
    ProxyConfig proxy;
};
struct TcpCoverage {
    std::string profile = "not_started";
    std::vector<uint16_t> selected_ports;
    size_t completed = 0, max_concurrent = 8;
};
struct Geolocation {
    std::string state = "not_checked", source, checked_at, address, city, region, country, country_code, timezone, isp, asn, error;
};
struct IcmpObservation {
    std::string state = "not_checked", address, reply_address, detail;
    double round_trip_ms = 0;
    uint32_t status_code = 0;
    int ttl = 0;
};
struct Reachability {
    std::string state, summary;
    size_t tcp_open = 0, tcp_refused = 0, tcp_no_response = 0, tcp_other = 0, udp_replies = 0;
};
struct DnsRecord {
    std::string name, type, value;
    uint32_t ttl = 0;
};
struct DnsQuery {
    std::string name, type, source, checked_at, state = "not_checked", error;
    int rcode = -1;
    bool truncated = false, authenticated = false;
    std::vector<DnsRecord> records;
};
struct DnsObservation {
    std::string state = "not_checked", reason;
    double elapsed_ms = 0;
    std::vector<DnsQuery> queries;
};
struct FaviconAsset {
    std::string url, content_type;
    std::string body;
};
struct ApiReference {
    std::string kind, method, value, source, detail;
};
struct LinkedPage {
    std::string role, requested_url, url, discovered_from, state, reason;
    int status = 0, requests = 0;
    std::vector<std::string> first_seen;
};
struct ScanReport {
    Target target;
    std::string started_at, finished_at, address, reverse_dns, status = "complete", error;
    std::vector<std::string> addresses, notes;
    std::vector<Service> services;
    std::vector<HttpObservation> web;
    WebCoverage web_coverage;
    std::vector<ScriptObservation> scripts;
    bool script_references_truncated = false;
    BrowserObservation browser;
    std::vector<Finding> findings;
    std::vector<VendorRelationship> vendors;
    std::vector<DisclosureSource> disclosures;
    bool vendors_truncated = false;
    RegistryRecord registry;
    std::vector<ScanEvent> activity;
    double duration_ms = 0;
    Geolocation geolocation;
    IcmpObservation icmp;
    TcpCoverage tcp_coverage;
    DnsObservation dns;
    std::vector<ScriptObservation> stylesheets;
    ProxyObservation proxy;
    FaviconAsset favicon;
    std::vector<ApiReference> api_references;
    bool api_references_truncated = false;
    std::vector<LinkedPage> linked_pages;
};
using Progress = std::function<void(float, const std::string&)>;
Target parse_target(const std::string& input);
bool same_website_host(const Target& original, const Target& destination);
std::vector<uint16_t> parse_ports(const std::string& input);
std::vector<uint16_t> default_ports();
TcpCoverage plan_tcp_scan(const Target& target, const ScanOptions& options);
std::string lower(std::string value);
std::string trim(std::string value);
std::string printable(const std::string& value, size_t limit = 1024);
std::string utc_now();
void identify_http(const HttpObservation& observation, std::vector<Finding>& findings);
std::string ntlm_type1_token();
bool ntlm_offered(const HttpObservation& observation);
bool ntlm_type2_present(const HttpObservation& observation);
bool ntlm_needs_type1(const HttpObservation& observation);
void identify_ntlm(const HttpObservation& observation, std::vector<Finding>& findings);
void identify_vendors(const HttpObservation& observation, ScanReport& report, const std::string& evidence_prefix = {});
void identify_browser_vendor(const BrowserRequest& request, ScanReport& report);
bool disclosure_url_allowed(const Target& subject, const std::string& url);
std::vector<DisclosureCandidate> disclosure_candidates(const HttpObservation& page, const Target& subject, const DisclosureCandidate* parent = nullptr);
std::vector<std::string> disclosure_links(const HttpObservation& page, const Target& subject);
size_t identify_disclosures(const HttpObservation& page, ScanReport& report, const DisclosureCandidate* source = nullptr);
void inspect_browser(const Target& page, ScanReport& report, const ScanOptions& options, std::atomic_bool& cancel, const ScanObserver& observer = {},
                     std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max(),
                     const HttpObservation* collected_page = nullptr,
                     const std::function<void(const HttpObservation&)>& document_observer = {});
void identify_banner(Service& service, std::vector<Finding>& findings);
std::string ssdp_probe(const std::string& address);
std::string ssdp_igd_probe(const std::string& address);
std::string mdns_probe();
std::string snmp_sysdescr_probe();
std::string snmp_sysobjectid_probe();
std::string snmp_sysname_probe();
std::string snmp_getnext_probe(const std::string& dotted_oid);
std::string snmp_v3_discovery_probe();
bool snmp_v3_engine_info(const std::string& reply, unsigned long& enterprise, std::string& engine_hex, std::string& format_data);
std::string udp_exchange(const std::string& address, uint16_t port, const std::string& payload, int timeout_ms, std::atomic_bool& cancel);
std::vector<std::string> udp_responses(const std::string& address, uint16_t port, const std::string& payload, int timeout_ms, std::atomic_bool& cancel, size_t max_replies);
void identify_upnp_type(const std::string& type, const std::string& source, std::vector<Finding>& findings);
void read_udp_reply(const std::string& kind, const std::string& reply, Service& service, std::vector<Finding>& findings,
                    std::vector<std::string>* locations = nullptr);
void absorb_upnp_description(const std::string& xml, const std::string& source, std::vector<Finding>& findings);
void discover_udp(ScanReport& report, int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer = {});
void discover_snmp_router(ScanReport& report, int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer = {});
std::string router_hosts_control_url(const std::string& description_xml, const std::string& description_url, std::string* service_type_out = nullptr);
std::string router_host_soap(int index, const std::string& service_type);
bool router_host_from_soap(const std::string& soap, std::string& address, std::string& mac, std::string& hostname, std::string& active);
void discover_router_hosts(ScanReport& report, int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer = {});
bool udp_loopback_ok();
std::string ssdp_location_url(const std::string& reply);
std::string upnp_service_control_url(const std::string& description_xml, const std::string& base_url, std::string* service_type_out = nullptr);
std::string upnp_port_mapping_soap(uint16_t index, const std::string& service_type);
bool upnp_mapping_from_soap(const std::string& soap, std::string& internal_client, std::string& protocol,
                            std::string& external_port, std::string& internal_port, std::string& description);
void enumerate_upnp_nat_map(const std::string& description_xml, const std::string& location_url, ScanReport& report,
                            int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer = {});
void identify_os_libraries(const std::string& text, const std::string& source, std::vector<Finding>& findings);
void identify_wordpress_plugins(const std::string& text, const std::string& source, std::vector<Finding>& findings);
void identify_hardware(const std::string& text, const std::string& source, std::vector<Finding>& findings);
RegistryRecord parse_rdap_json(const std::string& body);
RegistryRecord lookup_registry(const std::string& address, int timeout_ms, std::atomic_bool& cancel, const ProxyConfig& proxy = {});
bool geolocation_eligible(const std::string& address);
Geolocation parse_geolocation_json(const std::string& body, const std::string& address);
Geolocation lookup_geolocation(const std::string& address, int timeout_ms, std::atomic_bool& cancel, const ProxyConfig& proxy = {});
IcmpObservation inspect_icmp(const std::string& address, std::atomic_bool& cancel);
Reachability summarize_reachability(const ScanReport& report);
DnsObservation inspect_dns(const Target& target, const std::string& address, std::atomic_bool& cancel, const ProxyConfig& proxy = {});
void identify_dns(const DnsObservation& dns, std::vector<Finding>& findings);
void identify_javascript(const std::string& url, const std::string& body, std::vector<Finding>& findings);
void identify_script_reference(const std::string& url, const std::string& source, std::vector<Finding>& findings);
std::vector<std::string> page_script_references(const HttpObservation& page);
std::vector<std::string> page_script_urls(const HttpObservation& page);
std::vector<std::string> page_html_urls(const HttpObservation& page);
std::vector<std::string> page_anchor_urls(const HttpObservation& page);
std::vector<std::string> sitemap_page_urls(const std::string& page_url, const std::string& body);
std::vector<std::string> robots_sitemap_urls(const std::string& page_url, const std::string& body);
void absorb_page_markup(const HttpObservation& page, std::vector<Finding>& findings);
void add_finding(std::vector<Finding>& findings, Finding finding);
ScanReport scan(const Target& target, const ScanOptions& options, std::atomic_bool& cancel, const Progress& progress = {}, const ScanObserver& observer = {});
std::string report_json(const ScanReport& report);
void save_report(const ScanReport& report, const std::string& path);
ScanReport demo_report();
}
