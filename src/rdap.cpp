#include "proxy_transport.hpp"
#include "vector/model.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <regex>

namespace vectorapp {
namespace {
struct Buffer {
    std::string data;
    std::atomic_bool* cancel = nullptr;
};
size_t write_body(char* data, size_t size, size_t count, void* user) {
    auto& buffer = *static_cast<Buffer*>(user);
    if (buffer.cancel && *buffer.cancel) return 0;
    const size_t bytes = size * count;
    if (buffer.data.size() + bytes > 1024 * 1024) return 0;
    buffer.data.append(data, bytes);
    return bytes;
}
int progress_cb(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return *static_cast<std::atomic_bool*>(user) ? 1 : 0;
}
std::string text_of(const nlohmann::json& value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_array()) {
        std::string out;
        for (const auto& part : value) {
            if (!part.is_string()) continue;
            if (!out.empty()) out.push_back(' ');
            out += part.get<std::string>();
        }
        return out;
    }
    return {};
}
void add_contact(RegistryRecord& record, std::string role, std::string name, std::string email, std::string phone) {
    email = trim(std::move(email));
    name = trim(std::move(name));
    phone = trim(std::move(phone));
    if (email.empty() && name.empty() && phone.empty()) return;
    for (const auto& old : record.contacts)
        if (old.role == role && old.email == email && old.name == name) return;
    record.contacts.push_back(RegistryContact{std::move(role), std::move(name), std::move(email), std::move(phone)});
}
void read_vcard(const nlohmann::json& entity, std::string role, RegistryRecord& record) {
    if (!entity.contains("vcardArray") || !entity["vcardArray"].is_array() || entity["vcardArray"].size() < 2) return;
    const auto& props = entity["vcardArray"][1];
    if (!props.is_array()) return;
    std::string name, email, phone;
    for (const auto& prop : props) {
        if (!prop.is_array() || prop.size() < 4 || !prop[0].is_string()) continue;
        const auto key = lower(prop[0].get<std::string>());
        const auto value = text_of(prop[3]);
        if (value.empty()) continue;
        if (key == "fn" || (key == "org" && name.empty())) name = value;
        else if (key == "email") email = value;
        else if (key == "tel" && phone.empty()) phone = value;
    }
    add_contact(record, std::move(role), name, email, phone);
}
void walk_entities(const nlohmann::json& node, RegistryRecord& record, int depth) {
    if (depth > 4 || !node.is_object() || !node.contains("entities") || !node["entities"].is_array()) return;
    for (const auto& entity : node["entities"]) {
        if (!entity.is_object()) continue;
        std::string role;
        if (entity.contains("roles") && entity["roles"].is_array()) {
            for (const auto& item : entity["roles"]) {
                if (!item.is_string()) continue;
                const auto next = lower(item.get<std::string>());
                if (role.empty() || next == "abuse") role = next;
            }
        }
        read_vcard(entity, role, record);
        walk_entities(entity, record, depth + 1);
    }
}
void add_asn(RegistryRecord& record, const std::string& number) {
    if (number.empty()) return;
    const auto token = "AS" + number;
    if (record.asn.empty()) record.asn = token;
    else if (record.asn.find(token) == std::string::npos) record.asn += ", " + token;
}
bool ipv4_parts(const std::string& address, int out[4]) {
    int consumed = 0;
    if (std::sscanf(address.c_str(), "%d.%d.%d.%d%n", &out[0], &out[1], &out[2], &out[3], &consumed) != 4) return false;
    if (static_cast<size_t>(consumed) != address.size()) return false;
    for (int i = 0; i < 4; ++i) if (out[i] < 0 || out[i] > 255) return false;
    return true;
}
}

