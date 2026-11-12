#include "vector/model.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string_view>

namespace vectorapp {
namespace {
constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::string& bytes) {
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const unsigned a = static_cast<unsigned char>(bytes[i]);
        const unsigned b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
        const unsigned c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
        out += kAlphabet[a >> 2];
        out += kAlphabet[((a & 3) << 4) | (b >> 4)];
        out += i + 1 < bytes.size() ? kAlphabet[((b & 15) << 2) | (c >> 6)] : '=';
        out += i + 2 < bytes.size() ? kAlphabet[c & 63] : '=';
    }
    return out;
}

std::string base64_decode(const std::string& text) {
    std::string out;
    unsigned value = 0;
    int bits = -8;
    for (unsigned char c : text) {
        if (c == '=' || std::isspace(c)) {
            if (c == '=') break;
            continue;
        }
        const auto digit = kAlphabet.find(static_cast<char>(c));
        if (digit == std::string_view::npos) continue;
        value = (value << 6) | static_cast<unsigned>(digit);
        bits += 6;
        if (bits >= 0) {
            out += static_cast<char>((value >> bits) & 255);
            bits -= 8;
        }
        if (out.size() > 4096) break;
    }
    return out;
}

std::uint16_t read_u16(const std::string& data, size_t offset) {
    if (offset + 1 >= data.size()) return 0;
    return static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset]) |
        (static_cast<unsigned char>(data[offset + 1]) << 8));
}

std::uint32_t read_u32(const std::string& data, size_t offset) {
    if (offset + 3 >= data.size()) return 0;
    return static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset]) |
        (static_cast<unsigned char>(data[offset + 1]) << 8) |
        (static_cast<unsigned char>(data[offset + 2]) << 16) |
        (static_cast<unsigned char>(data[offset + 3]) << 24));
}

std::string utf16le_to_utf8(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() / 2);
    for (size_t i = 0; i + 1 < raw.size(); i += 2) {
        const unsigned code = static_cast<unsigned char>(raw[i]) | (static_cast<unsigned char>(raw[i + 1]) << 8);
        if (code == 0) break;
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) {
            out += static_cast<char>(0xc0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3f));
        } else {
            out += static_cast<char>(0xe0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (code & 0x3f));
        }
        if (out.size() > 256) break;
    }
    return out;
}

std::string windows_build_label(unsigned major, unsigned minor, unsigned build) {
    if (major == 10 && minor == 0) {
        if (build >= 22000) return "Windows 11 / Server 2022+";
        if (build >= 20348) return "Windows Server 2022";
        if (build >= 17763) return "Windows 10 / Server 2019";
        if (build >= 14393) return "Windows 10 / Server 2016";
        if (build >= 10240) return "Windows 10";
    }
    if (major == 6 && minor == 3) return "Windows 8.1 / Server 2012 R2";
    if (major == 6 && minor == 2) return "Windows 8 / Server 2012";
    if (major == 6 && minor == 1) return "Windows 7 / Server 2008 R2";
    if (major == 6 && minor == 0) return "Windows Vista / Server 2008";
    if (major || minor || build) return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
    return {};
}

bool url_has_port(const std::string& url, uint16_t port) {
    const auto needle = ":" + std::to_string(port);
    const auto pos = url.find(needle);
    if (pos == std::string::npos) return false;
    const auto after = pos + needle.size();
    return after >= url.size() || url[after] == '/' || url[after] == '?' || url[after] == '#';
}

struct NtlmNames {
    std::string nb_computer, nb_domain, dns_computer, dns_domain, dns_tree, os_label, os_build;
};

NtlmNames parse_type2_blob(const std::string& blob) {
    NtlmNames names;
    if (blob.size() < 48 || blob.compare(0, 8, "NTLMSSP\0", 8) != 0) return names;
    if (read_u32(blob, 8) != 2) return names;
    const auto flags = read_u32(blob, 20);
    const auto info_len = read_u16(blob, 40);
    const auto info_off = read_u32(blob, 44);
    if (info_len && info_off + info_len <= blob.size()) {
        size_t i = info_off;
        const size_t end = info_off + info_len;
        while (i + 4 <= end) {
            const auto av_id = read_u16(blob, i);
            const auto av_len = read_u16(blob, i + 2);
            i += 4;
            if (av_id == 0) break;
            if (i + av_len > end) break;
            const auto value = utf16le_to_utf8(blob.substr(i, av_len));
            i += av_len;
            if (value.empty()) continue;
            switch (av_id) {
                case 1: names.nb_computer = value; break;
                case 2: names.nb_domain = value; break;
                case 3: names.dns_computer = value; break;
                case 4: names.dns_domain = value; break;
                case 5: names.dns_tree = value; break;
                default: break;
            }
        }
    }
    if ((flags & 0x02000000u) && blob.size() >= 56) {
        const unsigned major = static_cast<unsigned char>(blob[48]);
        const unsigned minor = static_cast<unsigned char>(blob[49]);
        const unsigned build = read_u16(blob, 50);
        names.os_build = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
        names.os_label = windows_build_label(major, minor, build);
    }
    return names;
}

