#include "vector/model.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <regex>
#include <set>
#include <string_view>
#include <thread>

namespace vectorapp {
namespace {
using Clock = std::chrono::steady_clock;
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) { closesocket(s); }
int socket_error() { return WSAGetLastError(); }
bool pending(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
struct SocketReady { SocketReady() { WSADATA data{}; WSAStartup(MAKEWORD(2, 2), &data); } };
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
void close_socket(Socket s) { close(s); }
int socket_error() { return errno; }
bool pending(int e) { return e == EWOULDBLOCK || e == EINPROGRESS; }
struct SocketReady { SocketReady() {} };
#endif
struct OwnedSocket {
    Socket value = invalid_socket;
    ~OwnedSocket() { if (value != invalid_socket) close_socket(value); }
};
void set_nonblock(Socket s) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}
bool wait_read(Socket s, Clock::time_point deadline, std::atomic_bool& cancel) {
    while (!cancel && Clock::now() < deadline) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        timeval tv{0, static_cast<long>(std::min<int64_t>(50, std::max<int64_t>(1, remaining)) * 1000)};
        int result = select(static_cast<int>(s + 1), &set, nullptr, nullptr, &tv);
        if (result > 0) return true;
        if (result < 0) return false;
    }
    return false;
}
std::string header_value(const std::string& text, const char* name) {
    std::smatch match;
    if (!std::regex_search(text, match, std::regex(std::string("(?:^|\\r?\\n)") + name + R"(:\s*([^\r\n]+))", std::regex::icase))) return {};
    return trim(match[1].str());
}
std::string snmp_text(const std::string& raw) {
    std::string best;
    for (size_t i = 0; i + 2 < raw.size(); ++i) {
        if (static_cast<unsigned char>(raw[i]) != 0x04) continue;
        const size_t length = static_cast<unsigned char>(raw[i + 1]);
        if (length < 4 || length > 240 || i + 2 + length > raw.size()) continue;
        const auto value = raw.substr(i + 2, length);
        const bool textual = std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c < 127; });
        if (textual && value != "public" && value.size() > best.size()) best = value;
    }
    return best;
}
}
std::string ssdp_probe(const std::string& address) {
    const auto host = address.find(':') == std::string::npos ? address : "[" + address + "]";
    return "M-SEARCH * HTTP/1.1\r\nHOST: " + host + ":1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: ssdp:all\r\n\r\n";
}
std::string ssdp_igd_probe(const std::string& address) {
    const auto host = address.find(':') == std::string::npos ? address : "[" + address + "]";
    return "M-SEARCH * HTTP/1.1\r\nHOST: " + host + ":1900\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\n"
           "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
}
std::string xml_tag(const std::string& xml, const char* tag) {
    const std::regex pattern(std::string("<") + tag + R"((?:\s[^>]*)?>([^<]{1,256})</)" + tag + ">", std::regex::icase);
    std::smatch match;
    if (!std::regex_search(xml, match, pattern)) return {};
    return trim(match[1].str());
}
bool same_endpoint_host(const std::string& location_host, const std::string& inspected) {
    if (location_host.empty() || inspected.empty()) return false;
    if (lower(location_host) == lower(inspected)) return true;
    if (location_host == "localhost" && (inspected == "127.0.0.1" || inspected == "::1")) return true;
    return false;
}
bool private_ipv4_host(const std::string& host) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    char extra = 0;
    if (std::sscanf(host.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    if (a == 10) return true;
    if (a == 127) return true;
    if (a == 192 && b == 168) return true;
    if (a == 172 && b >= 16 && b <= 31) return true;
    if (a == 169 && b == 254) return true;
    return false;
}
size_t curl_append(char* pointer, size_t size, size_t count, void* user) {
    auto* body = static_cast<std::string*>(user);
    const size_t bytes = size * count;
    if (body->size() + bytes > 64 * 1024) return 0;
    body->append(pointer, bytes);
    return bytes;
}
std::string fetch_description_body(const std::string& url, const std::string& connect_ip, int timeout_ms, std::atomic_bool& cancel) {
    if (url.empty() || cancel) return {};
    auto* raw = curl_easy_init();
    if (!raw) return {};
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, curl_easy_cleanup);
    std::string body;
    char error[CURL_ERROR_SIZE]{};
    curl_easy_setopt(raw, CURLOPT_URL, url.c_str());
    curl_easy_setopt(raw, CURLOPT_PROXY, "");
    curl_easy_setopt(raw, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(raw, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(raw, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(raw, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::clamp(timeout_ms, 100, 3000)));
    curl_easy_setopt(raw, CURLOPT_TIMEOUT_MS, static_cast<long>(std::clamp(timeout_ms, 100, 3000)));
    curl_easy_setopt(raw, CURLOPT_USERAGENT, "Vector/0.1 (host fingerprinting)");
    curl_easy_setopt(raw, CURLOPT_ACCEPT_ENCODING, "identity");
    curl_easy_setopt(raw, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(raw, CURLOPT_WRITEFUNCTION, curl_append);
    curl_easy_setopt(raw, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(raw, CURLOPT_ERRORBUFFER, error);
    curl_easy_setopt(raw, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(raw, CURLOPT_XFERINFOFUNCTION, +[](void* client, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
        return static_cast<std::atomic_bool*>(client)->load() ? 1 : 0;
    });
    curl_easy_setopt(raw, CURLOPT_XFERINFODATA, &cancel);
    curl_easy_setopt(raw, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(raw, CURLOPT_SSL_VERIFYHOST, 0L);
    struct curl_slist* connect = nullptr;
    std::string pin;
    if (!connect_ip.empty()) {
        try {
            auto tip = parse_target(url);
            pin = tip.host + ":" + std::to_string(tip.port) + ":" + connect_ip;
            connect = curl_slist_append(connect, pin.c_str());
            curl_easy_setopt(raw, CURLOPT_CONNECT_TO, connect);
        } catch (const std::exception&) {}
    }
    const auto result = curl_easy_perform(raw);
    if (connect) curl_slist_free_all(connect);
    long status = 0;
    curl_easy_getinfo(raw, CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK || status < 200 || status >= 300) return {};
    return body;
}
void absorb_upnp_description(const std::string& xml, const std::string& source, std::vector<Finding>& findings) {
    if (xml.empty()) return;
    struct Field { const char* tag; const char* name; const char* category; const char* confidence; bool versioned; bool hardware; };
    static const Field fields[] = {
        {"deviceType", "UPnP device type", "Device hint", "High", false, false},
        {"friendlyName", "UPnP friendly name", "Device hint", "High", false, true},
        {"manufacturer", "UPnP manufacturer", "Device hint", "High", false, true},
        {"modelName", "UPnP model", "Device hint", "High", false, true},
        {"modelNumber", "UPnP model number", "Device hint", "High", true, false},
        {"modelDescription", "UPnP model description", "Device hint", "Medium", false, true},
        {"serialNumber", "UPnP serial", "Device hint", "Low", false, false},
        {"presentationURL", "UPnP presentation URL", "Service", "Medium", false, false},
    };
    for (const auto& field : fields) {
        const auto value = xml_tag(xml, field.tag);
        if (value.empty()) continue;
        add_finding(findings, {field.name, field.category, field.versioned ? value : std::string{}, field.confidence,
            {{source + " | " + field.tag, printable(value, 200)}}});
        if (field.hardware) identify_hardware(value, source + " | " + field.tag, findings);
        if (std::string_view(field.tag) == "deviceType") identify_upnp_type(value, source + " | deviceType", findings);
    }
}
std::string mdns_probe() {
    std::string packet;
    auto add16 = [&](unsigned value) {
        packet.push_back(static_cast<char>(value >> 8));
        packet.push_back(static_cast<char>(value));
    };
    auto label = [&](const char* text) {
        packet.push_back(static_cast<char>(std::strlen(text)));
        packet += text;
    };
    add16(1); add16(0); add16(1); add16(0); add16(0); add16(0);
    label("_services"); label("_dns-sd"); label("_udp"); label("local");
    packet.push_back(0);
    add16(12); add16(1);
    return packet;
}
std::string snmp_sysdescr_probe() {
    const std::string oid("\x06\x08\x2b\x06\x01\x02\x01\x01\x01\x00", 10);
    auto sequence = [](const std::string& inner) {
        return std::string("\x30", 1) + static_cast<char>(inner.size()) + inner;
    };
    const auto binding = sequence(oid + std::string("\x05\x00", 2));
    const auto binds = sequence(binding);
    const std::string inner = std::string("\x02\x04\x00\x00\x00\x01\x02\x01\x00\x02\x01\x00", 12) + binds;
    const std::string pdu = std::string("\xa0", 1) + static_cast<char>(inner.size()) + inner;
    const auto message = std::string("\x02\x01\x01\x04\x06") + "public" + pdu;
    return std::string("\x30", 1) + static_cast<char>(message.size()) + message;
}
std::vector<std::string> udp_responses(const std::string& address, uint16_t port, const std::string& payload, int timeout_ms, std::atomic_bool& cancel, size_t max_replies) {
    static SocketReady ready;
    if (payload.empty() || cancel || !max_replies) return {};
    max_replies=std::min<size_t>(max_replies,8);
    const bool v6 = address.find(':') != std::string::npos;
    OwnedSocket socket{::socket(v6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
    if (socket.value == invalid_socket) return {};
    set_nonblock(socket.value);
    sockaddr_storage destination{};
    socklen_t length = 0;
    if (!v6) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(&destination);
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &ipv4->sin_addr) != 1) return {};
        length = sizeof(sockaddr_in);
    } else {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(&destination);
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons(port);
        if (inet_pton(AF_INET6, address.c_str(), &ipv6->sin6_addr) != 1) return {};
        length = sizeof(sockaddr_in6);
    }
    const auto deadline = Clock::now() + std::chrono::milliseconds(std::clamp(timeout_ms, 100, 10000));
    int sent = sendto(socket.value, payload.data(), static_cast<int>(payload.size()), 0, reinterpret_cast<sockaddr*>(&destination), length);
    if (sent < 0 && !pending(socket_error())) return {};
    std::vector<std::string> replies;
    size_t datagrams=0;
    while(replies.size()<max_replies && datagrams<32 && wait_read(socket.value,deadline,cancel)) {
        std::string buffer(2048, '\0');
        sockaddr_storage from{};
        socklen_t from_length = sizeof(from);
        int got = recvfrom(socket.value, buffer.data(), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        if(got<0)break;
        ++datagrams;
        const bool same_address=from.ss_family==destination.ss_family && (v6
            ? std::memcmp(&reinterpret_cast<sockaddr_in6*>(&from)->sin6_addr,&reinterpret_cast<sockaddr_in6*>(&destination)->sin6_addr,16)==0
            : reinterpret_cast<sockaddr_in*>(&from)->sin_addr.s_addr==reinterpret_cast<sockaddr_in*>(&destination)->sin_addr.s_addr);
        if(!same_address || !got)continue;
        buffer.resize(static_cast<size_t>(got));
        if(std::find(replies.begin(),replies.end(),buffer)==replies.end())replies.push_back(std::move(buffer));
    }
    return replies;
}
std::string udp_exchange(const std::string& address, uint16_t port, const std::string& payload, int timeout_ms, std::atomic_bool& cancel) {
    auto replies=udp_responses(address,port,payload,timeout_ms,cancel,1);
    return replies.empty()?std::string{}:std::move(replies.front());
}
void read_udp_reply(const std::string& kind, const std::string& reply, Service& service, std::vector<Finding>& findings,
                    std::vector<std::string>* locations) {
    service.protocol = "udp";
    service.active_probe = true;
    service.banner = reply;
    service.state = "open";
    auto source = "UDP " + std::to_string(service.port) + " " + kind;
    auto add = [&](std::string name, std::string category, std::string version, std::string confidence, std::string evidence) {
        add_finding(findings, {std::move(name), std::move(category), std::move(version), std::move(confidence), {{source, printable(evidence, 300)}}});
    };
    if (kind == "ssdp") {
        service.name = "SSDP";
        const auto end=reply.find("\r\n\r\n");
        const auto headers=reply.substr(0,end==std::string::npos?reply.size():end);
        const auto text = printable(reply, 1500);
        add("UPnP / SSDP", "Protocol", {}, "High", text.substr(0, 180));
        const auto server = header_value(reply, "SERVER");
        if (!server.empty()) {
            add("UPnP server", "Service", {}, "High", server);
            identify_hardware(server, source, findings);
        }
        const auto st = header_value(headers, "ST");
        if (!st.empty()) add("UPnP target", "Service", {}, "Medium", st);
        const auto location = header_value(headers, "LOCATION");
        if (!location.empty()) {
            add("UPnP location", "Service", {}, "High", location);
            if (locations && std::find(locations->begin(), locations->end(), location) == locations->end())
                locations->push_back(location);
        }
        static const std::regex success(R"(^HTTP/1\.[01] 200(?:[ \r\n]|$))");
        if(std::regex_search(headers,success))identify_upnp_type(st,source+" | ST",findings);
        if (lower(server).find("linux") != std::string::npos) add("Linux", "OS hint", {}, "Low", server);
        return;
    }
    if (kind == "mdns") {
        service.name = "mDNS";
        const auto text = printable(reply, 1500);
        add("mDNS", "Protocol", {}, "High", text.substr(0, 180));
        std::regex service_type(R"(_([a-z0-9-]{1,32})\._(tcp|udp))", std::regex::icase);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), service_type); it != std::sregex_iterator(); ++it)
            add((*it)[1].str(), "Service", {}, "High", it->str());
        return;
    }
    if (kind == "snmp") {
        const auto descr = snmp_text(reply);
        if (descr.empty()) { service.name = "SNMP"; add("SNMP", "Protocol", {}, "Medium", printable(reply, 120)); return; }
        service.name = "SNMP";
        add("SNMP sysDescr", "Service", {}, "High", descr);
        const auto lowered = lower(descr);
        if (lowered.find("linux") != std::string::npos) add("Linux", "OS hint", {}, "Medium", descr);
        else if (lowered.find("windows") != std::string::npos) add("Windows", "OS hint", {}, "Medium", descr);
        if (lowered.find("routeros") != std::string::npos || lowered.find("mikrotik") != std::string::npos)
            add("MikroTik RouterOS", "Device hint", {}, "Medium", descr);
        else if (lowered.find("cisco ios") != std::string::npos || lowered.find("cisco internetwork") != std::string::npos)
            add("Cisco IOS", "Device hint", {}, "Medium", descr);
        else if (lowered.find("junos") != std::string::npos || lowered.find("juniper") != std::string::npos)
            add("Juniper Junos", "Device hint", {}, "Medium", descr);
        else if (lowered.find("vyos") != std::string::npos || lowered.find("vyatta") != std::string::npos)
            add("VyOS", "Device hint", {}, "Medium", descr);
    }
    identify_hardware(printable(reply, 1500), source, findings);
}
void discover_udp(ScanReport& report, int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer) {
    if (report.address.empty() || cancel) return;
    const int wait = std::min(std::max(timeout_ms, 100), 700);
    struct Item { uint16_t port; const char* kind; const char* expected; std::string payload; };
    Item items[] = {
        {1900, "ssdp", "SSDP", ssdp_probe(report.address)},
        {1900, "ssdp", "SSDP IGD", ssdp_igd_probe(report.address)},
        {5353, "mdns", "mDNS", mdns_probe()},
        {161, "snmp", "SNMP", snmp_sysdescr_probe()}
    };
    bool hit = false;
    std::vector<std::string> locations;
    bool ssdp_service_added = false;
    Service ssdp_service;
    ssdp_service.port = 1900;
    ssdp_service.expected = "SSDP";
    std::vector<Finding> ssdp_findings;
    std::string ssdp_banner;
    for (const auto& item : items) {
        if (cancel) break;
        const std::string operation = std::string(item.expected) + " / UDP " + std::to_string(item.port);
        if (observer) observer({ScanStage::Discovery,ActivityState::Running,0,operation,"Sending identification request; awaiting reply"});
        const bool ssdp=std::string_view(item.kind)=="ssdp";
        auto replies = udp_responses(report.address,item.port,item.payload,ssdp?std::clamp(timeout_ms,100,1500):wait,cancel,ssdp?8:1);
        size_t bytes=0;for(const auto& reply:replies)bytes+=reply.size();
        if (observer) observer({ScanStage::Discovery,cancel ? ActivityState::Cancelled : ActivityState::Complete,0,operation,
            cancel ? "Interrupted by cancellation" : replies.empty() ? "No reply received" : std::to_string(bytes)+" response bytes in "+std::to_string(replies.size())+" datagrams"});
        if (replies.empty()) continue;
        hit = true;
        if (ssdp) {
            for (const auto& reply : replies) {
                read_udp_reply(item.kind, reply, ssdp_service, ssdp_findings, &locations);
                if (!ssdp_banner.empty()) ssdp_banner += "\r\n";
                ssdp_banner += reply;
            }
            ssdp_service_added = true;
            continue;
        }
        Service service;
        service.port = item.port;
        service.expected = item.expected;
        std::vector<Finding> found;
        std::string retained;
        for(const auto& reply:replies) {
            read_udp_reply(item.kind,reply,service,found);
            if(!retained.empty())retained+="\r\n";
            retained+=reply;
        }
        service.banner=std::move(retained);
        for (auto& finding : found) add_finding(report.findings, std::move(finding));
        report.services.push_back(std::move(service));
    }
    if (ssdp_service_added) {
        ssdp_service.banner = std::move(ssdp_banner);
        for (auto& finding : ssdp_findings) add_finding(report.findings, std::move(finding));
        report.services.push_back(std::move(ssdp_service));
    }
    size_t fetched = 0;
    std::set<std::string> seen_locations;
    for (const auto& location : locations) {
        if (cancel || fetched >= 2) break;
        if (!seen_locations.insert(location).second) continue;
        Target tip;
        try { tip = parse_target(location); }
        catch (const std::exception&) {
            add_finding(report.findings, {"UPnP location", "Service", {}, "Low",
                {{"SSDP LOCATION", printable(location, 300)}}});
            continue;
        }
        const bool same_host = same_endpoint_host(tip.host, report.address) ||
            (!report.target.host.empty() && same_endpoint_host(tip.host, report.target.host));
        const bool private_on_public = !same_host && private_ipv4_host(tip.host) &&
            !private_ipv4_host(report.address);
        if (!same_host && !private_on_public) continue;
        if (private_on_public)
            add_finding(report.findings, {"UPnP private location", "Device hint", {}, "Medium",
                {{"SSDP LOCATION", printable(location, 300)},
                 {"connect pin", report.address}}});
        if (observer) observer({ScanStage::Discovery, ActivityState::Running, 0, "UPnP description",
            private_on_public
                ? "Reading device description from private LOCATION via inspected address"
                : "Reading device description from LOCATION"});
        const auto xml = fetch_description_body(location, report.address, timeout_ms, cancel);
        if (observer) observer({ScanStage::Discovery, cancel ? ActivityState::Cancelled : ActivityState::Complete, 0, "UPnP description",
            cancel ? "Interrupted by cancellation" : xml.empty() ? "No description body" : std::to_string(xml.size()) + " description bytes"});
        if (xml.empty()) continue;
        ++fetched;
        absorb_upnp_description(xml, location, report.findings);
        enumerate_upnp_nat_map(xml, location, report, timeout_ms, cancel, observer);
    }
    report.notes.push_back(hit
        ? "UDP replies from SSDP, mDNS, or SNMP were kept. Same-host UPnP LOCATION documents (and private LOCATION hosts pinned to the inspected address) may add device identity. UPnP IGD NAT mappings list hosts that opened ports, not a full subnet scan."
        : "UDP probes for SSDP (1900), mDNS (5353) and SNMP sysDescr (161, community public) got no reply. No reply is not proof those services are closed.");
}
bool udp_loopback_ok() {
    std::atomic_bool stop{false};
    std::atomic_bool listening{false};
    std::atomic<uint16_t> port{0};
    std::thread server([&] {
        static SocketReady ready;
        OwnedSocket socket{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
        if (socket.value == invalid_socket) return;
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        local.sin_port = 0;
        if (bind(socket.value, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) return;
        socklen_t length = sizeof(local);
        if (getsockname(socket.value, reinterpret_cast<sockaddr*>(&local), &length) != 0) return;
        port = ntohs(local.sin_port);
        listening = true;
        set_nonblock(socket.value);
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        if (!wait_read(socket.value, deadline, stop)) return;
        char buffer[512];
        sockaddr_in from{};
        socklen_t from_length = sizeof(from);
        int got = recvfrom(socket.value, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        if (got <= 0) return;
        const char response[] = "HTTP/1.1 200 OK\r\nSERVER: Linux/5.15 UPnP/1.0 VectorTest/1.2\r\nST: upnp:rootdevice\r\n\r\n";
        sendto(socket.value, response, static_cast<int>(sizeof(response) - 1), 0, reinterpret_cast<sockaddr*>(&from), from_length);
    });
    const auto ready_by = Clock::now() + std::chrono::seconds(1);
    while (!listening && Clock::now() < ready_by) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::atomic_bool cancel{false};
    const auto reply = listening ? udp_exchange("127.0.0.1", port.load(), "M-SEARCH * HTTP/1.1\r\n\r\n", 1000, cancel) : std::string{};
    stop = true;
    server.join();
    return reply.find("VectorTest/1.2") != std::string::npos;
}
}
