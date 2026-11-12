#pragma once
#include "vector/model.hpp"
#include <curl/curl.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif
#include <string>





namespace vectorapp::asset_scope {
enum class AddressScope { Any, Public, Loopback };
inline bool loopback_address(const std::string& text) {
    return text == "::1" || text.starts_with("127.") || text.starts_with("::ffff:127.");
}
inline bool address_in_scope(AddressScope scope, const std::string& text) {
    return scope == AddressScope::Public ? geolocation_eligible(text) : scope == AddressScope::Loopback ? loopback_address(text) : true;
}


inline bool public_host_form(const Target& t) {
    if (t.is_ip) return geolocation_eligible(t.host);
    const auto host = t.host.ends_with('.') ? t.host.substr(0, t.host.size() - 1) : t.host;
    if (host.find('.') == std::string::npos) return false;
    for (const char* suffix : {".localhost", ".local", ".internal", ".lan", ".home", ".corp", ".intranet", ".home.arpa", ".test", ".example", ".invalid", ".onion"})
        if (host.ends_with(suffix)) return false;
    return true;
}



inline AddressScope other_host_scope(const Target& asset, const Target& endpoint, const std::string& page_address, bool proxied) {
    if (proxied) return public_host_form(endpoint) && public_host_form(asset) ? AddressScope::Public : AddressScope::Any;
    if (geolocation_eligible(page_address)) return asset.is_ip && !geolocation_eligible(asset.host) ? AddressScope::Any : AddressScope::Public;
    if (loopback_address(page_address)) return (asset.is_ip ? loopback_address(asset.host) : asset.host == "localhost") ? AddressScope::Loopback : AddressScope::Any;
    return AddressScope::Any;
}
struct ScopeGuard {
    AddressScope scope = AddressScope::Any;
    bool blocked = false;
};



inline curl_socket_t open_scoped_socket(void* user, curlsocktype, curl_sockaddr* address) {
    auto& guard = *static_cast<ScopeGuard*>(user);
    char text[NI_MAXHOST]{};
    const bool numeric = getnameinfo(&address->addr, static_cast<socklen_t>(address->addrlen), text, sizeof(text), nullptr, 0, NI_NUMERICHOST) == 0;
    if (!numeric || !address_in_scope(guard.scope, text)) { guard.blocked = true; return CURL_SOCKET_BAD; }
    return socket(address->family, address->socktype, address->protocol);
}
}