std::string extract_ntlm_token(const std::string& header) {
    const auto lower_header = lower(header);
    const auto pos = lower_header.find("ntlm");
    if (pos == std::string::npos) return {};
    size_t i = pos + 4;
    while (i < header.size() && std::isspace(static_cast<unsigned char>(header[i]))) ++i;
    if (i >= header.size()) return {};
    size_t end = i;
    while (end < header.size() && !std::isspace(static_cast<unsigned char>(header[end])) && header[end] != ',') ++end;
    return header.substr(i, end - i);
}
}

std::string ntlm_type1_token() {
    std::string message("NTLMSSP", 7);
    message.push_back('\0');
    message.append("\x01\x00\x00\x00", 4);
    message.append("\x97\x82\x08\xe2", 4);
    message.append(16, '\0');
    return base64_encode(message);
}

bool ntlm_offered(const HttpObservation& observation) {
    for (const auto& [key, value] : observation.headers) {
        if (key != "www-authenticate") continue;
        if (lower(value).find("ntlm") != std::string::npos) return true;
    }
    return false;
}

bool ntlm_type2_present(const HttpObservation& observation) {
    for (const auto& [key, value] : observation.headers) {
        if (key != "www-authenticate") continue;
        const auto token = extract_ntlm_token(value);
        if (token.size() < 16) continue;
        const auto blob = base64_decode(token);
        if (blob.size() >= 12 && blob.compare(0, 8, "NTLMSSP\0", 8) == 0 && read_u32(blob, 8) == 2) return true;
    }
    return false;
}

bool ntlm_needs_type1(const HttpObservation& observation) {
    return ntlm_offered(observation) && !ntlm_type2_present(observation);
}

void identify_ntlm(const HttpObservation& observation, std::vector<Finding>& findings) {
    const auto source = observation.url + " | www-authenticate";
    bool saw_offer = false;
    for (const auto& [key, value] : observation.headers) {
        if (key != "www-authenticate") continue;
        const auto lowered = lower(value);
        if (lowered.find("ntlm") == std::string::npos) continue;
        saw_offer = true;
        const auto token = extract_ntlm_token(value);
        if (token.size() < 16) continue;
        const auto blob = base64_decode(token);
        if (blob.size() < 48 || blob.compare(0, 8, "NTLMSSP\0", 8) != 0 || read_u32(blob, 8) != 2) continue;
        const auto names = parse_type2_blob(blob);
        add_finding(findings, {"NTLM authentication", "Protocol", {}, "High", {{source, printable(value, 300)}}});
        auto note = [&](std::string name, std::string category, const std::string& version, const std::string& evidence) {
            if (evidence.empty() && version.empty()) return;
            add_finding(findings, {std::move(name), std::move(category), version, "High",
                {{source, printable(evidence.empty() ? version : evidence, 300)}}});
        };
        note("NTLM NetBIOS computer", "Identity", names.nb_computer, names.nb_computer);
        note("NTLM NetBIOS domain", "Identity", names.nb_domain, names.nb_domain);
        note("NTLM DNS computer", "Identity", names.dns_computer, names.dns_computer);
        note("NTLM DNS domain", "Identity", names.dns_domain, names.dns_domain);
        note("NTLM DNS forest", "Identity", names.dns_tree, names.dns_tree);
        if (!names.os_label.empty())
            add_finding(findings, {"Windows", "OS hint", names.os_label, "Medium",
                {{source, printable(names.os_build.empty() ? names.os_label : names.os_build, 300)}}});
        else if (!names.os_build.empty())
            add_finding(findings, {"Windows", "OS hint", names.os_build, "Medium", {{source, printable(names.os_build, 300)}}});
    }
    if (saw_offer && !ntlm_type2_present(observation))
        add_finding(findings, {"NTLM authentication", "Protocol", {}, "Medium",
            {{source, "WWW-Authenticate: NTLM"}}});

    bool httpapi = false;
    for (const auto& [key, value] : observation.headers) {
        if (key == "server" && lower(value).find("microsoft-httpapi") != std::string::npos) httpapi = true;
    }
    if (httpapi && (url_has_port(observation.url, 5985) || url_has_port(observation.url, 5986))) {
        add_finding(findings, {"WinRM", "Service", {}, "High",
            {{observation.url + " | server", "Microsoft-HTTPAPI on WinRM port"}}});
    }
}
}
