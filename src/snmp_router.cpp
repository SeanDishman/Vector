#include "vector/model.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <sstream>
#include <vector>

namespace vectorapp {
namespace {
constexpr size_t max_arp_neighbors = 24;

std::string ber_length(size_t n) {
    if (n < 128) return std::string(1, static_cast<char>(n));
    if (n < 256) return std::string("\x81", 1) + static_cast<char>(n);
    return std::string("\x82", 1) + static_cast<char>((n >> 8) & 0xff) + static_cast<char>(n & 0xff);
}
std::string ber_sequence(const std::string& inner) {
    return std::string("\x30", 1) + ber_length(inner.size()) + inner;
}
void append_oid_subid(std::string& content, unsigned long value) {
    if (value < 128) {
        content.push_back(static_cast<char>(value));
        return;
    }
    unsigned char stack[10];
    size_t n = 0;
    stack[n++] = static_cast<unsigned char>(value & 0x7f);
    value >>= 7;
    while (value) {
        stack[n++] = static_cast<unsigned char>(0x80 | (value & 0x7f));
        value >>= 7;
    }
    while (n) content.push_back(static_cast<char>(stack[--n]));
}
bool parse_dotted_oid(const std::string& dotted, std::vector<unsigned>& parts) {
    parts.clear();
    if (dotted.empty()) return false;
    size_t start = 0;
    while (start <= dotted.size()) {
        const auto dot = dotted.find('.', start);
        const auto token = dotted.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (token.empty()) return false;
        unsigned value = 0;
        for (char c : token) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            value = value * 10u + static_cast<unsigned>(c - '0');
        }
        parts.push_back(value);
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    return parts.size() >= 2;
}
std::string oid_tlv_from_parts(const std::vector<unsigned>& parts) {
    if (parts.size() < 2) return {};
    std::string content(1, static_cast<char>(40 * parts[0] + parts[1]));
    for (size_t i = 2; i < parts.size(); ++i) append_oid_subid(content, parts[i]);
    return std::string("\x06", 1) + ber_length(content.size()) + content;
}
std::string oid_tlv_dotted(const std::string& dotted) {
    std::vector<unsigned> parts;
    if (!parse_dotted_oid(dotted, parts)) return {};
    return oid_tlv_from_parts(parts);
}
std::string snmp_pdu(char tag, const std::string& oid_tlv) {
    if (oid_tlv.empty()) return {};
    const auto binding = ber_sequence(oid_tlv + std::string("\x05\x00", 2));
    const auto binds = ber_sequence(binding);
    const std::string inner = std::string("\x02\x04\x00\x00\x00\x01\x02\x01\x00\x02\x01\x00", 12) + binds;
    const std::string pdu = std::string(1, tag) + ber_length(inner.size()) + inner;
    const auto message = std::string("\x02\x01\x01\x04\x06") + "public" + pdu;
    return ber_sequence(message);
}
bool read_ber_length(const std::string& raw, size_t& i, size_t& length) {
    if (i >= raw.size()) return false;
    const auto lead = static_cast<unsigned char>(raw[i++]);
    if (lead < 0x80) {
        length = lead;
        return true;
    }
    const size_t count = lead & 0x7f;
    if (!count || count > 2 || i + count > raw.size()) return false;
    length = 0;
    for (size_t n = 0; n < count; ++n) length = (length << 8) | static_cast<unsigned char>(raw[i++]);
    return true;
}
std::string decode_oid_content(const std::string& content) {
    if (content.empty()) return {};
    std::ostringstream out;
    const auto first = static_cast<unsigned char>(content[0]);
    out << (first / 40) << '.' << (first % 40);
    size_t i = 1;
    while (i < content.size()) {
        unsigned long value = 0;
        for (;;) {
            if (i >= content.size()) return {};
            const auto byte = static_cast<unsigned char>(content[i++]);
            value = (value << 7) | (byte & 0x7f);
            if ((byte & 0x80) == 0) break;
        }
        out << '.' << value;
    }
    return out.str();
}
struct SnmpVar {
    std::string oid;
    unsigned char tag = 0;
    std::string value;
};
bool parse_snmp_var(const std::string& raw, SnmpVar& out) {
    out = {};
    size_t after_oid = std::string::npos;
    for (size_t i = 0; i + 2 < raw.size(); ++i) {
        if (static_cast<unsigned char>(raw[i]) != 0x06) continue;
        size_t pos = i + 1, length = 0;
        if (!read_ber_length(raw, pos, length) || pos + length > raw.size()) continue;
        out.oid = decode_oid_content(raw.substr(pos, length));
        after_oid = pos + length;
        break;
    }
    if (out.oid.empty() || after_oid == std::string::npos || after_oid >= raw.size()) return false;
    size_t pos = after_oid;
    out.tag = static_cast<unsigned char>(raw[pos++]);
    size_t length = 0;
    if (!read_ber_length(raw, pos, length) || pos + length > raw.size()) return false;
    out.value = raw.substr(pos, length);
    return true;
}
std::string octet_text(const std::string& value) {
    if (value.empty() || value.size() > 255) return {};
    const bool textual = std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c < 127; });
    return textual ? value : std::string{};
}
std::string format_mac(const std::string& value) {
    if (value.size() != 6) return {};
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
        static_cast<unsigned char>(value[0]), static_cast<unsigned char>(value[1]),
        static_cast<unsigned char>(value[2]), static_cast<unsigned char>(value[3]),
        static_cast<unsigned char>(value[4]), static_cast<unsigned char>(value[5]));
    return buf;
}
std::string format_ipv4(const std::string& value) {
    if (value.size() != 4) return {};
    return std::to_string(static_cast<unsigned char>(value[0])) + "." +
           std::to_string(static_cast<unsigned char>(value[1])) + "." +
           std::to_string(static_cast<unsigned char>(value[2])) + "." +
           std::to_string(static_cast<unsigned char>(value[3]));
}
std::string ipv4_from_net_address_oid(const std::string& oid) {
    const std::string prefix = "1.3.6.1.2.1.4.22.1.3.";
    if (!oid.starts_with(prefix)) return {};
    unsigned a = 0, b = 0, c = 0, d = 0, ifindex = 0;
    char extra = 0;
    if (std::sscanf(oid.c_str() + prefix.size(), "%u.%u.%u.%u.%u%c", &ifindex, &a, &b, &c, &d, &extra) != 5) return {};
    if (a > 255 || b > 255 || c > 255 || d > 255) return {};
    return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(d);
}
std::string enterprise_name(unsigned long enterprise) {
    switch (enterprise) {
        case 9: return "Cisco";
        case 11: return "HP";
        case 43: return "3Com";
        case 232: return "ZyXEL";
        case 311: return "Microsoft";
        case 674: return "Dell";
        case 1411: return "ADTRAN";
        case 1991: return "Brocade";
        case 2011: return "Huawei";
        case 2021: return "Net-SNMP";
        case 2636: return "Juniper";
        case 3955: return "Linksys";
        case 4115: return "Arris";
        case 4413: return "Arris";
        case 4526: return "NETGEAR";
        case 5567: return "Force10";
        case 6876: return "VMware";
        case 8072: return "net-snmp";
        case 12356: return "Fortinet";
        case 14988: return "MikroTik";
        case 25053: return "Ruckus";
        case 25506: return "H3C";
        case 30065: return "Arista";
        case 41112: return "Ubiquiti";
        case 42359: return "Versa";
        default: return {};
    }
}
std::string enterprise_vendor(const std::string& oid) {
    const std::string prefix = "1.3.6.1.4.1.";
    if (!oid.starts_with(prefix)) return {};
    unsigned long enterprise = 0;
    for (size_t i = prefix.size(); i < oid.size() && std::isdigit(static_cast<unsigned char>(oid[i])); ++i)
        enterprise = enterprise * 10 + static_cast<unsigned long>(oid[i] - '0');
    return enterprise_name(enterprise);
}