RegistryRecord parse_rdap_json(const std::string& body) {
    RegistryRecord record;
    nlohmann::json doc;
    try { doc = nlohmann::json::parse(body); }
    catch (const std::exception&) { record.error = "Registry response was not JSON."; return record; }
    if (!doc.is_object()) { record.error = "Registry response was not an object."; return record; }
    if (doc.contains("errorCode")) {
        record.error = doc.contains("title") && doc["title"].is_string() ? doc["title"].get<std::string>() : "No public registry record.";
        return record;
    }
    auto take = [&](const char* key) {
        return doc.contains(key) && doc[key].is_string() ? doc[key].get<std::string>() : std::string{};
    };
    record.handle = take("handle");
    record.name = take("name");
    record.type = take("type");
    record.country = take("country");
    if (doc.contains("status") && doc["status"].is_array()) {
        for (const auto& item : doc["status"]) {
            if (!item.is_string()) continue;
            if (!record.status.empty()) record.status += ", ";
            record.status += item.get<std::string>();
        }
    }
    if (doc.contains("startAddress") && doc.contains("endAddress") && doc["startAddress"].is_string() && doc["endAddress"].is_string())
        record.range = doc["startAddress"].get<std::string>() + "-" + doc["endAddress"].get<std::string>();
    if (doc.contains("cidr0_cidrs") && doc["cidr0_cidrs"].is_array()) {
        for (const auto& cidr : doc["cidr0_cidrs"]) {
            if (!cidr.is_object()) continue;
            std::string prefix;
            if (cidr.contains("v4prefix") && cidr["v4prefix"].is_string()) prefix = cidr["v4prefix"].get<std::string>();
            else if (cidr.contains("v6prefix") && cidr["v6prefix"].is_string()) prefix = cidr["v6prefix"].get<std::string>();
            if (prefix.empty() || !cidr.contains("length") || !cidr["length"].is_number()) continue;
            record.cidr = prefix + "/" + std::to_string(cidr["length"].get<int>());
            break;
        }
    }
    if (doc.contains("arin_originas0_originautnums") && doc["arin_originas0_originautnums"].is_array()) {
        for (const auto& asn : doc["arin_originas0_originautnums"])
            if (asn.is_number()) add_asn(record, std::to_string(asn.get<unsigned>()));
    }
    if (doc.contains("notices") && doc["notices"].is_array()) {
        for (const auto& notice : doc["notices"]) {
            if (!notice.is_object() || !notice.contains("title") || !notice["title"].is_string()) continue;
            if (notice["title"] != "Source" || !notice.contains("description") || !notice["description"].is_array()) continue;
            for (const auto& line : notice["description"]) {
                if (!line.is_string()) continue;
                const auto text = line.get<std::string>();
                if (text != "Objects returned came from source") record.source = text;
            }
        }
    }
    if (record.source.empty() && doc.contains("port43") && doc["port43"].is_string()) record.source = doc["port43"].get<std::string>();
    if (doc.contains("remarks") && doc["remarks"].is_array()) {
        std::string description;
        const std::regex email_re(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})");
        const std::regex asn_re(R"(\bAS([0-9]{1,10})\b)", std::regex::icase);
        for (const auto& remark : doc["remarks"]) {
            if (!remark.is_object() || !remark.contains("description") || !remark["description"].is_array()) continue;
            for (const auto& line : remark["description"]) {
                if (!line.is_string()) continue;
                const auto text = line.get<std::string>();
                if (text.find("---") != std::string::npos) continue;
                if (!description.empty()) description.push_back(' ');
                description += text;
                for (std::sregex_iterator it(text.begin(), text.end(), email_re), end; it != end; ++it)
                    add_contact(record, "noted", {}, it->str(), {});
                for (std::sregex_iterator it(text.begin(), text.end(), asn_re), end; it != end; ++it)
                    add_asn(record, (*it)[1].str());
            }
        }
        if (description.size() > 400) description.resize(400);
        record.description = std::move(description);
    }
    walk_entities(doc, record, 0);
    for (const auto& contact : record.contacts) {
        if (contact.role == "registrant" && record.org.empty()) record.org = contact.name;
        if (contact.role == "abuse" && !contact.email.empty() && record.abuse_email.empty()) record.abuse_email = contact.email;
    }
    if (record.org.empty()) {
        for (const auto& contact : record.contacts)
            if (!contact.name.empty() && contact.role != "noted") { record.org = contact.name; break; }
    }
    if (record.abuse_email.empty()) {
        for (const auto& contact : record.contacts)
            if (contact.role == "abuse" && !contact.email.empty()) { record.abuse_email = contact.email; break; }
    }
    record.scope = "public";
    return record;
}

RegistryRecord lookup_registry(const std::string& address, int timeout_ms, std::atomic_bool& cancel, const ProxyConfig& proxy) {
    RegistryRecord record;
    if (cancel) { record.error = "cancelled"; return record; }
    int parts[4]{};
    const bool v4 = ipv4_parts(address, parts);
    const bool special_v4 = v4 && (parts[0] == 0 || parts[0] == 10 || parts[0] == 127 || parts[0] >= 224 ||
        (parts[0] == 169 && parts[1] == 254) || (parts[0] == 192 && parts[1] == 168) ||
        (parts[0] == 172 && parts[1] >= 16 && parts[1] <= 31) || (parts[0] == 100 && parts[1] >= 64 && parts[1] <= 127));
    const auto lower_address = lower(address);
    const bool special_v6 = !v4 && (lower_address == "::1" || lower_address.rfind("fe80:", 0) == 0 ||
        lower_address.rfind("fc", 0) == 0 || lower_address.rfind("fd", 0) == 0 || lower_address.rfind("ff", 0) == 0);
    if (special_v4 || special_v6) {
        record.scope = "private";
        record.description = "No public allocation. Private, loopback, link-local, shared, and multicast addresses are not sent to a registry.";
        return record;
    }
    if (address.empty() || address.find_first_not_of("0123456789abcdefABCDEF.:") != std::string::npos) {
        record.error = "Address is not an IP, so no registry query was sent.";
        return record;
    }
    const std::string url = "https://rdap.arin.net/registry/ip/" + address;
    Buffer buffer{std::string(), &cancel};
    auto raw = curl_easy_init();
    if (!raw) { record.error = "Could not initialize the registry client."; return record; }
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, curl_easy_cleanup);
    char error[CURL_ERROR_SIZE]{};
    curl_easy_setopt(raw, CURLOPT_URL, url.c_str());
    configure_proxy(raw,proxy);
    curl_easy_setopt(raw, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(raw, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(raw, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(raw, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(raw, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(raw, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(timeout_ms));
    curl_easy_setopt(raw, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
    curl_easy_setopt(raw, CURLOPT_USERAGENT, "Vector/0.1 (RDAP registry lookup)");
    curl_easy_setopt(raw, CURLOPT_ACCEPT_ENCODING, "identity");
    curl_easy_setopt(raw, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(raw, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(raw, CURLOPT_XFERINFOFUNCTION, progress_cb);
    curl_easy_setopt(raw, CURLOPT_XFERINFODATA, &cancel);
    curl_easy_setopt(raw, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(raw, CURLOPT_ERRORBUFFER, error);
    const auto result = curl_easy_perform(raw);
    if (cancel) { record.error = "cancelled"; return record; }
    long status = 0;
    curl_easy_getinfo(raw, CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK) {
        record.error = proxy.enabled ? curl_easy_strerror(result) : error[0] ? error : curl_easy_strerror(result);
        return record;
    }
    if (status == 404) { record.error = "No public registry record."; return record; }
    if (status < 200 || status >= 300) { record.error = "Registry returned HTTP " + std::to_string(status) + "."; return record; }
    record = parse_rdap_json(buffer.data);
    if (record.scope.empty()) record.scope = "public";
    return record;
}
}
