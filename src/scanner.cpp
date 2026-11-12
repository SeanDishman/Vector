#include "vector/fingerprint.hpp"
#include "vector/model.hpp"
#include "vector/api_endpoints.hpp"
#include "proxy_transport.hpp"
#include "asset_scope.hpp"
#include "crawl_priority.hpp"
#include "linked_page.hpp"
#include "frontend.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <curl/curl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <stdexcept>
#include <thread>

namespace vectorapp {
namespace {
using Clock = std::chrono::steady_clock;
#ifdef _WIN32
using Socket = SOCKET;
using SocketLength = int;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) { closesocket(s); }
int socket_error() { return WSAGetLastError(); }
bool pending(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
bool refused(int e) { return e == WSAECONNREFUSED; }
#else
using Socket = int;
using SocketLength = socklen_t;
constexpr Socket invalid_socket = -1;
void close_socket(Socket s) { close(s); }
int socket_error() { return errno; }
bool pending(int e) { return e == EINPROGRESS || e == EWOULDBLOCK; }
bool refused(int e) { return e == ECONNREFUSED; }
#endif
struct OwnedSocket {
    Socket value = invalid_socket;
    ~OwnedSocket() { if (value != invalid_socket) close_socket(value); }
};
struct Runtime {
    Runtime() {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2,2), &data) != 0) throw std::runtime_error("Cannot initialize Winsock.");
#endif
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw std::runtime_error("Cannot initialize libcurl.");
    }
    ~Runtime() {
        curl_global_cleanup();
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
struct Address {
    sockaddr_storage socket{};
    int length = 0;
    std::string text;
};
std::vector<Address> resolve(const std::string& host) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* raw = nullptr;
    int error = getaddrinfo(host.c_str(), nullptr, &hints, &raw);
    if (error || !raw) throw std::runtime_error("DNS lookup failed for " + host + ". Check the address and your connection.");
    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> info(raw, freeaddrinfo);
    std::vector<Address> addresses;
    for (auto* ai = raw; ai; ai = ai->ai_next) {
        if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) continue;
        Address a;
        a.length = static_cast<int>(ai->ai_addrlen);
        std::memcpy(&a.socket, ai->ai_addr, ai->ai_addrlen);
        char text[NI_MAXHOST]{};
        if (getnameinfo(ai->ai_addr, static_cast<SocketLength>(ai->ai_addrlen), text, sizeof(text), nullptr, 0, NI_NUMERICHOST)) continue;
        a.text = text;
        if (std::none_of(addresses.begin(), addresses.end(), [&](const Address& old) { return old.text == a.text; })) addresses.push_back(a);
    }
    std::stable_sort(addresses.begin(), addresses.end(), [](const Address& a, const Address& b) { return a.socket.ss_family == AF_INET && b.socket.ss_family != AF_INET; });
    if (addresses.empty()) throw std::runtime_error("No usable IPv4 or IPv6 address was returned.");
    return addresses;
}
bool wait_socket(Socket s, bool reading, Clock::time_point deadline, std::atomic_bool& cancel) {
    while (!cancel && Clock::now() < deadline) {
        fd_set set, errors;
        FD_ZERO(&set); FD_SET(s, &set);
        FD_ZERO(&errors); FD_SET(s, &errors);
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        timeval tv{0, static_cast<long>(std::min<int64_t>(50, std::max<int64_t>(1,remaining)) * 1000)};
        int result = select(static_cast<int>(s + 1), reading ? &set : nullptr, reading ? nullptr : &set, &errors, &tv);
        if (result > 0) return true;
        if (result < 0) return false;
    }
    return false;
}
std::string expected_service(uint16_t p) {
    static const std::map<int,std::string> names = {{21,"FTP"},{22,"SSH"},{23,"Telnet"},{25,"SMTP"},{53,"DNS (TCP)"},{80,"HTTP"},{110,"POP3"},{135,"MS RPC"},{139,"NetBIOS"},{143,"IMAP"},{443,"HTTPS"},{445,"SMB"},{554,"RTSP"},{631,"IPP / HTTP"},{993,"IMAPS"},{995,"POP3S"},{1883,"MQTT"},{3389,"RDP"},{5000,"HTTP / vendor service"},{5900,"VNC"},{5985,"WinRM HTTP"},{5986,"WinRM HTTPS"},{8000,"HTTP"},{8080,"HTTP"},{8443,"HTTPS"},{9100,"Printer"}};
    auto it = names.find(p);
    return it == names.end() ? "Unknown" : it->second;
}
bool web_port(uint16_t p) { return p == 80 || p == 443 || p == 631 || p == 5000 || p == 5985 || p == 5986 || p == 8000 || p == 8008 || p == 8080 || p == 8081 || p == 8443 || p == 8888; }
bool send_all(Socket s, const char* data, size_t n, Clock::time_point deadline, std::atomic_bool& cancel) {
    size_t sent = 0;
    while (sent < n && !cancel && Clock::now() < deadline) {
        if (!wait_socket(s, false, deadline, cancel)) return false;
        int result = send(s, data + sent, static_cast<int>(n - sent), 0);
        if (result > 0) sent += static_cast<size_t>(result);
        else if (!pending(socket_error())) return false;
    }
    return sent == n;
}
std::string recv_some(Socket s, Clock::time_point deadline, std::atomic_bool& cancel) {
    if (!wait_socket(s, true, deadline, cancel)) return {};
    std::array<char, 4096> buffer{};
    auto n = recv(s, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (n <= 0) return {};
    return std::string(buffer.data(), static_cast<size_t>(n));
}
enum class ProbeKind { None, Http, Smb, Rdp, Tls, Redis, Postgres, Memcached, Mqtt, Amqp, Dns, Ssh, Smtp, Ftp, Rtsp };
ProbeKind probe_kind(uint16_t port) {
    switch (port) {
        case 21: return ProbeKind::Ftp;
        case 22: return ProbeKind::Ssh;
        case 25: return ProbeKind::Smtp;
        case 53: return ProbeKind::Dns;
        case 139: case 445: return ProbeKind::Smb;
        case 554: return ProbeKind::Rtsp;
        case 465: case 636: case 989: case 990: case 992: case 993: case 995: return ProbeKind::Tls;
        case 1883: return ProbeKind::Mqtt;
        case 3389: return ProbeKind::Rdp;
        case 5432: return ProbeKind::Postgres;
        case 5672: return ProbeKind::Amqp;
        case 6379: return ProbeKind::Redis;
        case 11211: return ProbeKind::Memcached;
        default: return ProbeKind::Http;
    }
}
std::string probe_payload(ProbeKind kind) {
    switch (kind) {
        case ProbeKind::Http: return "GET / HTTP/1.0\r\nHost: vector\r\n\r\n";
        case ProbeKind::Ssh: return "SSH-2.0-Vector_0.1\r\n";
        case ProbeKind::Smtp: return "EHLO vector\r\n";
        case ProbeKind::Ftp: return "HELP\r\n";
        case ProbeKind::Rtsp: return "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\n\r\n";
        case ProbeKind::Redis: return "PING\r\n";
        case ProbeKind::Memcached: return "version\r\n";
        case ProbeKind::Amqp: return std::string("AMQP\x00\x00\x09\x01", 8);
        case ProbeKind::Postgres: return std::string("\x00\x00\x00\x08\x04\xd2\x16\x2f", 8);
        case ProbeKind::Mqtt: return std::string("\x10\x12\x00\x04MQTT\x04\x02\x00\x3c\x00\x06vector", 20);
        case ProbeKind::Tls: return tls_client_hello();
        case ProbeKind::Rdp: return std::string("\x03\x00\x00\x13\x0e\xe0\x00\x00\x00\x00\x00\x01\x00\x08\x00\x03\x00\x00\x00", 19);
        case ProbeKind::Smb: {
            std::string packet(104, '\0');
            const unsigned char prefix[] = {0xfe,'S','M','B',0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01};
            std::memcpy(packet.data(), prefix, sizeof(prefix));
            packet[64] = 36; packet[66] = 2; packet[68] = 1;
            packet[100] = 0x02; packet[101] = 0x02; packet[102] = 0x10; packet[103] = 0x02;
            return packet;
        }
        case ProbeKind::Dns: {
            std::string body;
            auto add16 = [&](unsigned v) { body.push_back(static_cast<char>(v >> 8)); body.push_back(static_cast<char>(v)); };
            add16(0x1234); add16(0x0100); add16(1); add16(0); add16(0); add16(0);
            body.push_back(6); body += "vector"; body.push_back(7); body += "invalid"; body.push_back(0);
            add16(1); add16(1);
            std::string packet;
            packet.push_back(static_cast<char>(body.size() >> 8));
            packet.push_back(static_cast<char>(body.size()));
            return packet + body;
        }
        case ProbeKind::None: return {};
    }
    return {};
}
std::string label_reply(ProbeKind kind, const std::string& reply) {
    if (reply.empty()) return {};
    if (kind == ProbeKind::Postgres && (reply == "S" || reply == "N")) return "PostgreSQL SSLRequest " + reply;
    if (kind == ProbeKind::Mqtt && static_cast<unsigned char>(reply[0]) == 0x20) return "MQTT CONNACK";
    if (kind == ProbeKind::Dns && reply.size() >= 4) return "DNS TCP response " + reply;
    return reply;
}
Service probe(Address address, uint16_t port, int timeout, std::atomic_bool& cancel, bool http_planned, const std::function<void(const std::string&)>& activity) {
    Service s;
    s.port = port; s.expected = expected_service(port); s.state = "cancelled";
    if (cancel) return s;
    if (address.socket.ss_family == AF_INET) reinterpret_cast<sockaddr_in*>(&address.socket)->sin_port = htons(port);
    else reinterpret_cast<sockaddr_in6*>(&address.socket)->sin6_port = htons(port);
    OwnedSocket socket{::socket(address.socket.ss_family, SOCK_STREAM, IPPROTO_TCP)};
    if (socket.value == invalid_socket) { s.state = "error"; s.error = "Cannot create socket."; return s; }
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket.value, FIONBIO, &mode);
#else
    fcntl(socket.value, F_SETFL, fcntl(socket.value,F_GETFL,0) | O_NONBLOCK);
#endif
    const auto start = Clock::now();
    const auto deadline = start + std::chrono::milliseconds(timeout);
    int result = connect(socket.value, reinterpret_cast<sockaddr*>(&address.socket), address.length);
    int error = result == 0 ? 0 : socket_error();
    if (result != 0 && pending(error)) {
        if (wait_socket(socket.value,false,deadline,cancel)) {
            error = 0;
            SocketLength length = sizeof(error);
            getsockopt(socket.value, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length);
        } else {
            s.state = cancel ? "cancelled" : "no response";
            s.error = cancel ? "Cancelled." : "Connection timed out; filtering or packet loss is possible.";
            return s;
        }
    }
    s.latency_ms = std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    if (error) {
        s.state = refused(error) ? "closed" : "unreachable";
        s.error = "Socket error " + std::to_string(error);
        return s;
    }
    s.state = "open";
    activity("Connected; checking the response");

    if (port == 9100 || cancel) return s;
    const bool server_first = !web_port(port) && port != 993 && port != 995 && port != 3389 && port != 445;
    if (server_first) {
        activity("Connected; waiting for a service banner");
        const auto banner_deadline = Clock::now() + std::chrono::milliseconds(std::min(timeout,700));
        if (wait_socket(socket.value,true,banner_deadline,cancel)) {
            std::array<char,4096> buffer{};
            auto n = recv(socket.value,buffer.data(),static_cast<int>(buffer.size()),0);
            if (n > 0) s.banner.assign(buffer.data(),static_cast<size_t>(n));
        }
    }

    if (!s.banner.empty() || http_planned || cancel) return s;
    const auto kind = probe_kind(port);
    const auto payload = probe_payload(kind);
    if (payload.empty()) return s;
    s.active_probe = true;
    activity("Connected; sending one identification probe");
    const auto probe_deadline = Clock::now() + std::chrono::milliseconds(std::min(timeout,800));
    if (send_all(socket.value, payload.data(), payload.size(), probe_deadline, cancel))
        s.banner = label_reply(kind, recv_some(socket.value, probe_deadline, cancel));
    return s;
}
Service probe_proxy(const ProxyConfig& proxy,const std::string& host,uint16_t port,int timeout,std::atomic_bool& cancel,bool http_planned,const std::function<void(const std::string&)>& activity) {
    Service s;s.port=port;s.expected=expected_service(port);s.state="cancelled";
    if(cancel)return s;
    const auto start=Clock::now();ProxyTunnel tunnel;
    if(!tunnel.connect(proxy,host,port,start+std::chrono::milliseconds(timeout),cancel)) {
        s.state=cancel?"cancelled":"error";s.error=tunnel.error();return s;
    }
    s.latency_ms=std::chrono::duration<double,std::milli>(Clock::now()-start).count();s.state="open";
    activity("Proxy tunnel established; checking the response");
    if(port==9100||cancel)return s;
    const bool server_first=!web_port(port)&&port!=993&&port!=995&&port!=3389&&port!=445;
    if(server_first)s.banner=tunnel.read(Clock::now()+std::chrono::milliseconds(std::min(timeout,700)),cancel);
    if(!s.banner.empty()||http_planned||cancel)return s;
    const auto kind=probe_kind(port);const auto payload=probe_payload(kind);if(payload.empty())return s;
    s.active_probe=true;activity("Connected through proxy; sending one identification probe");
    const auto deadline=Clock::now()+std::chrono::milliseconds(std::min(timeout,800));
    if(tunnel.write(payload,deadline,cancel))s.banner=label_reply(kind,tunnel.read(deadline,cancel));
    return s;
}
constexpr size_t max_body = 512 * 1024;
using asset_scope::AddressScope;
struct Transfer {
    HttpObservation observation;
    std::atomic_bool* cancel;
    size_t header_bytes = 0;
    size_t body_limit = max_body;
    asset_scope::ScopeGuard guard = {};
};
size_t body_callback(char* data, size_t size, size_t count, void* user) {
    auto& t = *static_cast<Transfer*>(user);
    const size_t bytes = size * count;
    if (*t.cancel) return 0;
    size_t remaining = t.body_limit - t.observation.body.size();
    t.observation.body.append(data,std::min(bytes,remaining));
    if (bytes > remaining) { t.observation.body_truncated = true; return 0; }
    return bytes;
}
size_t header_callback(char* data, size_t size, size_t count, void* user) {
    auto& t = *static_cast<Transfer*>(user);
    const size_t bytes = size * count;
    if (*t.cancel || (t.header_bytes += bytes) > 65536) return 0;
    std::string line(data,bytes);
    if (line.starts_with("HTTP/")) { t.observation.headers.clear(); return bytes; }
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        auto key = lower(trim(line.substr(0,colon)));
        auto value = trim(line.substr(colon+1));
        if (key == "set-cookie") {
            auto equal = value.find('=');
            value = value.substr(0,equal) + "=[redacted]";
        }
        t.observation.headers.emplace_back(key, printable(value,4096));
    }
    return bytes;
}
int progress_callback(void* data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return *static_cast<std::atomic_bool*>(data) ? 1 : 0;
}
HttpObservation fetch(const Target& target, const Address& address, int timeout, bool allow_untrusted_tls, std::atomic_bool& cancel,const ProxyConfig& proxy,size_t body_limit=max_body, const std::string& extra_header={},
                      AddressScope scope=AddressScope::Any, bool* scope_blocked=nullptr) {
    Transfer t{{},&cancel};
    t.body_limit=body_limit;
    t.guard.scope=scope;
    t.observation.url = target.url();
    auto raw = curl_easy_init();
    if (!raw) { t.observation.error = "Could not initialize HTTP client."; return t.observation; }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw,curl_easy_cleanup);
    char error[CURL_ERROR_SIZE]{};
    curl_easy_setopt(raw,CURLOPT_URL,t.observation.url.c_str());
    configure_proxy(raw,proxy);
    curl_easy_setopt(raw,CURLOPT_PROTOCOLS_STR,"http,https");
    curl_easy_setopt(raw,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");
    curl_easy_setopt(raw,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(raw,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(raw,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(timeout));
    curl_easy_setopt(raw,CURLOPT_TIMEOUT_MS,static_cast<long>(timeout));
    curl_easy_setopt(raw,CURLOPT_USERAGENT,"Vector/0.1 (host fingerprinting)");
    curl_easy_setopt(raw,CURLOPT_ACCEPT_ENCODING,"identity");
    curl_easy_setopt(raw,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(raw,CURLOPT_WRITEFUNCTION,body_callback);
    curl_easy_setopt(raw,CURLOPT_WRITEDATA,&t);
    curl_easy_setopt(raw,CURLOPT_HEADERFUNCTION,header_callback);
    curl_easy_setopt(raw,CURLOPT_HEADERDATA,&t);
    curl_easy_setopt(raw,CURLOPT_ERRORBUFFER,error);
    curl_easy_setopt(raw,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(raw,CURLOPT_XFERINFOFUNCTION,progress_callback);
    curl_easy_setopt(raw,CURLOPT_XFERINFODATA,&cancel);
    curl_easy_setopt(raw,CURLOPT_CERTINFO,1L);
    curl_easy_setopt(raw,CURLOPT_SSL_VERIFYPEER,allow_untrusted_tls ? 0L : 1L);
    curl_easy_setopt(raw,CURLOPT_SSL_VERIFYHOST,allow_untrusted_tls ? 0L : 2L);
    curl_slist* headers = nullptr;
    std::unique_ptr<curl_slist,decltype(&curl_slist_free_all)> header_list(nullptr,curl_slist_free_all);
    if (!extra_header.empty()) {
        headers = curl_slist_append(nullptr, extra_header.c_str());
        header_list.reset(headers);
        curl_easy_setopt(raw,CURLOPT_HTTPHEADER,headers);
    }
    std::string numeric = address.text.find(':') != std::string::npos ? "["+address.text+"]" : address.text;
    std::string mapping = target.authority() + ":" + std::to_string(target.port) + ":" + numeric + ":" + std::to_string(target.port);
    curl_slist* connect = curl_slist_append(nullptr,mapping.c_str());
    std::unique_ptr<curl_slist,decltype(&curl_slist_free_all)> connections(connect,curl_slist_free_all);

    if(!proxy.enabled&&!address.text.empty())curl_easy_setopt(raw,CURLOPT_CONNECT_TO,connect);

    if(scope!=AddressScope::Any&&!proxy.enabled) {
        curl_easy_setopt(raw,CURLOPT_OPENSOCKETFUNCTION,asset_scope::open_scoped_socket);
        curl_easy_setopt(raw,CURLOPT_OPENSOCKETDATA,&t.guard);
    }
    const auto result = curl_easy_perform(raw);
    curl_easy_getinfo(raw,CURLINFO_RESPONSE_CODE,&t.observation.status);
    if(scope_blocked)*scope_blocked=t.guard.blocked&&t.observation.status==0;
    double elapsed = 0;
    curl_easy_getinfo(raw,CURLINFO_TOTAL_TIME,&elapsed);
    t.observation.elapsed_ms = elapsed * 1000;
    char* ip = nullptr;
    curl_easy_getinfo(raw,CURLINFO_PRIMARY_IP,&ip);
    if (ip&&!proxy.enabled) t.observation.address = ip;
    long http_version = 0;
    curl_easy_getinfo(raw,CURLINFO_HTTP_VERSION,&http_version);
    t.observation.http_version = http_version == CURL_HTTP_VERSION_1_0 ? "HTTP/1.0" : http_version == CURL_HTTP_VERSION_1_1 ? "HTTP/1.1" : "";
    char* redirect = nullptr;
    curl_easy_getinfo(raw,CURLINFO_REDIRECT_URL,&redirect);
    if (redirect) t.observation.redirect = redirect;
    long verify = -1;
    curl_easy_getinfo(raw,CURLINFO_SSL_VERIFYRESULT,&verify);
    t.observation.tls_verified = !allow_untrusted_tls && target.scheme == "https" && verify == 0 && t.observation.status > 0;
    curl_certinfo* certificates = nullptr;
    if (target.scheme == "https" && curl_easy_getinfo(raw,CURLINFO_CERTINFO,&certificates) == CURLE_OK && certificates && certificates->num_of_certs > 0) {
        for (auto* entry = certificates->certinfo[0]; entry; entry = entry->next) {
            std::string value = entry->data;
            auto colon = value.find(':');
            if (colon != std::string::npos && value.substr(0,colon) == "Cert") {
                auto pem = value.substr(colon + 1);
                if (pem.size() > 16384) pem.resize(16384);
                t.observation.certificate.emplace_back("pem", pem);
            } else if (colon != std::string::npos)
                t.observation.certificate.emplace_back(value.substr(0,colon),printable(value.substr(colon+1),4096));
        }
    }
    if (result != CURLE_OK && !(result == CURLE_WRITE_ERROR && t.observation.body_truncated))
        t.observation.error = cancel ? "Cancelled." : (proxy.enabled ? curl_easy_strerror(result) : error[0] ? error : curl_easy_strerror(result));
    static const std::regex title(R"(<title\b[^>]{0,256}>([^<]{0,512})</title\s*>)",std::regex::icase);
    std::smatch match;
    if (std::regex_search(t.observation.body,match,title)) t.observation.title = printable(match[1].str(),300);
    return std::move(t.observation);
}
}
ScanReport scan(const Target& target, const ScanOptions& requested, std::atomic_bool& cancel, const Progress& progress, const ScanObserver& observer) {
    static Runtime runtime;
    ScanOptions options=requested;
    ScanReport report;
    report.target = target; report.started_at = utc_now();
    const auto start = Clock::now();
    std::mutex activity_mutex;
    InspectionActivity activity;
    auto event = [&](ScanStage stage, ActivityState state, const std::string& item, const std::string& detail, size_t completed = 0, size_t total = 0) {
        std::lock_guard lock(activity_mutex);
        ScanEvent e{stage,state,std::chrono::duration<double,std::milli>(Clock::now()-start).count(),item,detail,completed,total};
        activity.accept(e);
        report.activity.push_back(e);
        if (observer) observer(e);
    };
    auto update = [&](float percent,const std::string& text) { if (progress) progress(percent,text); };
    auto finish = [&] {
        if (cancel) { report.status = "cancelled"; report.notes.emplace_back("Inspection cancelled. Observations are partial; remaining checks may not have run."); }

        for (size_t i = 0; i < 4; ++i) {
            const auto s = activity.stages[i];
            if (s.state == ActivityState::Running)
                event(static_cast<ScanStage>(i),cancel ? ActivityState::Cancelled : ActivityState::Failed,{},cancel ? "Interrupted; collected observations retained" : report.error,s.completed,s.total);
            else if (s.state == ActivityState::Waiting)
                event(static_cast<ScanStage>(i),cancel ? ActivityState::Cancelled : ActivityState::Skipped,{},cancel ? "Not started before cancellation" : "Not reached after an earlier failure");
        }
        for (const auto& w : activity.work) if (w.state == ActivityState::Running)
            event(w.stage,cancel ? ActivityState::Cancelled : ActivityState::Failed,w.item,cancel ? "Interrupted by cancellation" : "Interrupted by inspection failure");
        event(ScanStage::Report,ActivityState::Running,{},"Preserving observations and their evidence");
        report.finished_at = utc_now();
        report.duration_ms = std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        event(ScanStage::Report,ActivityState::Complete,{},report.status == "complete" ? "Report ready" : "Partial report ready");
        update(1.0f, report.status == "cancelled" ? "Cancelled; partial observations retained." : report.status == "error" ? "Inspection failed." : "Inspection complete.");
        return report;
    };
    try {
        if (options.timeout_ms < 100 || options.timeout_ms > 10000 || options.http_timeout_ms < 100 || options.http_timeout_ms > 30000 || options.browser_timeout_ms < 1000 || options.browser_timeout_ms > 30000)
            throw std::invalid_argument("Timeout is outside the supported range.");
        validate_proxy(options.proxy);
        report.proxy={options.proxy.enabled,!options.proxy.username.empty(),options.proxy.enabled?proxy_endpoint(options.proxy):std::string{}};
        if(options.proxy.enabled&&options.proxy.scheme=="auto") {
            update(0.01f,"Checking the proxy at "+proxy_endpoint(options.proxy));
            event(ScanStage::Resolve,ActivityState::Running,{},"Checking the proxy at "+proxy_endpoint(options.proxy));
            event(ScanStage::Resolve,ActivityState::Running,"Proxy type","Asking "+proxy_endpoint(options.proxy)+" whether it is an HTTP, HTTPS or SOCKS5 proxy");
            std::string detected;
            options.proxy=resolve_proxy(options.proxy,target.host,target.port,cancel,&detected);
            if(cancel)return finish();
            event(ScanStage::Resolve,ActivityState::Complete,"Proxy type",detected);
            report.notes.emplace_back("Proxy type detected automatically. "+detected+".");
        }
        report.proxy={options.proxy.enabled,!options.proxy.username.empty(),options.proxy.enabled?proxy_endpoint(options.proxy):std::string{}};
        report.tcp_coverage = plan_tcp_scan(target,options);
        if (cancel) return finish();
        update(0.02f,"Resolving " + target.host);
        event(ScanStage::Resolve,ActivityState::Running,{},"Resolving " + target.host);
        event(ScanStage::Resolve,ActivityState::Running,"Address lookup","Resolving " + target.host);
        std::vector<Address> addresses;
        if(options.proxy.enabled) {
            addresses.emplace_back();
            if(target.is_ip){report.address=target.host;report.addresses.push_back(target.host);}
            event(ScanStage::Resolve,ActivityState::Complete,"Address lookup","Target connections and hostname resolution use the configured proxy");
            report.notes.emplace_back("Proxy enabled: TCP, web, browser and HTTPS metadata requests use "+proxy_endpoint(options.proxy)+". No direct fallback. Target IPs are not resolved locally; proxy addresses are not target evidence.");
            report.notes.emplace_back("ICMP and UDP discovery are skipped through a TCP proxy. Proxy policy may restrict ports; a refused tunnel does not establish that the target port is closed.");
        } else {
            addresses=resolve(target.host);
            for(const auto& a:addresses)report.addresses.push_back(a.text);
            report.address=addresses.front().text;
            event(ScanStage::Resolve,ActivityState::Complete,"Address lookup","Using "+report.address+" / "+std::to_string(addresses.size())+" addresses returned");
        }
        event(ScanStage::Resolve,ActivityState::Running,"Domain DNS","Checking mail, authoritative DNS and hosting aliases");
        report.dns=inspect_dns(target,report.address,cancel,options.proxy);
        identify_dns(report.dns,report.findings);
        event(ScanStage::Resolve,cancel?ActivityState::Cancelled:report.dns.state=="skipped"?ActivityState::Skipped:
            report.dns.state=="complete"?ActivityState::Complete:ActivityState::Failed,"Domain DNS",report.dns.reason);
        if(cancel)return finish();
        event(ScanStage::Resolve,ActivityState::Running,"ICMP echo","Sending one echo request to " + report.address);
        if(options.proxy.enabled){report.icmp.state="skipped";report.icmp.detail="ICMP is unavailable through the configured TCP proxy.";}
        else report.icmp=inspect_icmp(report.address,cancel);
        event(ScanStage::Resolve,cancel?ActivityState::Cancelled:report.icmp.state=="skipped"?ActivityState::Skipped:report.icmp.state=="error"||report.icmp.state=="unavailable"?ActivityState::Failed:ActivityState::Complete,
            "ICMP echo",report.icmp.detail);
        if(!options.proxy.enabled)report.notes.emplace_back("Inspection covers one resolved address: " + report.address + ". Additional DNS addresses are listed, not scanned.");
        report.notes.emplace_back("Device and OS matches are hints from advertised data. NAT, virtual hosting, proxies and spoofed banners can obscure the origin.");
        report.notes.emplace_back("A silent TCP port receives one identification probe. Port 9100 is never sent a payload. HTTP(S) targets are read with the web collector instead.");
        if (options.allow_untrusted_tls) report.notes.emplace_back("TLS certificate verification was disabled for this inspection by user option. Peer identity is unverified.");
        if (cancel) return finish();
        update(0.05f, "Reading registry record for " + report.address);
        event(ScanStage::Resolve,ActivityState::Running,{},"Checking the IP registry record for " + report.address);
        event(ScanStage::Resolve,ActivityState::Running,"IP registry","Checking registry eligibility and allocation metadata");
        if(report.address.empty()){report.registry.scope="unavailable";report.registry.error="Target IP unavailable with remote proxy resolution; registry lookup skipped.";}
        else report.registry=lookup_registry(report.address,5000,cancel,options.proxy);
        event(ScanStage::Resolve,cancel ? ActivityState::Cancelled : report.registry.scope == "private" ? ActivityState::Skipped : report.registry.error.empty() ? ActivityState::Complete : ActivityState::Failed,
            "IP registry",report.registry.scope == "private" ? "Local address; no registry request sent" : report.registry.error.empty() ? "Registry response collected" : printable(report.registry.error,300));
        if (cancel) return finish();
        event(ScanStage::Resolve,ActivityState::Running,"IP geolocation","Looking up approximate location for the selected IP");
        report.geolocation=lookup_geolocation(report.address,3000,cancel,options.proxy);
        event(ScanStage::Resolve,cancel?ActivityState::Cancelled:report.geolocation.state=="skipped"?ActivityState::Skipped:report.geolocation.state=="available"?ActivityState::Complete:ActivityState::Failed,
            "IP geolocation",report.geolocation.state=="available"?"Approximate location collected from ipwho.is":report.geolocation.error);
        if (cancel) return finish();
        event(ScanStage::Resolve,ActivityState::Complete,{},"Address ready: " + report.address,1,1);
        const auto& ports = report.tcp_coverage.selected_ports;
        const auto concurrency = report.tcp_coverage.max_concurrent;
        const std::string selection = report.tcp_coverage.profile == "deep_ip" ? "Deeper IP scan: TCP 1-1024 plus common higher service ports" :
            report.tcp_coverage.profile == "common" ? "Common hostname ports" : report.tcp_coverage.profile == "custom" ? "Custom port selection" : "Explicit web or port endpoint";
        report.notes.push_back(selection + " / " + std::to_string(ports.size()) + " TCP ports; up to " + std::to_string(concurrency) + " connections at once.");
        event(ScanStage::Services,ActivityState::Running,{},selection + "; up to " + std::to_string(concurrency) + " connections at once",0,ports.size());
        std::atomic_size_t checks_complete{0};
        for (size_t base = 0; base < ports.size() && !cancel; base += concurrency) {
            std::vector<std::future<Service>> workers;
            const size_t end = std::min(base+concurrency,ports.size());
            for (size_t i = base; i < end; ++i) {
                const auto port = ports[i];
                const bool http_planned = port != 9100 && (web_port(port) || (port == target.port && (target.explicit_port || target.explicit_scheme || options.web_only)));
                workers.push_back(std::async(std::launch::async,[&,port,http_planned] {
                    const auto item = "TCP " + std::to_string(port);
                    event(ScanStage::Services,ActivityState::Running,item,"Opening a TCP connection");
                    const auto callback=[&](const std::string& detail){event(ScanStage::Services,ActivityState::Running,item,detail);};
                    auto service=options.proxy.enabled?probe_proxy(options.proxy,target.host,port,options.timeout_ms,cancel,http_planned,callback):
                        probe(addresses.front(),port,options.timeout_ms,cancel,http_planned,callback);
                    const auto state = cancel || service.state == "cancelled" ? ActivityState::Cancelled : service.state == "error" ? ActivityState::Failed : ActivityState::Complete;
                    auto detail = service.state;
                    if (state == ActivityState::Cancelled) detail += " / response check interrupted";
                    else if (!service.banner.empty()) detail += " / " + std::to_string(service.banner.size()) + " response bytes";
                    else if (service.state == "open") detail += port == 9100 ? " / connection check only" : " / no banner";
                    event(ScanStage::Services,state,item,detail);
                    if (state == ActivityState::Complete || state == ActivityState::Failed) ++checks_complete;
                    return service;
                }));
            }
            for (size_t i = 0; i < workers.size(); ++i) {
                auto service = workers[i].get();
                identify_banner(service,report.findings);
                report.services.push_back(std::move(service));
                report.tcp_coverage.completed = checks_complete.load();
                event(ScanStage::Services,ActivityState::Running,{},"Checking responses against service signatures",checks_complete.load(),ports.size());
                update(0.08f+0.48f*static_cast<float>(base+i+1)/static_cast<float>(ports.size()),"Checking TCP services: " + std::to_string(base+i+1) + "/" + std::to_string(ports.size()));
            }
        }
        if (!cancel) event(ScanStage::Services,ActivityState::Complete,{},std::to_string(ports.size()) + (ports.size() == 1 ? " port checked" : " ports checked"),ports.size(),ports.size());
        std::vector<uint16_t> http_ports;
        for (const auto& s : report.services) {
            const bool planned = web_port(s.port) || (s.port == target.port && (target.explicit_port || target.explicit_scheme || options.web_only));
            if (s.state == "open" && s.port != 9100 && (s.name == "HTTP" || s.name == "HTTPS" || (s.name == "Unknown" && planned))) http_ports.push_back(s.port);
        }

        std::stable_sort(http_ports.begin(),http_ports.end(),[&](uint16_t a,uint16_t b) {
            const auto priority=[&](uint16_t port) {
                if((target.explicit_scheme||target.explicit_port)&&port==target.port)return 0;
                return port==443?1:port==80?2:port==8443?3:port==8080?4:5;
            };
            return priority(a)<priority(b);
        });
        if (http_ports.size()>8) { http_ports.resize(8); report.notes.emplace_back("HTTP inspection limited to eight open web endpoints."); }
        auto& coverage=report.web_coverage;
        const auto web_started=Clock::now(), web_deadline=web_started+std::chrono::milliseconds(WebCoverage::time_limit_ms);
        coverage.started=!cancel&&!http_ports.empty();
        auto cap=[&](const std::string& reason) {
            if(std::find(coverage.reasons.begin(),coverage.reasons.end(),reason)!=coverage.reasons.end())return;
            coverage.limited=true;coverage.reasons.push_back(reason);report.notes.push_back(reason+"; collected evidence retained.");
            event(ScanStage::Web,ActivityState::Skipped,"Web evidence cap",reason+"; collected evidence retained");
        };
        auto remaining_ms=[&]() {
            const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(web_deadline-Clock::now()).count();
            if(left<=0){cap("30-second web evidence time limit reached");return 0;}
            return static_cast<int>(left);
        };
        auto page_available=[&]() {
            if(cancel||!remaining_ms())return false;
            if(coverage.page_requests>=WebCoverage::page_limit){cap("12-page web request limit reached");return false;}
            return true;
        };
        auto web_progress=[&]() {
            event(ScanStage::Web,ActivityState::Running,{},"Automatic cap: 12 page requests, 40 static scripts, 30 seconds across all web endpoints",
                  coverage.page_requests,WebCoverage::page_limit);
        };
        if (!cancel) {
            if(http_ports.empty())event(ScanStage::Web,ActivityState::Skipped,{},"No eligible open web endpoint");
            else web_progress();
        }
        if (!options.browser_runtime) { report.browser.state="disabled"; report.browser.reason="Browser execution disabled by CLI option"; }
        std::set<std::string> disclosure_seen,disclosure_queued;
        std::set<std::string> script_references_seen,page_attempts,browser_attempts,stylesheet_seen;
        size_t stylesheet_requests=0;
        std::map<std::string,Address> web_addresses{{target.host,addresses.front()}};
        std::map<std::string,AddressScope> followed_hosts;
        std::set<std::string> refused_hosts;

        std::set<std::string> asset_hosts_failed;
        const auto asset_endpoint=[](const Target& asset){return asset.host+":"+std::to_string(asset.port);};
        auto other_host_scope=[&](const Target& asset,const Target& endpoint,const HttpObservation& page) {
            return asset_scope::other_host_scope(asset,endpoint,page.address,options.proxy.enabled);
        };
        auto collect_stylesheets=[&](const HttpObservation& page,const Target& endpoint,const Address& address) {
            if(page.status<200||page.status>=300||(!page.error.empty()&&!page.body_truncated)||cancel)return;
            for(const auto& [name,value]:page.headers)if(name=="content-type") {
                const auto mime=lower(trim(value.substr(0,value.find(';'))));
                if(mime!="text/html"&&mime!="application/xhtml+xml")return;
            }
            const auto references=page_stylesheet_references(page);

            for(const bool other_host:{false,true}) for(const auto& url:references) {
                if(cancel)break;
                if(stylesheet_seen.contains(url))continue;
                Target asset;bool valid=true;
                try {asset=parse_target(url);}catch(const std::exception&){valid=false;}
                if((valid&&asset.host!=endpoint.host)!=other_host)continue;
                if(report.stylesheets.size()>=128){cap("Stylesheet references reached their 128-entry limit");break;}
                stylesheet_seen.insert(url);
                ScriptObservation read;read.url=url;read.referenced_by=page.url;read.state="skipped";
                if(!valid){read.reason="Invalid stylesheet URL";report.stylesheets.push_back(std::move(read));continue;}
                const auto scope=other_host?other_host_scope(asset,endpoint,page):AddressScope::Any;
                if(other_host?scope==AddressScope::Any:asset.port!=endpoint.port)read.reason="Stylesheet is outside the page host and port";
                else if(endpoint.scheme=="https"&&asset.scheme!="https")read.reason="HTTPS downgrade not followed";
                else if(asset.port==9100)read.reason="Port 9100 does not receive payloads";
                else if(other_host&&asset_hosts_failed.contains(asset_endpoint(asset)))read.reason="External host failed an earlier request; not retried";
                else if(stylesheet_requests>=8){read.reason="Eight-stylesheet request limit reached";cap(read.reason);}
                else if(!remaining_ms())read.reason="Web evidence time limit reached";
                if(read.reason.empty()) {
                    ++stylesheet_requests;
                    event(ScanStage::Web,ActivityState::Running,"Stylesheet "+url,other_host?"Reading a declared external stylesheet for framework evidence":"Reading stylesheet framework evidence");
                    const auto timeout=std::min({options.http_timeout_ms,remaining_ms(),1500});
                    if(timeout<=0){read.reason="Web evidence time limit reached";report.stylesheets.push_back(std::move(read));break;}

                    bool blocked=false;
                    auto file=other_host?fetch(asset,Address{},timeout,false,cancel,options.proxy,max_body,{},scope,&blocked)
                                        :fetch(asset,address,timeout,options.allow_untrusted_tls,cancel,options.proxy);
                    if(other_host&&file.status==0&&!cancel)asset_hosts_failed.insert(asset_endpoint(asset));
                    read.status=file.status;read.body_bytes=file.body.size();read.body_truncated=file.body_truncated;
                    std::string mime;for(const auto& [name,value]:file.headers)if(name=="content-type")mime=lower(trim(value.substr(0,value.find(';'))));
                    if(cancel){read.state="cancelled";read.reason="Cancelled during stylesheet read";}
                    else if(blocked){read.state="skipped";read.reason="External host resolves outside the page's address scope; not requested";}
                    else if(file.status>=300&&file.status<400){read.state="skipped";read.reason="Stylesheet redirect recorded, not followed";}
                    else if(file.status<200||file.status>=300||(!file.error.empty()&&!file.body_truncated)){read.state="failed";read.reason=file.error.empty()?"HTTP "+std::to_string(file.status):file.error;}
                    else if(mime!="text/css"){read.state="skipped";read.reason="Response is not text/css";}
                    else {
                        identify_stylesheet(url,file.body,report.findings);read.analyzed=true;read.state="analyzed";
                        read.reason=file.body_truncated?"Analyzed first 512 KiB; stylesheet truncated":"Stylesheet declarations and banners analyzed";
                        if(file.body_truncated)cap("Stylesheet body reached its 512 KiB limit");
                    }
                    event(ScanStage::Web,cancel?ActivityState::Cancelled:read.state=="failed"?ActivityState::Failed:ActivityState::Complete,"Stylesheet "+url,read.reason);
                }
                report.stylesheets.push_back(std::move(read));
            }
        };
        std::vector<DisclosureCandidate> disclosure_queue;
        size_t disclosure_requests=0;
        double disclosure_ms=0;
        bool disclosure_browser_used=false;
        auto queue_disclosures=[&](const HttpObservation& page,const DisclosureCandidate* parent=nullptr) {
            for(auto candidate:disclosure_candidates(page,target,parent)) {
                if(disclosure_queued.size()>=48)break;
                if(disclosure_queued.insert(candidate.url).second)disclosure_queue.push_back(std::move(candidate));
            }
        };
        auto collect_disclosures=[&]() {
            while(!disclosure_queue.empty()&&!cancel) {
                std::stable_sort(disclosure_queue.begin(),disclosure_queue.end(),[](const auto& a,const auto& b){return a.priority>b.priority;});
                auto candidate=std::move(disclosure_queue.front());disclosure_queue.erase(disclosure_queue.begin());
                const auto url=candidate.url;
                if(!disclosure_seen.insert(url).second||page_attempts.contains(url))continue;
                DisclosureSource source;source.url=url;source.requested_url=url;source.checked_at=utc_now();
                source.discovered_from=candidate.discovered_from;source.discovery_label=candidate.label;source.discovery_chain=candidate.chain;source.depth=candidate.depth;
                if(disclosure_requests>=4||disclosure_ms>=5000||!page_available()) {
                    source.state="skipped";
                    source.reason=disclosure_requests>=4?"Four disclosure request limit reached":disclosure_ms>=5000?"Five-second disclosure transfer limit reached":"Shared web evidence cap reached";
                    event(ScanStage::Web,ActivityState::Skipped,"Disclosure "+url,source.reason);
                    report.disclosures.push_back(std::move(source));continue;
                }
                const auto tip=parse_target(url);
                const int timeout=std::min({2500,remaining_ms(),std::max(1,5000-static_cast<int>(disclosure_ms))});
                if(!timeout)continue;
                ++disclosure_requests;++coverage.page_requests;page_attempts.insert(url);web_progress();
                event(ScanStage::Web,ActivityState::Running,"Disclosure "+url,"Following a published privacy, trust or provider link");
                const auto began=Clock::now();
                const Address address=web_addresses.contains(tip.host)?web_addresses.at(tip.host):Address{};
                auto page=fetch(tip,address,timeout,options.allow_untrusted_tls,cancel,options.proxy,3*1024*1024);
                disclosure_ms+=std::chrono::duration<double,std::milli>(Clock::now()-began).count();
                source.title=page.title;source.status=page.status;
                if(cancel){source.state="cancelled";source.reason="Cancelled during disclosure read";}
                else if(page.status>=300&&page.status<400&&!page.redirect.empty()) {
                    source.state="redirect";source.reason="Published source redirected";
                    bool followed=false;
                    try {
                        const auto dest=parse_target(page.redirect);
                        const bool allowed=disclosure_url_allowed(tip,dest.url())||(!target.is_ip&&!dest.is_ip&&dest.scheme=="https"&&dest.port==443);
                        if(allowed&&!(tip.scheme=="https"&&dest.scheme!="https")&&candidate.depth<4&&disclosure_queued.size()<48&&disclosure_queued.insert(dest.url()).second) {
                            auto next=candidate;next.url=dest.url();next.discovered_from=url;next.label="HTTP redirect";next.depth++;
                            next.chain.push_back({url,"HTTP "+std::to_string(page.status)+" redirect -> "+next.url});
                            disclosure_queue.push_back(std::move(next));followed=true;
                        }
                    }catch(const std::exception&){}
                    if(!followed)source.reason+="; destination outside permitted scope, already attempted, or depth limit reached";
                } else if(!page.error.empty()||page.status<200||page.status>=300) {
                    source.state="failed";source.reason=page.error.empty()?"HTTP "+std::to_string(page.status):page.error;
                } else {
                    source.providers=identify_disclosures(page,report,&candidate);source.state=source.providers?"collected":"no_match";
                    source.reason=source.providers?"Provider records extracted from the collected document":"No supported provider records found";
                    if(page.body_truncated)source.reason+="; disclosure body truncated at 3 MiB";
                    queue_disclosures(page,&candidate);

                    if(!source.providers&&!disclosure_browser_used&&options.browser_runtime&&remaining_ms()>1500&&
                       report.browser.pages.size()<7&&(candidate.priority>=3||disclosure_candidates(page,target,&candidate).empty())&&
                       lower(page.body).find("<script")!=std::string::npos) {
                        disclosure_browser_used=true;
                        ScanReport rendered;rendered.target=report.target;rendered.address=report.address;
                        auto browser_options=options;browser_options.browser_timeout_ms=std::min(5000,remaining_ms());
                        inspect_browser(tip,rendered,browser_options,cancel,[&](const ScanEvent& e){event(e.stage,e.state,"Disclosure "+e.item,e.detail);},web_deadline,&page,
                            [&](const HttpObservation& dom) {
                                auto origin=candidate;origin.url=dom.url;origin.chain.push_back({dom.url,"Rendered document after JavaScript execution"});
                                source.providers=identify_disclosures(dom,report,&origin);queue_disclosures(dom,&origin);
                            });
                        for(auto& browser_page:rendered.browser.pages) {browser_page.purpose="disclosure";report.browser.pages.push_back(std::move(browser_page));}
                        for(auto& request:rendered.browser.requests)report.browser.requests.push_back(std::move(request));
                        if(report.browser.engine.empty())report.browser.engine=rendered.browser.engine;
                        if(report.browser.state=="not_run"){report.browser.state=rendered.browser.state;report.browser.reason=rendered.browser.reason;}
                        source.state=cancel?"cancelled":source.providers?"collected":"no_match";
                        source.reason=source.providers?"Provider records extracted from the rendered document":"No supported provider records in collected or rendered document; "+rendered.browser.reason;
                    }
                }
                event(ScanStage::Web,cancel?ActivityState::Cancelled:source.state=="failed"?ActivityState::Failed:ActivityState::Complete,
                      "Disclosure "+url,std::to_string(source.providers)+" published providers / "+source.reason);
                report.disclosures.push_back(std::move(source));
            }
        };
        struct LinkedCandidate {std::string url,page,address;int score=0;std::string destination;AddressScope scope=AddressScope::Any;};
        LinkedCandidate documentation;
        std::set<std::string> entry_pages,linked_known;
        bool documentation_open=true;
        size_t linked_web_begin=0,linked_api_begin=0;
        auto consider_documentation=[&](const HttpObservation& page,const std::string& address) {
            if(!documentation_open)return;
            Target from;
            try {from=parse_target(page.url);}catch(const std::exception&){return;}
            for(const auto& url:page_anchor_urls(page)) {
                Target link;
                try {link=parse_target(url);}catch(const std::exception&){continue;}
                const int score=documentation_link_score(from,link);
                if(score>documentation.score)documentation={link.url(),from.url(),address,score,{},AddressScope::Any};
            }
        };
        auto observe_document=[&](const HttpObservation& dom) {
            try {
                const auto rendered=parse_target(dom.url);
                if(entry_pages.contains(rendered.url())) {
                    const auto known=web_addresses.find(rendered.host);
                    consider_documentation(dom,known==web_addresses.end()?std::string{}:known->second.text);
                }
            } catch(const std::exception&) {}
            DisclosureCandidate origin;origin.url=dom.url;origin.chain.push_back({dom.url,"Rendered document after JavaScript execution"});
            const auto providers=identify_disclosures(dom,report,&origin);
            if(providers) {
                DisclosureSource source;source.url=dom.url;source.title=dom.title;source.status=dom.status;source.checked_at=utc_now();source.providers=providers;
                source.state="collected";source.reason="Provider records extracted from the rendered document";source.discovery_chain=origin.chain;
                report.disclosures.push_back(std::move(source));
            }
            queue_disclosures(dom,&origin);
        };
        struct WebEndpoint {
            Target seed;
            std::set<std::string> visited;

            std::multimap<int,std::string,std::greater<int>> queue;
            bool browser_read=false;
            Target moved;
            bool has_moved=false;
            bool linked=false;
        };
        std::vector<WebEndpoint> endpoints;
        for (const auto port:http_ports) {
            Target seed = target;
            seed.port = port;
            seed.scheme = (seed.port == target.port && (target.explicit_scheme || target.explicit_port)) ? target.scheme : (seed.port == 443 || seed.port == 8443 ? "https" : "http");
            seed.path = seed.port == target.port || (!target.explicit_scheme&&!target.explicit_port&&seed.port==443) ? target.path : "/";
            endpoints.push_back({std::move(seed),{},{},false,{},false});
        }

        auto begin_linked_page=[&]() {
            documentation_open=false;
            if(cancel||documentation.url.empty())return false;
            LinkedPage linked;linked.role="documentation";linked.requested_url=documentation.url;linked.discovered_from=documentation.page;
            const auto skip=[&](const std::string& reason) {
                linked.state="skipped";linked.reason=reason;
                event(ScanStage::Web,ActivityState::Skipped,"Linked page "+documentation.url,reason);
                report.linked_pages.push_back(std::move(linked));
                return false;
            };
            Target link,from;
            try {link=parse_target(documentation.url);from=parse_target(documentation.page);}
            catch(const std::exception&){return false;}
            if(same_website_host(from,link)&&documentation.destination.empty()&&page_attempts.contains(link.url()))return false;
            auto scope=documentation.scope;
            if(!documentation.destination.empty()) {
                try {link=parse_target(documentation.destination);}catch(const std::exception&){return false;}
                if(scope==AddressScope::Any)return skip("Its redirect to "+link.url()+" is outside the entered page's address scope; not requested");
            } else scope=asset_scope::other_host_scope(link,from,documentation.address,options.proxy.enabled);
            if(page_attempts.contains(link.url()))return skip("Already read with the entered site's pages");
            if(!web_addresses.contains(link.host)) {
                if(scope==AddressScope::Any)return skip("Linked host is outside the entered page's address scope; not requested");
                web_addresses.try_emplace(link.host);followed_hosts.emplace(link.host,scope);
            }
            if(refused_hosts.contains(link.host))return skip("Linked host was refused earlier in this inspection");
            if(!remaining_ms())return skip("30-second web evidence time limit reached before the linked documentation page");
            for(const auto& f:report.findings)linked_known.insert(f.name);
            for(const auto& v:report.vendors)linked_known.insert(v.name);
            linked_web_begin=report.web.size();linked_api_begin=report.api_references.size();
            report.notes.emplace_back("The entered page links to its documentation at "+link.url()+". That one page is read as well, after the entered site; names first seen there keep that page as their source.");
            event(ScanStage::Web,ActivityState::Running,"Linked page "+link.url(),"Reading the documentation page linked from "+documentation.page);
            WebEndpoint work{link,{},{},false,{},false};
            work.linked=true;
            endpoints.push_back(std::move(work));
            report.linked_pages.push_back(std::move(linked));
            return true;
        };
        size_t linked_requests=0;
        const auto attempt_available=[&](const WebEndpoint& work) {
            return work.linked?!cancel&&linked_requests<3&&remaining_ms()>0:page_available();
        };
        for (int pass=0;pass<4&&!cancel;++pass) {
        if(pass==3&&!begin_linked_page())break;
        for(auto& work:endpoints) {
            if(work.linked!=(pass==3))continue;
            if(!attempt_available(work))break;
            const auto& seed=work.seed;
            auto& visited=work.visited;
            auto& queue=work.queue;
            auto& browser_read=work.browser_read;
            auto enqueue = [&](std::string url) {
                if (url.empty() || url.size() > 2048) return;
                const auto hash = url.find('#');
                if (hash != std::string::npos) url.resize(hash);
                try {if(disclosure_seen.contains(parse_target(url).url()))return;}catch(const std::exception&){return;}
                if (!visited.insert(url).second) return;
                const int priority=url==seed.url()?100:crawl_priority(url);
                queue.emplace(priority,std::move(url));
            };
            if(pass==0||pass==3) {
                enqueue(seed.url());
                if(!work.linked)for (const char* seed_path : {"/robots.txt", "/sitemap.xml", "/sitemap_index.xml"}) {
                    Target tip = seed;
                    tip.path = seed_path;
                    enqueue(tip.url());
                }
            }
            if(pass==2)for(const char* probe_path : {"/favicon.ico", "/security.txt", "/.well-known/security.txt",
                    "/swagger.json", "/openapi.json", "/v2/api-docs", "/v3/api-docs", "/graphql"}) {
                Target tip=seed;tip.path=probe_path;enqueue(tip.url());
            }
            while (!queue.empty() && attempt_available(work)) {
                Target endpoint;
                try { endpoint = parse_target(queue.begin()->second); }
                catch (const std::exception&) { queue.erase(queue.begin()); continue; }
                queue.erase(queue.begin());
                if(work.has_moved&&endpoint.host==seed.host&&endpoint.url()!=seed.url()) {
                    endpoint.host=work.moved.host;endpoint.scheme=work.moved.scheme;endpoint.port=work.moved.port;endpoint.is_ip=work.moved.is_ip;
                    if(!visited.insert(endpoint.url()).second)continue;
                }
                if (!web_addresses.contains(endpoint.host)) continue;
                if (refused_hosts.contains(endpoint.host)) continue;
                if (endpoint.port != (work.has_moved&&endpoint.host==work.moved.host?work.moved.port:seed.port)) continue;
                if (seed.scheme == "https" && endpoint.scheme != "https") continue;
                if (endpoint.port == 9100) continue;
                const bool entry_chain=endpoint.url()==seed.url();
                const bool documentation_chain=!work.linked&&documentation_open&&endpoint.url()==documentation.url;
                std::set<std::string> redirect_chain{endpoint.url()};
                for (int hops = 0; hops < 4 && attempt_available(work); ++hops) {

                    if(!page_attempts.insert(endpoint.url()).second) {
                        if(browser_attempts.contains(endpoint.url()))browser_read=true;
                        event(ScanStage::Web,ActivityState::Skipped,endpoint.url(),"Already collected or attempted in this inspection");
                        break;
                    }
                    const int request_timeout=std::min(options.http_timeout_ms,remaining_ms());
                    if(!request_timeout)break;
                    if(work.linked)++linked_requests;else ++coverage.page_requests;
                    web_progress();
                    update(0.58f + 0.38f * static_cast<float>(coverage.page_requests) / static_cast<float>(WebCoverage::page_limit),
                        "Reading " + endpoint.url());
                    event(ScanStage::Web,ActivityState::Running,endpoint.url(),endpoint.scheme == "https" ? "TLS handshake and HTTP response" : "Waiting for the HTTP response");
                    auto& page_address=web_addresses.at(endpoint.host);
                    const auto followed=followed_hosts.find(endpoint.host);
                    const bool followed_host=followed!=followed_hosts.end();
                    bool scope_blocked=false;
                    auto observation = followed_host
                        ? fetch(endpoint,page_address,request_timeout,false,cancel,options.proxy,1024*1024,{},followed->second,&scope_blocked)
                        : fetch(endpoint,page_address,request_timeout,options.allow_untrusted_tls,cancel,options.proxy,1024*1024);
                    if(scope_blocked) {
                        refused_hosts.insert(endpoint.host);
                        report.notes.emplace_back("Redirect destination "+endpoint.host+" resolves outside the entered address's scope; not requested.");
                    }
                    if(page_address.text.empty()&&!observation.address.empty())page_address.text=observation.address;
                    const auto from_address=observation.address;
                    const auto next = observation.redirect;
                    const auto code = observation.status;
                    const auto path_lower = lower(endpoint.path);
                    const bool looks_robots = path_lower == "/robots.txt" || path_lower.ends_with("/robots.txt");
                    const bool looks_sitemap = path_lower.find("sitemap") != std::string::npos;
                    std::string content_type;
                    for (const auto& [key, value] : observation.headers) if (key == "content-type") { content_type = lower(value); break; }

                    const bool html_document = [&] {
                        if (!content_type.starts_with("text/html")) return false;
                        const auto start = observation.body.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
                        if (start == std::string::npos) return false;
                        const auto head = lower(observation.body.substr(start,15));
                        return head.starts_with("<!doctype html") || head.starts_with("<html");
                    }();
                    const bool xml_body = content_type.find("xml") != std::string::npos || (looks_sitemap && !html_document);
                    if (observation.status > 0) {
                        event(ScanStage::Web,ActivityState::Running,endpoint.url(),"Response received; analyzing the page and linked scripts");
                        if (looks_robots) {
                            for (const auto& map : robots_sitemap_urls(observation.url, observation.body)) enqueue(map);
                        } else if (xml_body) {
                            for (const auto& loc : sitemap_page_urls(observation.url, observation.body)) enqueue(loc);
                        } else {

                            const bool looks_favicon = path_lower == "/favicon.ico" || path_lower.ends_with("/favicon.ico");
                            const bool favicon_type = content_type.find("image/") != std::string::npos
                                || content_type.find("icon") != std::string::npos
                                || content_type.find("octet-stream") != std::string::npos
                                || content_type.empty();
                            if (report.favicon.body.empty() && code >= 200 && code < 300 && !observation.body.empty()
                                && observation.body.size() <= 256 * 1024 && looks_favicon && favicon_type) {
                                report.favicon.url = observation.url;
                                report.favicon.content_type = content_type.empty() ? "image/x-icon" : content_type;
                                report.favicon.body = observation.body;
                            }
                            identify_http(observation,report.findings);
                            if(entry_chain&&!work.linked){entry_pages.insert(endpoint.url());consider_documentation(observation,from_address);}
                            collect_stylesheets(observation,endpoint,page_address);
                            if (!cancel && ntlm_needs_type1(observation)) {
                                event(ScanStage::Web,ActivityState::Running,endpoint.url(),"NTLM Type1 negotiate for directory identity");
                                const int ntlm_timeout=std::min(options.http_timeout_ms,remaining_ms());
                                if (ntlm_timeout) {
                                    auto challenge=fetch(endpoint,page_address,ntlm_timeout,options.allow_untrusted_tls,cancel,options.proxy,max_body,
                                        "Authorization: NTLM " + ntlm_type1_token());
                                    if (challenge.status > 0) identify_ntlm(challenge,report.findings);
                                }
                            }
                            const auto providers=identify_disclosures(observation,report);
                            if(providers) {
                                DisclosureSource source;source.url=observation.url;source.title=observation.title;source.state="collected";
                                source.reason="Provider records extracted from the collected document";source.checked_at=utc_now();source.status=observation.status;source.providers=providers;
                                report.disclosures.push_back(std::move(source));
                            }
                            queue_disclosures(observation);
                            const auto mime = trim(content_type.substr(0,content_type.find(';')));
                            if (!browser_read && (code>=400||(code>=200 && code<300)) && (mime=="text/html" || (mime.empty() && observation.body.find('<')!=std::string::npos)) && browser_attempts.insert(endpoint.url()).second) {
                                browser_read = true;
                                update(0.58f,"Executing page JavaScript in an isolated browser");
                                inspect_browser(endpoint,report,options,cancel,[&](const ScanEvent& e){event(e.stage,e.state,e.item,e.detail);},web_deadline,&observation,observe_document);
                            }

                            collect_disclosures();
                            const auto script_references=page_script_references(observation);

                            for (const bool other_host : {false,true}) for (const auto& script : script_references) {
                                if(cancel)break;
                                if(script_references_seen.contains(script))continue;
                                Target asset;bool valid=true;
                                try {asset=parse_target(script);}
                                catch(const std::exception&){valid=false;}
                                if((valid&&asset.host!=endpoint.host)!=other_host)continue;
                                if(report.scripts.size()>=512){report.script_references_truncated=true;break;}
                                script_references_seen.insert(script);
                                ScriptObservation read;read.url=script;read.referenced_by=observation.url;
                                read.state="skipped";
                                if(!valid){read.reason="Invalid script URL";report.scripts.push_back(std::move(read));continue;}
                                const auto scope=other_host?other_host_scope(asset,endpoint,observation):AddressScope::Any;
                                if(other_host&&scope==AddressScope::Any)read.reason="External host; URL signatures only";
                                else if(!other_host&&asset.port!=endpoint.port)read.reason="Different port; URL signatures only";
                                else if(endpoint.scheme=="https"&&asset.scheme!="https")read.reason="HTTPS downgrade not followed";
                                else if(asset.port==9100)read.reason="Port 9100 does not receive payloads";

                                else if(std::any_of(report.scripts.begin(),report.scripts.end(),[&](const ScriptObservation& s) {
                                        return s.collection=="browser"&&s.analyzed&&!s.body_truncated&&s.url==script; }))
                                    read.reason=other_host?"External host; already analyzed in the browser":"Already analyzed in the browser";
                                else if(other_host&&asset_hosts_failed.contains(asset_endpoint(asset)))read.reason="External host failed an earlier request; not retried";
                                else if(coverage.script_requests>=WebCoverage::script_limit) {
                                    read.reason=other_host?"Static script limit reached":"Same-host script limit reached";cap("40-script web request limit reached");
                                }
                                else if(!remaining_ms())read.reason="Web evidence time limit reached";
                                if(!read.reason.empty()) {
                                    report.scripts.push_back(std::move(read));continue;
                                }

                                const int script_timeout=std::min({options.http_timeout_ms,remaining_ms(),other_host?4000:options.http_timeout_ms});
                                if(!script_timeout){read.reason="Web evidence time limit reached";report.scripts.push_back(std::move(read));continue;}
                                ++coverage.script_requests;
                                const auto item="Script "+asset.url();
                                event(ScanStage::Web,ActivityState::Running,item,other_host?"Reading a declared external script for library signatures":"Reading a same-host script for library signatures");

                                bool blocked=false;
                                auto file=other_host?fetch(asset,Address{},script_timeout,false,cancel,options.proxy,max_body,{},scope,&blocked)
                                                    :fetch(asset,page_address,script_timeout,options.allow_untrusted_tls&&!followed_host,cancel,options.proxy);
                                if(other_host&&file.status==0&&!cancel)asset_hosts_failed.insert(asset_endpoint(asset));
                                read.status=file.status;read.body_bytes=file.body.size();read.body_truncated=file.body_truncated;
                                std::string mime;
                                for(const auto& [key,value]:file.headers)if(key=="content-type"){mime=lower(trim(value.substr(0,value.find(';'))));break;}
                                const bool javascript=mime.empty()||mime=="text/javascript"||mime=="application/javascript"||mime=="application/x-javascript"||mime=="text/ecmascript"||mime=="application/ecmascript"||mime=="text/plain"||mime=="application/octet-stream";
                                if(cancel){read.state="cancelled";read.reason="Cancelled";}
                                else if(blocked){read.state="skipped";read.reason="External host resolves outside the page's address scope; not requested";}
                                else if(file.status<200||file.status>=300||file.body.empty()) {
                                    read.state="failed";read.reason=file.error.empty()?"HTTP "+std::to_string(file.status)+"; no script analyzed":printable(file.error,300);
                                } else if(!javascript){read.state="skipped";read.reason="Response content type is not JavaScript: "+printable(mime,100);}
                                else {
                                    identify_javascript(file.url,file.body,report.findings);
                                    collect_api_references(file.url,file.body,report);
                                    read.analyzed=true;read.state=file.error.empty()?"analyzed":"partial";
                                    read.reason=file.error.empty()?(file.body_truncated?"Analyzed the first 512 KiB; response truncated":"Analyzed received script bytes"):printable(file.error,300);
                                }
                                event(ScanStage::Web,cancel?ActivityState::Cancelled:read.state=="failed"||read.state=="partial"?ActivityState::Failed:ActivityState::Complete,item,
                                    "HTTP "+std::to_string(read.status)+" / "+std::to_string(read.body_bytes)+" script bytes / "+read.reason);
                                report.scripts.push_back(std::move(read));
                            }
                            if(!work.linked)for (const auto& link : page_html_urls(observation)) enqueue(link);
                            identify_vendors(observation,report);
                        }
                        auto service = std::find_if(report.services.begin(),report.services.end(),[&](const Service& s) { return s.port == endpoint.port; });
                        if (service != report.services.end()&&endpoint.host==target.host) service->name = endpoint.scheme == "https" ? "HTTPS" : "HTTP";
                    }

                    if(!browser_read&&!looks_robots&&!xml_body&&(code==0||code>=400)&&!scope_blocked&&(!followed_host||!observation.address.empty())&&browser_attempts.insert(endpoint.url()).second) {
                        browser_read=true;
                        if(entry_chain&&!work.linked)entry_pages.insert(endpoint.url());
                        inspect_browser(endpoint,report,options,cancel,[&](const ScanEvent& e){event(e.stage,e.state,e.item,e.detail);},web_deadline,followed_host?&observation:nullptr,observe_document);
                        collect_disclosures();
                    }
                    event(ScanStage::Web,cancel ? ActivityState::Cancelled : observation.error.empty() ? ActivityState::Complete : ActivityState::Failed,endpoint.url(),
                        observation.error.empty() ? "HTTP " + std::to_string(code) + " / " + std::to_string(observation.body.size()) + " body bytes" : printable(observation.error,300));
                    report.web.push_back(std::move(observation));
                    if (next.empty() || code < 300 || code >= 400) break;
                    try {
                        auto next_target = parse_target(next);
                        auto follow_scope=AddressScope::Any;
                        const bool other_site=!same_website_host(target,next_target)&&!followed_hosts.contains(next_target.host);
                        if (other_site) {
                            if(entry_chain&&followed_hosts.size()<(work.linked?4U:2U)&&!refused_hosts.contains(next_target.host)&&!from_address.empty())
                                follow_scope=asset_scope::other_host_scope(next_target,endpoint,from_address,options.proxy.enabled);
                            if(documentation_chain&&next_target.port==endpoint.port&&next_target.port!=9100&&!(endpoint.scheme=="https"&&next_target.scheme!="https")) {
                                documentation.destination=next_target.url();
                                documentation.scope=from_address.empty()?AddressScope::Any:asset_scope::other_host_scope(next_target,endpoint,from_address,options.proxy.enabled);
                                if(documentation.scope!=AddressScope::Any)break;
                            }
                            if(follow_scope==AddressScope::Any) {
                                report.notes.emplace_back((entry_chain?"Redirect to another host recorded but not followed (outside the entered address's scope or past the two-host limit): "
                                                                      :"External redirect recorded but not followed: ") + next);
                                break;
                            }
                        }
                        if (next_target.port == 9100 || (next_target.port != seed.port && !web_port(next_target.port))) {
                            report.notes.emplace_back("Redirect to a non-web port recorded but not followed: " + next);
                            break;
                        }
                        if (endpoint.scheme == "https" && next_target.scheme == "http") {
                            report.notes.emplace_back("HTTPS downgrade redirect recorded but not followed: " + next);
                            break;
                        }
                        if (!redirect_chain.insert(next_target.url()).second) {
                            report.notes.emplace_back("Stopped a redirect loop at " + next);
                            break;
                        }
                        if(other_site) {
                            followed_hosts.emplace(next_target.host,follow_scope);
                            work.moved=next_target;work.has_moved=true;
                            report.notes.emplace_back("Following the site's redirect to " + next_target.host +
                                "; the destination is resolved separately, must stay in the entered address's scope, and web evidence keeps its actual URL and address. The entered endpoint is unchanged.");
                        }
                        if(web_addresses.try_emplace(next_target.host).second&&!followed_hosts.contains(next_target.host))
                            report.notes.emplace_back("Following www website redirect to " + next_target.host +
                                "; destination DNS is resolved separately and web evidence retains its actual URL and address. The original endpoint is unchanged.");
                        visited.insert(next_target.url());
                        endpoint = std::move(next_target);
                        if (hops == 3) report.notes.emplace_back("Redirect limit reached at " + next);
                    } catch (const std::exception&) { report.notes.emplace_back("Invalid redirect was not followed."); break; }
                }
                if(pass==0||pass==3)break;
            }
            if (!cancel) web_progress();
        }
        }
        if(!report.linked_pages.empty()&&report.linked_pages.back().state.empty()) {
            auto& linked=report.linked_pages.back();
            const HttpObservation* last=nullptr;
            for(size_t i=linked_web_begin;i<report.web.size();++i)last=&report.web[i];
            linked.requests=static_cast<int>(linked_requests);
            if(report.api_references.size()>linked_api_begin)report.api_references.resize(linked_api_begin);
            if(!last){linked.state="skipped";linked.reason=cancel?"Cancelled before the linked documentation page":"30-second web evidence time limit reached before the linked documentation page";}
            else {
                linked.url=last->url;linked.status=last->status;
                const bool collected=last->status>=200&&last->status<300,moved=last->status>=300&&last->status<400;
                linked.state=collected?"collected":moved?"redirect":"failed";
                linked.reason=collected?"Read after the entered site":moved?"Its redirect to "+printable(last->redirect,300)+" was recorded and not followed":
                    last->error.empty()?"HTTP "+std::to_string(last->status):printable(last->error,300);
                const auto first=[&](const std::string& name) {
                    if(name.empty()||name=="Unknown"||name=="Unattributed origin"||linked_known.contains(name))return;
                    if(std::find(linked.first_seen.begin(),linked.first_seen.end(),name)==linked.first_seen.end())linked.first_seen.push_back(name);
                };
                if(collected) {
                    for(const auto& f:report.findings)
                        if(f.category!="Fingerprint"&&f.category!="Service"&&f.category!="Package"&&f.category.find("hint")==std::string::npos)first(f.name);
                    for(const auto& v:report.vendors)if(v.published_by.empty())first(v.name);
                }
            }
            event(ScanStage::Web,linked.state=="collected"?ActivityState::Complete:linked.state=="skipped"?ActivityState::Skipped:ActivityState::Failed,
                "Linked page "+linked.requested_url,linked.reason);
        }
        if(options.browser_runtime&&!cancel&&report.browser.state=="not_run")
            report.browser.reason=http_ports.empty()?"No eligible open web endpoint":"No eligible HTML page reached; inspect recorded redirects and HTTP outcomes";
        if(coverage.started)coverage.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-web_started).count();
        if(coverage.started&&!cancel&&Clock::now()>=web_deadline)remaining_ms();
        if (!cancel && !http_ports.empty()) {
            const auto responses = std::count_if(report.web.begin(),report.web.end(),[](const auto& h) { return h.status > 0; });
            const auto failures = std::count_if(report.web.begin(),report.web.end(),[](const auto& h) { return !h.error.empty(); });
            auto detail = std::to_string(responses) + (responses == 1 ? " HTTP response collected" : " HTTP responses collected");
            if (failures) detail += "; " + std::to_string(failures) + (failures == 1 ? " request failed" : " requests failed");
            if(coverage.limited)detail+="; collection capped, partial web coverage";
            event(ScanStage::Web,ActivityState::Complete,{},detail,coverage.page_requests,WebCoverage::page_limit);
        }
        if (!cancel && !options.proxy.enabled && !options.web_only && !target.explicit_port && !target.explicit_scheme && target.path == "/" && options.ports.empty()) {
            update(0.96f, "Checking SSDP, mDNS and SNMP");
            event(ScanStage::Discovery,ActivityState::Running,{},"Sending SSDP, mDNS and SNMP probes; awaiting replies",0,3);
            size_t discovery_checked = 0;
            discover_udp(report, options.timeout_ms, cancel,[&](const ScanEvent& e) {
                event(e.stage,e.state,e.item,e.detail);
                if (e.state == ActivityState::Complete) event(ScanStage::Discovery,ActivityState::Running,{},"Checking discovery replies",++discovery_checked,3);
            });
            if (!cancel) {
                discover_snmp_router(report, options.timeout_ms, cancel,[&](const ScanEvent& e) {
                    event(e.stage,e.state,e.item,e.detail);
                });
            }
            if (!cancel) {
                discover_router_hosts(report, options.timeout_ms, cancel,[&](const ScanEvent& e) {
                    event(e.stage,e.state,e.item,e.detail);
                });
            }
            if (!cancel) event(ScanStage::Discovery,ActivityState::Complete,{},"Device discovery finished; replies retained when available",discovery_checked,3);
        }
        else if (!cancel) event(ScanStage::Discovery,ActivityState::Skipped,{},options.proxy.enabled?"UDP discovery is unavailable through the configured TCP proxy":"Only enabled for a full host inspection");
        auto open = std::count_if(report.services.begin(),report.services.end(),[](const Service& s) { return s.state=="open"; });
        if (open == 0 && !cancel) report.notes.emplace_back("No open ports observed in this selection. This does not prove the host is offline.");
        if (report.findings.empty() && !cancel) report.notes.emplace_back("No signature matched the collected responses. Unrecognized software can still be present.");
    } catch (const std::exception& e) { report.error = e.what(); report.status = "error"; }
    return finish();
}
}