bool read_ber_tlv(const std::string& raw, size_t& i, unsigned char& tag, std::string& value) {
    if (i >= raw.size()) return false;
    tag = static_cast<unsigned char>(raw[i++]);
    size_t length = 0;
    if (!read_ber_length(raw, i, length) || i + length > raw.size()) return false;
    value = raw.substr(i, length);
    i += length;
    return true;
}


struct SnmpV3Engine {
    std::string engine_id;
    unsigned long enterprise = 0;
    bool rfc3411 = false;
    unsigned format = 0;
    std::string format_data;
    unsigned long boots = 0;
    unsigned long engine_time = 0;
};
bool parse_snmpv3_engine(const std::string& raw, SnmpV3Engine& out) {
    out = {};
    size_t i = 0;
    unsigned char tag = 0;
    std::string outer;
    if (!read_ber_tlv(raw, i, tag, outer) || tag != 0x30) return false;
    size_t j = 0;
    std::string version_raw, global_data, security;
    if (!read_ber_tlv(outer, j, tag, version_raw) || tag != 0x02) return false;
    if (version_raw.size() != 1 || static_cast<unsigned char>(version_raw[0]) != 3) return false;
    if (!read_ber_tlv(outer, j, tag, global_data) || tag != 0x30) return false;
    if (!read_ber_tlv(outer, j, tag, security) || tag != 0x04) return false;
    size_t k = 0;
    std::string engine_id, boots_raw, time_raw;
    unsigned char first = 0;
    std::string first_val;
    if (!read_ber_tlv(security, k, first, first_val)) return false;
    if (first == 0x30) {
        size_t inner = 0;
        if (!read_ber_tlv(first_val, inner, tag, engine_id) || tag != 0x04) return false;
        if (!read_ber_tlv(first_val, inner, tag, boots_raw) || tag != 0x02) return false;
        if (!read_ber_tlv(first_val, inner, tag, time_raw) || tag != 0x02) return false;
    } else if (first == 0x04) {
        engine_id = std::move(first_val);
        if (!read_ber_tlv(security, k, tag, boots_raw) || tag != 0x02) return false;
        if (!read_ber_tlv(security, k, tag, time_raw) || tag != 0x02) return false;
    } else {
        return false;
    }
    if (engine_id.size() < 5 || engine_id.size() > 32) return false;
    out.engine_id = engine_id;
    auto read_uint = [](const std::string& bytes) -> unsigned long {
        unsigned long n = 0;
        for (unsigned char c : bytes) n = (n << 8) | c;
        return n;
    };
    out.boots = read_uint(boots_raw);
    out.engine_time = read_uint(time_raw);

    const auto b0 = static_cast<unsigned char>(engine_id[0]);
    const auto b1 = static_cast<unsigned char>(engine_id[1]);
    const auto b2 = static_cast<unsigned char>(engine_id[2]);
    const auto b3 = static_cast<unsigned char>(engine_id[3]);
    unsigned long ent = (static_cast<unsigned long>(b0) << 24) | (static_cast<unsigned long>(b1) << 16) |
                        (static_cast<unsigned long>(b2) << 8) | static_cast<unsigned long>(b3);
    out.rfc3411 = (ent & 0x80000000u) != 0;
    out.enterprise = out.rfc3411 ? (ent & 0x7fffffffu) : ent;
    if (out.rfc3411 && engine_id.size() >= 6) {
        out.format = static_cast<unsigned char>(engine_id[4]);
        const auto data = engine_id.substr(5);
        if (out.format == 1 && data.size() >= 4) out.format_data = format_ipv4(data.substr(0, 4));
        else if (out.format == 3 && data.size() >= 6) out.format_data = format_mac(data.substr(0, 6));
        else if (out.format == 4) out.format_data = octet_text(data);
        else if (!data.empty()) {
            std::ostringstream hex;
            for (size_t n = 0; n < data.size() && n < 16; ++n) {
                if (n) hex << ':';
                char buf[3];
                std::snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(data[n]));
                hex << buf;
            }
            out.format_data = hex.str();
        }
    }
    return !out.engine_id.empty();
}
std::string engine_id_hex(const std::string& id) {
    std::ostringstream hex;
    for (size_t n = 0; n < id.size(); ++n) {
        if (n) hex << ':';
        char buf[3];
        std::snprintf(buf, sizeof(buf), "%02x", static_cast<unsigned char>(id[n]));
        hex << buf;
    }
    return hex.str();
}
bool oid_under(const std::string& oid, const std::string& prefix) {
    return oid == prefix || (oid.size() > prefix.size() && oid.starts_with(prefix) && oid[prefix.size()] == '.');
}
SnmpVar snmp_query(const std::string& address, const std::string& request, int timeout_ms, std::atomic_bool& cancel) {
    SnmpVar var;
    if (request.empty() || cancel) return var;
    const auto reply = udp_exchange(address, 161, request, timeout_ms, cancel);
    if (reply.empty()) return var;
    parse_snmp_var(reply, var);
    return var;
}
}

