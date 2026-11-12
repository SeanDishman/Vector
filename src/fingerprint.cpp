#include "vector/fingerprint.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string_view>

namespace vectorapp {
namespace {
uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
std::string sha256_12(std::string_view text) {
    static constexpr uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h0=0x6a09e667,h1=0xbb67ae85,h2=0x3c6ef372,h3=0xa54ff53a,h4=0x510e527f,h5=0x9b05688c,h6=0x1f83d9ab,h7=0x5be0cd19;
    std::string msg(text);
    const uint64_t bits = static_cast<uint64_t>(msg.size()) * 8;
    msg.push_back(static_cast<char>(0x80));
    while ((msg.size() % 64) != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i) msg.push_back(static_cast<char>((bits >> (i * 8)) & 0xff));
    for (size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[64]{};
        for (int i = 0; i < 16; ++i) {
            size_t p = off + static_cast<size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(static_cast<uint8_t>(msg[p])) << 24) | (static_cast<uint32_t>(static_cast<uint8_t>(msg[p+1])) << 16) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(msg[p+2])) << 8) | static_cast<uint8_t>(msg[p+3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
            uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,hh=h7;
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+S0+maj;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e; h5+=f; h6+=g; h7+=hh;
    }
    uint32_t words[8] = {h0,h1,h2,h3,h4,h5,h6,h7};
    static constexpr char hex[] = "0123456789abcdef";
    std::string out(12, '0');
    for (int i = 0; i < 6; ++i) {
        uint8_t byte = static_cast<uint8_t>(words[i / 4] >> (24 - 8 * (i % 4)));
        out[i * 2] = hex[byte >> 4];
        out[i * 2 + 1] = hex[byte & 0xf];
    }
    return out;
}
std::string hash_list(const std::vector<std::string>& items) {
    if (items.empty()) return "000000000000";
    std::string joined;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) joined.push_back(',');
        joined += items[i];
    }
    return sha256_12(joined);
}
bool grease(uint16_t v) { return (v & 0x0f0f) == 0x0a0a && ((v >> 8) & 0xf) == (v & 0xf); }
std::string hex4(uint16_t v) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out(4, '0');
    for (int i = 0; i < 4; ++i) out[i] = hex[(v >> (12 - 4 * i)) & 0xf];
    return out;
}
const char* tls_version(uint16_t v) {
    switch (v) {
        case 0x0304: return "13";
        case 0x0303: return "12";
        case 0x0302: return "11";
        case 0x0301: return "10";
        case 0x0300: return "s3";
        case 0x0002: return "s2";
        default: return "00";
    }
}
struct Span { const uint8_t* p; size_t n; };
bool der(Span& in, uint8_t& tag, Span& body) {
    if (in.n < 2) return false;
    tag = in.p[0];
    size_t len = in.p[1];
    size_t hdr = 2;
    if (len & 0x80) {
        size_t octets = len & 0x7f;
        if (octets == 0 || octets > 3 || in.n < 2 + octets) return false;
        len = 0;
        for (size_t i = 0; i < octets; ++i) len = (len << 8) | in.p[2 + i];
        hdr = 2 + octets;
    }
    if (in.n < hdr + len) return false;
    body = {in.p + hdr, len};
    in.p += hdr + len; in.n -= hdr + len;
    return true;
}
bool skip(Span& in) { uint8_t tag; Span body; return der(in, tag, body); }
std::string oid_body(Span body) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.resize(body.n * 2);
    for (size_t i = 0; i < body.n; ++i) {
        out[i * 2] = hex[body.p[i] >> 4];
        out[i * 2 + 1] = hex[body.p[i] & 0xf];
    }
    return out;
}
bool name_oids(Span name, std::vector<std::string>& oids) {
    while (name.n) {
        uint8_t tag; Span rdn;
        if (!der(name, tag, rdn) || tag != 0x31) return false;
        while (rdn.n) {
            Span atv;
            if (!der(rdn, tag, atv) || tag != 0x30) return false;
            Span oid;
            if (!der(atv, tag, oid) || tag != 0x06) return false;
            oids.push_back(oid_body(oid));
            if (atv.n && !skip(atv)) return false;
        }
    }
    return true;
}
std::string server_hello(Span body) {
    if (body.n < 35) return {};
    uint16_t legacy = (body.p[0] << 8) | body.p[1];
    body.p += 34; body.n -= 34;
    size_t sid = body.p[0];
    if (body.n < 1 + sid + 3) return {};
    body.p += 1 + sid; body.n -= 1 + sid;
    uint16_t cipher = (body.p[0] << 8) | body.p[1];
    body.p += 3; body.n -= 3;
    std::vector<std::string> extensions;
    std::string alpn = "00";
    uint16_t version = legacy;
    if (body.n >= 2) {
        size_t ext_len = (body.p[0] << 8) | body.p[1];
        body.p += 2; body.n -= 2;
        if (body.n < ext_len) return {};
        Span exts{body.p, ext_len};
        while (exts.n) {
            if (exts.n < 4) return {};
            uint16_t kind = (exts.p[0] << 8) | exts.p[1];
            size_t elen = (exts.p[2] << 8) | exts.p[3];
            exts.p += 4; exts.n -= 4;
            if (exts.n < elen) return {};
            Span data{exts.p, elen};
            exts.p += elen; exts.n -= elen;
            extensions.push_back(hex4(kind));
            if (kind == 0x002b && data.n >= 2) {
                uint16_t best = 0;
                if (data.n == 2) best = (data.p[0] << 8) | data.p[1];
                else {
                    size_t list = data.p[0];
                    Span vers{data.p + 1, std::min(list, data.n - 1)};
                    while (vers.n >= 2) {
                        uint16_t v = (vers.p[0] << 8) | vers.p[1];
                        vers.p += 2; vers.n -= 2;
                        if (!grease(v) && v > best) best = v;
                    }
                }
                if (best && !grease(best)) version = best;
            } else if (kind == 0x0010 && data.n >= 3) {
                size_t list = (data.p[0] << 8) | data.p[1];
                Span protos{data.p + 2, std::min(list, data.n - 2)};
                if (protos.n >= 1 && protos.p[0] > 0 && protos.n >= static_cast<size_t>(1 + protos.p[0])) {
                    char first = static_cast<char>(protos.p[1]);
                    char last = static_cast<char>(protos.p[protos.p[0]]);
                    if (static_cast<uint8_t>(first) >= 128) first = '9';
                    if (static_cast<uint8_t>(last) >= 128) last = '9';
                    alpn.clear(); alpn.push_back(first); alpn.push_back(last);
                }
            }
        }
    }
    if (extensions.size() > 99) extensions.resize(99);
    char count[3];
    count[0] = static_cast<char>('0' + extensions.size() / 10);
    count[1] = static_cast<char>('0' + extensions.size() % 10);
    count[2] = 0;
    return std::string("t") + tls_version(version) + count + alpn + "_" + hex4(cipher) + "_" + hash_list(extensions);
}
std::string decode_b64(std::string_view in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    int acc = 0, bits = 0;
    for (char c : in) {
        int d = val(c);
        if (d < 0) continue;
        acc = (acc << 6) | d;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<char>((acc >> bits) & 0xff)); }
    }
    return out;
}
uint32_t murmur3(std::string_view data) {
    uint32_t h = 0;
    const auto* p = reinterpret_cast<const uint8_t*>(data.data());
    size_t n = data.size(), i = 0;
    while (i + 4 <= n) {
        uint32_t k = p[i] | (p[i+1] << 8) | (p[i+2] << 16) | (p[i+3] << 24);
        k *= 0xcc9e2d51u; k = (k << 15) | (k >> 17); k *= 0x1b873593u;
        h ^= k; h = (h << 13) | (h >> 19); h = h * 5 + 0xe6546b64u;
        i += 4;
    }
    uint32_t k = 0;
    if (n & 3) {
        if ((n & 3) >= 3) k ^= p[i+2] << 16;
        if ((n & 3) >= 2) k ^= p[i+1] << 8;
        k ^= p[i];
        k *= 0xcc9e2d51u; k = (k << 15) | (k >> 17); k *= 0x1b873593u; h ^= k;
    }
    h ^= static_cast<uint32_t>(n);
    h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
    return h;
}
std::string b64_lines(std::string_view raw) {
    static constexpr char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string packed;
    unsigned acc = 0; int bits = 0;
    for (unsigned char c : raw) {
        acc = (acc << 8) | c; bits += 8;
        while (bits >= 6) { bits -= 6; packed.push_back(tab[(acc >> bits) & 63]); }
    }
    if (bits) packed.push_back(tab[(acc << (6 - bits)) & 63]);
    while (packed.size() % 4) packed.push_back('=');
    std::string out;
    for (size_t i = 0; i < packed.size(); i += 76) {
        out.append(packed, i, std::min<size_t>(76, packed.size() - i));
        out.push_back('\n');
    }
    if (out.empty()) out.push_back('\n');
    return out;
}
}
std::string tls_client_hello() {
    std::string ext;
    auto add16 = [&](unsigned v) { ext.push_back(static_cast<char>(v >> 8)); ext.push_back(static_cast<char>(v)); };
    auto extension = [&](unsigned type, const std::string& data) { add16(type); add16(static_cast<unsigned>(data.size())); ext += data; };
    extension(0x002b, std::string("\x04\x03\x04\x03\x03", 5));
    extension(0x000a, std::string("\x00\x04\x00\x1d\x00\x17", 6));
    extension(0x000d, std::string("\x00\x08\x04\x03\x08\x04\x04\x01\x02\x01", 10));
    extension(0x000b, std::string("\x01\x00", 2));
    extension(0x0010, std::string("\x00\x0c\x02h2\x08http/1.1", 14));
    std::string body;
    auto b16 = [&](unsigned v) { body.push_back(static_cast<char>(v >> 8)); body.push_back(static_cast<char>(v)); };
    b16(0x0303);
    body.append(32, '\x11');
    body.push_back(0);
    b16(16);
    const unsigned char ciphers[] = {0x13,0x01,0x13,0x02,0x13,0x03,0xc0,0x2f,0xc0,0x2b,0x00,0x9c,0x00,0x2f,0x00,0x35};
    body.append(reinterpret_cast<const char*>(ciphers), sizeof(ciphers));
    body.push_back(1);
    body.push_back(0);
    b16(static_cast<unsigned>(ext.size()));
    body += ext;
    std::string handshake;
    handshake.push_back(1);
    handshake.push_back(static_cast<char>((body.size() >> 16) & 0xff));
    handshake.push_back(static_cast<char>((body.size() >> 8) & 0xff));
    handshake.push_back(static_cast<char>(body.size() & 0xff));
    handshake += body;
    std::string record;
    record.push_back(0x16);
    record.push_back(0x03);
    record.push_back(0x01);
    record.push_back(static_cast<char>((handshake.size() >> 8) & 0xff));
    record.push_back(static_cast<char>(handshake.size() & 0xff));
    record += handshake;
    return record;
}
std::string ja4s_from_tls(const std::string& records) {
    Span in{reinterpret_cast<const uint8_t*>(records.data()), records.size()};
    while (in.n >= 5) {
        uint8_t type = in.p[0];
        size_t len = (in.p[3] << 8) | in.p[4];
        if (in.n < 5 + len) return {};
        Span frag{in.p + 5, len};
        in.p += 5 + len; in.n -= 5 + len;
        if (type != 0x16) continue;
        while (frag.n >= 4) {
            uint8_t ht = frag.p[0];
            size_t hl = (static_cast<size_t>(frag.p[1]) << 16) | (static_cast<size_t>(frag.p[2]) << 8) | frag.p[3];
            if (frag.n < 4 + hl) break;
            Span body{frag.p + 4, hl};
            frag.p += 4 + hl; frag.n -= 4 + hl;
            if (ht == 2) return server_hello(body);
        }
    }
    return {};
}
std::string ja4x_from_der(const std::string& der_bytes) {
    Span in{reinterpret_cast<const uint8_t*>(der_bytes.data()), der_bytes.size()};
    uint8_t tag; Span cert;
    if (!der(in, tag, cert) || tag != 0x30) return {};
    Span tbs;
    if (!der(cert, tag, tbs) || tag != 0x30) return {};
    if (tbs.n && tbs.p[0] == 0xa0 && !skip(tbs)) return {};
    if (!skip(tbs) || !skip(tbs)) return {};
    Span issuer, validity, subject, spki;
    if (!der(tbs, tag, issuer) || tag != 0x30) return {};
    if (!der(tbs, tag, validity) || tag != 0x30) return {};
    if (!der(tbs, tag, subject) || tag != 0x30) return {};
    if (!der(tbs, tag, spki) || tag != 0x30) return {};
    std::vector<std::string> issuer_oids, subject_oids, ext_oids;
    if (!name_oids(issuer, issuer_oids) || !name_oids(subject, subject_oids)) return {};
    while (tbs.n) {
        Span extra;
        if (!der(tbs, tag, extra)) return {};
        if (tag != 0xa3) continue;
        Span seq;
        if (!der(extra, tag, seq) || tag != 0x30) return {};
        while (seq.n) {
            Span ext;
            if (!der(seq, tag, ext) || tag != 0x30) return {};
            Span oid;
            if (!der(ext, tag, oid) || tag != 0x06) return {};
            ext_oids.push_back(oid_body(oid));
        }
    }
    return hash_list(issuer_oids) + "_" + hash_list(subject_oids) + "_" + hash_list(ext_oids);
}
std::int32_t favicon_hash(const std::string& bytes) {
    auto encoded = b64_lines(bytes);
    uint32_t h = murmur3(encoded);
    std::int32_t signed_hash = 0;
    std::memcpy(&signed_hash, &h, sizeof(signed_hash));
    return signed_hash;
}
std::string header_order_fingerprint(const std::vector<std::pair<std::string, std::string>>& headers) {
    std::vector<std::string> names;
    for (const auto& [name, value] : headers) {
        (void)value;
        if (!name.empty()) names.push_back(lower(name));
    }
    return hash_list(names);
}
void note_http_signals(const HttpObservation& observation, std::vector<Finding>& findings) {
    if (!observation.headers.empty()) {
        std::string order;
        std::vector<std::string> cookies;
        for (const auto& [name, value] : observation.headers) {
            if (!order.empty()) order.push_back(',');
            order += name;
            if (name == "set-cookie") {
                auto end = value.find('=');
                auto cookie = trim(value.substr(0, end));
                if (!cookie.empty() && std::find(cookies.begin(), cookies.end(), cookie) == cookies.end()) cookies.push_back(cookie);
            }
        }
        add_finding(findings, {"HTTP header order","Fingerprint",header_order_fingerprint(observation.headers),"Medium",
            {{observation.url + " | header order", printable(order, 300)}}});
        if (!cookies.empty()) {
            Finding cookie{"HTTP cookies","Fingerprint",{},"Low",{}};
            for (const auto& name : cookies) cookie.evidence.push_back({observation.url + " | set-cookie", name});
            add_finding(findings, std::move(cookie));
        }
        for (const auto& [name, value] : observation.headers) {
            if (name != "content-type") continue;
            auto type = lower(value);
            if (type.find("image/") == std::string::npos || observation.body.empty() || observation.body.size() > 256 * 1024) break;
            const auto hash = favicon_hash(observation.body);
            const auto hash_text = std::to_string(hash);
            add_finding(findings, {"Favicon hash","Fingerprint",hash_text,"Medium",
                {{observation.url + " | content-type", printable(value, 120)},
                 {observation.url + " | shodan", "http.favicon.hash:" + hash_text}}});
            struct Known { std::int32_t hash; const char* name; };
            static const Known known[] = {
                {-297069493, "Apache Tomcat"},
                {-305179312, "Atlassian Confluence"},
                {667017222, "Atlassian Bitbucket"},
                {1265477436, "GitLab"},
                {-267431135, "Kibana"},
                {1485257654, "SonarQube"},
                {1139788073, "Metasploit"},
                {-1235192469, "Metasploit"},
                {213144638, "Proxmox"},
                {-652508439, "Plesk"},
                {999357577, "Hikvision"},
                {-2013924196, "Fortinet FortiGate"},
            };
            for (const auto& entry : known) {
                if (entry.hash != hash) continue;
                add_finding(findings, {entry.name,"Application hint",{},"Medium",
                    {{observation.url + " | favicon hash", hash_text}}});
                break;
            }
            break;
        }
    }
    const auto path = lower(observation.url);
    if (path.find("security.txt") != std::string::npos && observation.status >= 200 && observation.status < 300) {
        std::istringstream stream(observation.body);
        std::string line;
        while (std::getline(stream, line)) {
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            auto colon = trimmed.find(':');
            if (colon == std::string::npos) continue;
            auto key = lower(trim(trimmed.substr(0, colon)));
            auto value = trim(trimmed.substr(colon + 1));
            if (value.empty()) continue;
            if (key == "contact" || key == "expires" || key == "encryption" || key == "policy" || key == "acknowledgments")
                add_finding(findings, {"security.txt","Disclosure",key,"High",{{observation.url + " | " + key, printable(value, 300)}}});
        }
    }
    if (observation.status >= 200 && observation.status < 300 && !observation.body.empty()) {
        const auto body = lower(observation.body.substr(0, std::min<size_t>(observation.body.size(), 8192)));
        if ((body.find("\"swagger\"") != std::string::npos || body.find("\"openapi\"") != std::string::npos) &&
            (body.find("\"paths\"") != std::string::npos || body.find("\"info\"") != std::string::npos))
            add_finding(findings, {"OpenAPI / Swagger","API surface",{},"High",
                {{observation.url + " | body", "OpenAPI or Swagger document markers"}}});
        if (body.find("\"graphql\"") != std::string::npos && (body.find("\"__schema\"") != std::string::npos || body.find("introspection") != std::string::npos))
            add_finding(findings, {"GraphQL","API surface",{},"Medium",
                {{observation.url + " | body", "GraphQL introspection markers"}}});
    }
    for (const auto& [name, value] : observation.certificate) {
        if (name != "pem") continue;
        auto der_bytes = decode_b64(value);
        auto fp = ja4x_from_der(der_bytes);
        if (!fp.empty()) add_finding(findings, {"JA4X","Fingerprint",fp,"High",{{observation.url + " | certificate", fp}}});
        break;
    }
}
void note_tls_banner(Service& service, std::vector<Finding>& findings) {
    if (service.banner.empty() || static_cast<unsigned char>(service.banner[0]) != 0x16) return;
    auto fp = ja4s_from_tls(service.banner);
    if (fp.empty()) return;
    service.name = "TLS";
    add_finding(findings, {"JA4S","Fingerprint",fp,"High",{{"TCP " + std::to_string(service.port) + " ServerHello", fp}}});
}
}