std::string snmp_sysobjectid_probe() {
    return snmp_pdu('\xa0', oid_tlv_dotted("1.3.6.1.2.1.1.2.0"));
}
std::string snmp_sysname_probe() {
    return snmp_pdu('\xa0', oid_tlv_dotted("1.3.6.1.2.1.1.5.0"));
}
std::string snmp_getnext_probe(const std::string& dotted_oid) {
    return snmp_pdu('\xa1', oid_tlv_dotted(dotted_oid));
}
std::string snmp_v3_discovery_probe() {
    static const unsigned char bytes[] = {
        0x30, 0x3a, 0x02, 0x01, 0x03, 0x30, 0x0f, 0x02, 0x02, 0x4a, 0x69, 0x02, 0x03, 0x00, 0xff, 0xe3,
        0x04, 0x01, 0x04, 0x02, 0x01, 0x03, 0x04, 0x10, 0x30, 0x0e, 0x04, 0x00, 0x02, 0x01, 0x00, 0x02,
        0x01, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x30, 0x12, 0x04, 0x00, 0x04, 0x00, 0xa0, 0x0c,
        0x02, 0x02, 0x37, 0xf0, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x30, 0x00
    };
    return std::string(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}
bool snmp_v3_engine_info(const std::string& reply, unsigned long& enterprise, std::string& engine_hex, std::string& format_data) {
    SnmpV3Engine engine;
    if (!parse_snmpv3_engine(reply, engine)) return false;
    enterprise = engine.enterprise;
    engine_hex = engine_id_hex(engine.engine_id);
    format_data = engine.format_data;
    return true;
}

void discover_snmp_router(ScanReport& report, int timeout_ms, std::atomic_bool& cancel, const ScanObserver& observer) {
    if (report.address.empty() || cancel) return;

    const int wait = std::clamp(timeout_ms, 100, 700);
    const std::string source = "UDP 161 snmp";

    if (observer) observer({ScanStage::Discovery, ActivityState::Running, 0, "SNMPv3 engine",
        "Sending unsolicited SNMPv3 discovery for engineID"});
    const auto v3_reply = udp_exchange(report.address, 161, snmp_v3_discovery_probe(), wait, cancel);
    SnmpV3Engine engine;
    if (!cancel && !v3_reply.empty() && parse_snmpv3_engine(v3_reply, engine)) {
        auto ensure_snmp_service = [&] {
            const bool have = std::any_of(report.services.begin(), report.services.end(), [](const Service& s) {
                return s.port == 161 && s.protocol == "udp";
            });
            if (have) return;
            Service s;
            s.port = 161;
            s.protocol = "udp";
            s.name = "SNMP";
            s.expected = "SNMP";
            s.state = "open";
            s.active_probe = true;
            s.banner = printable(v3_reply, 200);
            report.services.push_back(std::move(s));
        };
        ensure_snmp_service();
        add_finding(report.findings, {"SNMPv3", "Protocol", {}, "High",
            {{source + " | version", "3"}}});
        add_finding(report.findings, {"SNMP engineID", "Device hint", {}, "High",
            {{source + " | engineID", engine_id_hex(engine.engine_id)}}});
        if (engine.enterprise) {
            const auto vendor = enterprise_name(engine.enterprise);
            std::string evidence = "enterprise " + std::to_string(engine.enterprise);
            if (!vendor.empty()) evidence += " (" + vendor + ")";
            add_finding(report.findings, {"SNMP enterprise", "Device hint", {}, "High",
                {{source + " | engineID enterprise", evidence}}});
            if (!vendor.empty()) {
                add_finding(report.findings, {vendor, "Device hint", {}, "High",
                    {{source + " | engineID enterprise", evidence}}});
                identify_hardware(vendor, source + " | engineID enterprise", report.findings);
            }
        }
        if (engine.format == 3 && !engine.format_data.empty()) {
            add_finding(report.findings, {"SNMP engine MAC", "Device hint", {}, "Medium",
                {{source + " | engineID MAC", engine.format_data}}});
        } else if (engine.format == 1 && !engine.format_data.empty()) {
            add_finding(report.findings, {"SNMP engine IPv4", "Device hint", {}, "Low",
                {{source + " | engineID IPv4", engine.format_data}}});
        } else if (engine.format == 4 && !engine.format_data.empty()) {
            add_finding(report.findings, {"SNMP engine text", "Device hint", {}, "Medium",
                {{source + " | engineID text", printable(engine.format_data, 120)}}});
            identify_hardware(engine.format_data, source + " | engineID text", report.findings);
        }
        if (engine.boots || engine.engine_time) {
            add_finding(report.findings, {"SNMP engine uptime", "Device hint", {}, "Low",
                {{source + " | boots/time",
                  "boots=" + std::to_string(engine.boots) + " time=" + std::to_string(engine.engine_time) + "s"}}});
        }
    }
    if (observer) observer({ScanStage::Discovery, cancel ? ActivityState::Cancelled : ActivityState::Complete, 0, "SNMPv3 engine",
        cancel ? "Interrupted by cancellation"
               : (v3_reply.empty() ? "No SNMPv3 reply"
                                   : (engine.engine_id.empty() ? "SNMPv3 reply not parsed" : "engineID captured"))});
    if (cancel) return;

    const bool snmp_open = std::any_of(report.services.begin(), report.services.end(), [](const Service& s) {
        return s.port == 161 && s.state == "open" && s.protocol == "udp";
    });
    if (!snmp_open) return;

    if (observer) observer({ScanStage::Discovery, ActivityState::Running, 0, "SNMP identity", "Reading sysObjectID and sysName"});

    auto object = snmp_query(report.address, snmp_sysobjectid_probe(), wait, cancel);
    if (!cancel && object.tag == 0x06 && !object.value.empty()) {
        const auto oid = decode_oid_content(object.value);
        if (!oid.empty()) {
            add_finding(report.findings, {"SNMP sysObjectID", "Device hint", {}, "High", {{source + " | sysObjectID", oid}}});
            if (const auto vendor = enterprise_vendor(oid); !vendor.empty()) {
                add_finding(report.findings, {vendor, "Device hint", {}, "Medium", {{source + " | enterprise", oid}}});
                identify_hardware(vendor, source + " | enterprise", report.findings);
            }
        }
    }

    auto name = snmp_query(report.address, snmp_sysname_probe(), wait, cancel);
    if (!cancel && (name.tag == 0x04 || name.tag == 0x44)) {
        const auto text = octet_text(name.value);
        if (!text.empty() && lower(text) != "public") {
            add_finding(report.findings, {"SNMP sysName", "Device hint", {}, "High", {{source + " | sysName", printable(text, 200)}}});
            identify_hardware(text, source + " | sysName", report.findings);
        }
    }

    if (observer) observer({ScanStage::Discovery, cancel ? ActivityState::Cancelled : ActivityState::Complete, 0, "SNMP identity",
        cancel ? "Interrupted by cancellation" : "sysObjectID and sysName checked"});
    if (cancel) return;

    if (observer) observer({ScanStage::Discovery, ActivityState::Running, 0, "SNMP ARP table", "Walking ipNetToMediaNetAddress"});

    std::string cursor = "1.3.6.1.2.1.4.22.1.3";
    const std::string column = "1.3.6.1.2.1.4.22.1.3";
    std::set<std::string> seen;

    for (size_t n = 0; !cancel && n < max_arp_neighbors; ++n) {
        auto var = snmp_query(report.address, snmp_getnext_probe(cursor), wait, cancel);
        if (var.oid.empty() || !oid_under(var.oid, column)) break;
        cursor = var.oid;

        std::string ip = ipv4_from_net_address_oid(var.oid);
        if (ip.empty() && (var.tag == 0x40 || var.tag == 0x04) && var.value.size() == 4) ip = format_ipv4(var.value);
        if (ip.empty() || !seen.insert(ip).second) continue;

        std::string mac;
        const auto phys_oid = std::string("1.3.6.1.2.1.4.22.1.2") + var.oid.substr(column.size());
        auto phys = snmp_query(report.address, snmp_pdu('\xa0', oid_tlv_dotted(phys_oid)), wait, cancel);
        if (phys.tag == 0x04) mac = format_mac(phys.value);

        std::string evidence = ip;
        if (!mac.empty()) evidence += " / " + mac;
        add_finding(report.findings, {ip, "LAN host (SNMP ARP)", {}, "Medium",
            {{source + " | ipNetToMediaTable", printable(evidence, 200)}}});
    }

    if (observer) observer({ScanStage::Discovery, cancel ? ActivityState::Cancelled : ActivityState::Complete, 0, "SNMP ARP table",
        cancel ? "Interrupted by cancellation"
               : (seen.empty() ? "No ARP neighbors returned" : std::to_string(seen.size()) + " ARP neighbor(s) from the gateway")});

    if (!seen.empty()) {
        report.notes.push_back(
            "SNMP ipNetToMediaTable listed " + std::to_string(seen.size()) +
            " neighbor IP(s) the gateway has ARP'd (community public). That is not a complete subnet map.");
    }
}
}
