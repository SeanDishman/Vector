#include "vector/fingerprint.hpp"
#include "vector/model.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace vectorapp;
int checks=0;
void check(bool ok,const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f,const char* message) { bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; } check(rejected,message); }
int main() {
    try {
        auto t=parse_target("https://Example.COM:8443/app?q=yes#fragment");
        check(t.host=="example.com" && t.port==8443 && t.path=="/app?q=yes","URL components and fragment handling");
        check(parse_target("127.0.0.1:8090").port==8090,"bare host with port");
        check(parse_target("localhost:8443").scheme=="https","infer HTTPS on a conventional TLS port");
        check(parse_target("::1").host=="::1","bare IPv6");
        check(parse_target("[::1]:8080").port==8080,"IPv6 with port");
        check(parse_target("192.0.2.42").is_ip,"IPv4 input");
        check(parse_target("localhost").url()=="http://localhost:80/","hostname normalization");
        check(same_website_host(parse_target("google.com"),parse_target("https://www.google.com/")),"follow exact www redirect");
        check(same_website_host(parse_target("www.example.co.uk"),parse_target("https://example.co.uk/")),"follow reverse www redirect");
        check(!same_website_host(parse_target("example.com"),parse_target("https://www.example.com.attacker.test/")),"reject www suffix deception");
        check(!same_website_host(parse_target("example.com"),parse_target("https://login.example.com/")),"www exception does not include arbitrary sibling hosts");
        check(!same_website_host(parse_target("127.0.0.1"),parse_target("http://www.127.0.0.1/")),"IP addresses do not infer www aliases");
        rejects([] {parse_target("file:///etc/passwd");},"reject file scheme");
        rejects([] {parse_target("https://user:secret@example.com");},"reject credentials");
        rejects([] {parse_target("example.com\r\nX-Test: bad");},"reject header injection");
        rejects([] {parse_target("example.com:0");},"reject zero port");
        rejects([] {parse_target("");},"reject empty input");
        check(parse_ports("22,80,443,8000-8002,22")==std::vector<uint16_t>({22,80,443,8000,8001,8002}),"deduplicate and expand ports");
        rejects([] {parse_ports("1-65535");},"bound port count");
        rejects([] {parse_ports("80,");},"reject empty port token");
        rejects([] {parse_ports("-1");},"reject negative port");
        rejects([] {parse_ports("443-80");},"reject descending range");
        HttpObservation h;
        h.url="http://test/";
        h.headers={{"server","nginx/1.26.2"},{"x-powered-by","PHP/8.3.12"}};
        h.body=R"(<meta content='WordPress 6.6.2' name='generator'><script src='/wp-includes/js/jquery/jquery-3.7.1.min.js'></script>)";
        std::vector<Finding> f;
        identify_http(h,f);
        auto has=[&](std::string name,std::string version="") { return std::any_of(f.begin(),f.end(),[&](const Finding& x) {return x.name==name && x.version==version;}); };
        check(has("nginx","1.26.2"),"server version");
        check(has("PHP","8.3.12"),"runtime version");
        check(has("WordPress","6.6.2"),"generator attributes can be reversed");
        check(has("jQuery","3.7.1"),"script library version");
        check(!f.front().evidence.empty(),"matches retain evidence");
        check(std::count_if(f.begin(),f.end(),[](const Finding& x){return x.name=="WordPress";})==1,"combine versioned and unversioned evidence");
        auto count=f.size(); identify_http(h,f); check(f.size()==count,"deduplicate repeat observations");
        f.clear(); h.headers.clear(); h.body="This article mentions React, nginx, WordPress, jquery.min.js, MikroTik and PHP.";
        identify_http(h,f); check(f.empty(),"product names in prose are not fingerprints");
        Service s; s.port=22022; s.banner="SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13\r\n";
        identify_banner(s,f);
        check(s.name=="SSH" && has("OpenSSH","9.6p1"),"identify SSH on nonstandard port");
        check(has("Ubuntu Linux"),"OS hint from banner");
        f.clear(); s.banner.clear(); s.port=22; s.name="Unknown"; s.active_probe=false; identify_banner(s,f);
        check(f.empty() && s.name=="Unknown","port number alone is not identification");
        f.clear(); s = Service{}; s.port=445; s.active_probe=true;
        s.banner = std::string("\xfeSMB",4) + std::string(66,'\0');
        s.banner[64]=65; s.banner[68]=0x11; s.banner[69]=0x03;
        identify_banner(s,f);
        check(s.name=="SMB" && has("SMB2","3.1.1"),"SMB2 dialect from a negotiate response");
        f.clear(); s = Service{}; s.port=3389; s.active_probe=true;
        s.banner = std::string("\x03\x00\x00\x13\x0e\xd0\x00\x00\x00\x00\x00\x02\x00\x08\x00\x01\x00\x00\x00",19);
        identify_banner(s,f);
        check(s.name=="RDP" && has("RDP","TLS"),"RDP negotiation response");
        f.clear(); s = Service{}; s.port=6379; s.active_probe=true; s.banner="+PONG\r\n";
        identify_banner(s,f);
        check(s.name=="Redis" && has("Redis"),"Redis PONG probe");
        f.clear(); s = Service{}; s.port=3000; s.active_probe=true;
        s.banner="HTTP/1.0 200 OK\r\nServer: nginx/1.26.2\r\n\r\n";
        identify_banner(s,f);
        check(s.name=="HTTP" && has("nginx","1.26.2"),"HTTP probe Server header");
        f.clear(); s = Service{}; s.port=445; identify_banner(s,f);
        check(f.empty() && s.name=="Unknown","SMB port alone is not identification");
        const auto snmp = snmp_sysdescr_probe();
        check(snmp.size() > 16 && static_cast<unsigned char>(snmp[0]) == 0x30 && snmp.find("public") != std::string::npos, "SNMPv2c sysDescr probe");
        check(snmp.find(std::string("\x2b\x06\x01\x02\x01\x01\x01\x00", 8)) != std::string::npos, "sysDescr OID");
        const auto object_id = snmp_sysobjectid_probe();
        check(object_id.find(std::string("\x2b\x06\x01\x02\x01\x01\x02\x00", 8)) != std::string::npos, "sysObjectID OID");
        const auto sysname = snmp_sysname_probe();
        check(sysname.find(std::string("\x2b\x06\x01\x02\x01\x01\x05\x00", 8)) != std::string::npos, "sysName OID");
        const auto arp_next = snmp_getnext_probe("1.3.6.1.2.1.4.22.1.3");
        check(arp_next.find(std::string("\xa1", 1)) != std::string::npos &&
              arp_next.find(std::string("\x2b\x06\x01\x02\x01\x04\x16\x01\x03", 9)) != std::string::npos,
            "ipNetToMediaNetAddress GetNext probe");
        const auto v3 = snmp_v3_discovery_probe();
        check(v3.size() == 60 && static_cast<unsigned char>(v3[0]) == 0x30 &&
              static_cast<unsigned char>(v3[4]) == 0x03 && v3.find(std::string("\x02\x02\x4a\x69", 4)) != std::string::npos,
            "SNMPv3 discovery probe (Nmap engineID sync)");
        static const unsigned char v3_reply_bytes[] = {
            0x30, 0x39,
            0x02, 0x01, 0x03,
            0x30, 0x0f, 0x02, 0x02, 0x4a, 0x69, 0x02, 0x03, 0x00, 0xff, 0xe3, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03,
            0x04, 0x1b,
            0x30, 0x19,
            0x04, 0x0b, 0x80, 0x00, 0x07, 0xc7, 0x03, 0x74, 0x8e, 0xf8, 0x31, 0xdb, 0x80,
            0x02, 0x01, 0x01,
            0x02, 0x01, 0x02,
            0x04, 0x00, 0x04, 0x00, 0x04, 0x00,
            0x30, 0x06, 0x04, 0x00, 0x04, 0x00, 0x30, 0x00
        };
        const std::string v3_reply(reinterpret_cast<const char*>(v3_reply_bytes), sizeof(v3_reply_bytes));
        unsigned long ent = 0;
        std::string engine_hex, format_data;
        check(snmp_v3_engine_info(v3_reply, ent, engine_hex, format_data) && ent == 1991 &&
              format_data == "74:8e:f8:31:db:80" && engine_hex.find("80:00:07:c7:03:74") != std::string::npos,
            "SNMPv3 engineID parses enterprise + MAC");
        const auto mdns = mdns_probe();
        check(mdns.find("_services") != std::string::npos && mdns.find("_dns-sd") != std::string::npos, "mDNS service enumeration query");
        check(ssdp_probe("192.0.2.10").find("M-SEARCH") != std::string::npos && ssdp_probe("192.0.2.10").find("192.0.2.10:1900") != std::string::npos, "SSDP M-SEARCH");
        check(ssdp_igd_probe("192.0.2.10").find("InternetGatewayDevice:1") != std::string::npos, "SSDP IGD M-SEARCH");
        Service udp; udp.port = 1900;
        std::vector<std::string> locations;
        const std::string ssdp = "HTTP/1.1 200 OK\r\nSERVER: Linux/5.15 UPnP/1.0 VectorTest/1.2\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nLOCATION: http://192.0.2.10:5000/rootDesc.xml\r\n\r\n";
        read_udp_reply("ssdp", ssdp, udp, f, &locations);
        check(udp.name == "SSDP" && udp.protocol == "udp" && has("UPnP server") && has("Linux"), "SSDP reply names the server");
        check(has("UPnP location") && has("Router / gateway") && locations.size() == 1 && locations[0].find("rootDesc.xml") != std::string::npos,
            "SSDP LOCATION and InternetGatewayDevice");
        f.clear();
        absorb_upnp_description(
            R"(<?xml version="1.0"?><root><device><deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
            <friendlyName>Vector Gateway</friendlyName><manufacturer>OpenWrt</manufacturer>
            <modelName>VectorRouter</modelName><modelNumber>1.2.3</modelNumber></device></root>)",
            "http://192.0.2.10:5000/rootDesc.xml", f);
        check(has("UPnP manufacturer") && has("UPnP model") && has("UPnP model number", "1.2.3") && has("Router / gateway"),
            "UPnP description fields become device hints");
        f.clear(); udp = {}; udp.port = 161;
        const std::string descr = "Linux Vector 5.15";
        std::string snmp_reply = std::string("\x04", 1) + static_cast<char>(descr.size()) + descr;
        read_udp_reply("snmp", snmp_reply, udp, f);
        check(udp.name == "SNMP" && has("SNMP sysDescr") && has("Linux"), "SNMP sysDescr is evidence");
        f.clear(); udp = {}; udp.port = 5353;
        read_udp_reply("mdns", std::string("_http._tcp.local"), udp, f);
        check(udp.name == "mDNS" && has("http"), "mDNS service type");
        check(udp_loopback_ok(), "UDP probe round trip");
        const auto type1 = ntlm_type1_token();
        check(type1.starts_with("TlRMTVNTUAAB") && type1.size() > 16, "NTLM Type1 negotiate token");
        f.clear(); h.url = "http://exchange.test/";
        h.headers = {{"www-authenticate", "NTLM"}};
        h.body.clear();
        check(ntlm_offered(h) && ntlm_needs_type1(h) && !ntlm_type2_present(h), "NTLM offer without Type2");
        identify_ntlm(h, f);
        check(has("NTLM authentication"), "NTLM offer recorded");
        f.clear();
        h.headers = {{"www-authenticate",
            "NTLM TlRMTVNTUAACAAAAAAAAAAAAAAAVgogCAAAAAAAAAAAAAAAAAAAAAI4AjgA4AAAACgBjRQAAAA8BAAwARQBYAEMASAAwADEAAgAOAEMATwBOAFQATwBTAE8AAwAoAEUAWABDAEgAMAAxAC4AYwBvAG4AdABvAHMAbwAuAGwAbwBjAGEAbAAEABoAYwBvAG4AdABvAHMAbwAuAGwAbwBjAGEAbAAFABoAYwBvAG4AdABvAHMAbwAuAGwAbwBjAGEAbAAAAAAA"}};
        check(ntlm_type2_present(h) && !ntlm_needs_type1(h), "NTLM Type2 challenge present");
        identify_ntlm(h, f);
        check(has("NTLM NetBIOS computer", "EXCH01"), "NTLM NetBIOS computer name");
        check(has("NTLM NetBIOS domain", "CONTOSO"), "NTLM NetBIOS domain");
        check(has("NTLM DNS computer", "EXCH01.contoso.local"), "NTLM DNS computer");
        check(has("NTLM DNS domain", "contoso.local"), "NTLM DNS domain");
        check(has("NTLM DNS forest", "contoso.local"), "NTLM DNS forest");
        check(has("Windows", "Windows 10 / Server 2019"), "NTLM Windows build hint");
        f.clear(); h.url = "http://winrm.test:5985/wsman";
        h.headers = {{"server", "Microsoft-HTTPAPI/2.0"}, {"www-authenticate", "Negotiate"}};
        identify_ntlm(h, f);
        check(has("WinRM"), "WinRM on 5985 with HTTPAPI");
        const auto ports = default_ports();
        check(std::find(ports.begin(), ports.end(), 5985) != ports.end() && std::find(ports.begin(), ports.end(), 5986) != ports.end(), "WinRM ports in default coverage");
        f.clear(); s = Service{}; s.port=25; s.banner="220\r\n2.0.0 ESMTP ready";
        identify_banner(s,f);
        check(s.name!="MySQL","a text banner with a newline is not a MySQL greeting");
        s.banner="SSH-2.0-OpenSSH_9.2p1 Debian-2+deb12u3\r\n"; s.name="Unknown";
        identify_banner(s,f);
        check(has("Debian Linux","12"),"debian release from the openssh package suffix");
        check(has("Debian OpenSSH package","2+deb12u3"),"debian openssh package revision");
        f.clear(); s.banner="SSH-2.0-OpenSSH_for_Windows_9.5p1\r\n"; s.name="Unknown";
        identify_banner(s,f);
        check(has("Windows") && has("OpenSSH for Windows","9.5p1"),"windows openssh build");
        f.clear(); h.headers={{"server","Apache/2.4.58 (Win64) OpenSSL/3.0.14"}}; h.body.clear();
        identify_http(h,f);
        check(has("Apache HTTP Server","2.4.58") && has("OpenSSL","3.0.14") && has("Windows"),"apache parenthetical os and openssl library");
        f.clear(); h.headers={{"x-powered-by","Phusion Passenger 6.0.19"},{"x-aspnetmvc-version","5.2"}};
        h.body=R"(<script src="https://cdn.jsdelivr.net/npm/lodash@4.17.21/lodash.min.js"></script><script src="https://cdnjs.cloudflare.com/ajax/libs/axios/1.6.7/axios.min.js"></script>)";
        identify_http(h,f);
        check(has("lodash","4.17.21"),"jsdelivr library version");
        check(has("axios","1.6.7"),"cdnjs library version");
        check(has("Phusion Passenger","6.0.19"),"passenger runtime");
        check(has("ASP.NET MVC","5.2"),"asp.net mvc version header");
        f.clear(); h.headers.clear();
        h.body="The notes mention OpenSSL/3.0.14, lodash.min.js, Debian-2+deb12u3 and Alpine Linux v3.19.";
        identify_http(h,f);
        check(f.empty(),"library and os tokens in prose are not fingerprints");
        h.headers={{"server","nginx"},{"content-type","text/html"},{"set-cookie","session=[redacted]"}};
        h.body.clear(); f.clear(); identify_http(h,f);
        check(has("HTTP header order","07a1199f48c3"),"header order fingerprint");
        check(has("HTTP cookies"),"cookie name kept, value dropped");
        check(favicon_hash("hello")==1155597304,"favicon murmur3");
        const unsigned char hello[] = {
            0x16,0x03,0x03,0x00,0x38, 0x02,0x00,0x00,0x34, 0x03,0x03,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0x00, 0x13,0x01, 0x00, 0x00,0x0c,
            0x00,0x2b,0x00,0x02,0x03,0x04, 0x00,0x33,0x00,0x02,0x00,0x1d};
        check(ja4s_from_tls(std::string(reinterpret_cast<const char*>(hello),sizeof(hello)))=="t130200_1301_a56c5b993250","JA4S ServerHello");
        const auto client = tls_client_hello();
        const auto record_len = (static_cast<unsigned char>(client[3]) << 8) | static_cast<unsigned char>(client[4]);
        check(client.size() > 80 && static_cast<unsigned char>(client[0]) == 0x16 && client.size() == 5 + record_len, "TLS ClientHello record");
        const unsigned char cert[] = {
            0x30,0x81,0x8c,0x30,0x77,0xa0,0x03,0x02,0x01,0x02,0x02,0x01,0x01,0x30,0x0d,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0b,0x05,0x00,0x30,0x0c,0x31,0x0a,0x30,0x08,0x06,0x03,0x55,0x04,0x03,0x0c,0x01,0x61,0x30,0x1e,0x17,0x0d,0x32,0x35,0x30,0x31,0x30,0x31,0x30,0x30,0x30,0x30,0x30,0x30,0x5a,0x17,0x0d,0x33,0x35,0x30,0x31,0x30,0x31,0x30,0x30,0x30,0x30,0x30,0x30,0x5a,0x30,0x0c,0x31,0x0a,0x30,0x08,0x06,0x03,0x55,0x04,0x03,0x0c,0x01,0x62,0x30,0x13,0x30,0x0d,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01,0x05,0x00,0x03,0x02,0x00,0x00,0xa3,0x0d,0x30,0x0b,0x30,0x09,0x06,0x03,0x55,0x1d,0x0e,0x04,0x02,0x04,0x00,0x30,0x0d,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0b,0x05,0x00,0x03,0x02,0x00,0x00};
        check(ja4x_from_der(std::string(reinterpret_cast<const char*>(cert),sizeof(cert)))=="7022c563de38_7022c563de38_30d204a01551","JA4X issuer subject extension");
        s.banner.assign(reinterpret_cast<const char*>(hello),sizeof(hello)); s.port=443; s.name="Unknown"; s.active_probe=true; f.clear();
        identify_banner(s,f);
        check(s.name=="TLS" && has("JA4S","t130200_1301_a56c5b993250"),"TLS banner becomes JA4S");
        f.clear();
        h.url="https://app.example:443/";
        h.headers={{"server","nginx/1.26.2"}};
        h.body=R"(<script type="module" src="/assets/index-Dk3f.js"></script><link rel="modulepreload" href="/assets/vue-vendor.js"><script type="importmap">{"imports":{"jquery":"https://cdn.jsdelivr.net/npm/jquery@3.7.1/dist/jquery.min.js"}}</script>)";
        identify_http(h,f);
        check(has("nginx","1.26.2"),"https page server header names nginx");
        check(has("jQuery","3.7.1"),"import map names the jquery build");
        auto scripts=page_script_urls(h);
        check(scripts.size()==2 && scripts[0].find("/assets/index-Dk3f.js")!=std::string::npos,"same-host hashed script is queued");
        check(scripts[1].find("vue-vendor.js")!=std::string::npos,"modulepreload script is queued");
        h.body=R"(<a href="/about">About</a><a href="https://cdn.example/lib.js">cdn</a><a href="/logo.png">img</a><area href="/contact?ref=1#top">)";
        auto pages=page_html_urls(h);
        check(pages.size()==2 && pages[0].find("/about")!=std::string::npos && pages[1].find("/contact?ref=1")!=std::string::npos,"same-host html links are queued; assets and other hosts are not");
        auto maps=sitemap_page_urls(h.url, "<urlset><url><loc>https://app.example/shop</loc></url><url><loc>https://app.example/logo.png</loc></url></urlset>");
        check(maps.size()==1 && maps[0].find("/shop")!=std::string::npos,"sitemap locs keep html pages only");
        auto bots=robots_sitemap_urls(h.url, "User-agent: *\nDisallow: /\nSitemap: https://app.example/sitemap.xml\n");
        check(bots.size()==1 && bots[0].find("/sitemap.xml")!=std::string::npos,"robots sitemap lines are collected");
        identify_javascript(scripts[0], "/*! jQuery v3.6.0 | jquery.org */\nSymbol.for('react.element');Symbol.for('react.fragment');React.version=\"18.3.1\"", f);
        check(has("jQuery","3.6.0") && has("React","18.3.1"),"library banners inside a fetched bundle");
        h.body="The article says nginx and jQuery v3.7.1 and react.element.";
        h.headers.clear(); f.clear();
        identify_http(h,f);
        check(f.empty(),"library banners in prose are not fingerprints");
        h.body = R"(<script src="/wp-content/plugins/woocommerce/assets/js/frontend/add-to-cart.min.js?ver=8.2.1"></script><link rel="stylesheet" href="/wp-content/plugins/wordpress-seo/css/dist/adminbar.css?ver=22.1">)";
        h.headers = {{"server","RomPager/4.07"}};
        f.clear();
        identify_http(h,f);
        check(has("WooCommerce","8.2.1") && has("Yoast SEO","22.1") && has("Allegro RomPager","4.07"),"wordpress plugins and embedded device server");
        h.body = "Reviewed WooCommerce and Hikvision in the article, path not present.";
        h.headers.clear(); f.clear();
        identify_http(h,f);
        check(!has("WooCommerce") && !has("Hikvision"),"plugin and camera names in prose are not fingerprints");
        f.clear(); s = Service{}; s.port = 22; s.banner = "SSH-2.0-ROSSSH\r\nRouterOS 7.12\r\n"; s.name = "Unknown";
        identify_banner(s,f);
        check(has("MikroTik RouterOS","7.12"),"routeros version from the banner");
        const char* rdap = R"({"country":"AU","name":"APNIC-LABS","status":["active"],"notices":[{"title":"Source","description":["Objects returned came from source","APNIC"]}],"remarks":[{"description":["Routed globally by AS13335/Cloudflare","resolver-abuse@cloudflare.com"],"title":"remarks"}],"entities":[{"roles":["abuse"],"vcardArray":["vcard",[["fn",{},"text","IRT-APNICRANDNET-AU"],["email",{},"text","helpdesk@apnic.net"]]]},{"roles":["registrant"],"vcardArray":["vcard",[["fn",{},"text","APNIC Research and Development"],["email",{},"text","helpdesk@apnic.net"]]]}],"handle":"1.1.1.0 - 1.1.1.255","type":"ASSIGNED PORTABLE","startAddress":"1.1.1.0","endAddress":"1.1.1.255","cidr0_cidrs":[{"v4prefix":"1.1.1.0","length":24}]})";
        auto reg = parse_rdap_json(rdap);
        check(reg.name=="APNIC-LABS" && reg.country=="AU" && reg.cidr=="1.1.1.0/24" && reg.source=="APNIC","rdap network identity");
        check(reg.org=="APNIC Research and Development" && reg.abuse_email=="helpdesk@apnic.net" && reg.asn.find("AS13335")!=std::string::npos,"rdap org abuse asn");
        check(std::any_of(reg.contacts.begin(),reg.contacts.end(),[](const RegistryContact& c){return c.role=="noted" && c.email=="resolver-abuse@cloudflare.com";}),"operator email from registry remarks");
        std::atomic_bool stay=false;
        auto priv = lookup_registry("192.168.1.20", 200, stay);
        check(priv.scope=="private" && priv.abuse_email.empty(),"private addresses are not sent to a registry");
        auto demo=demo_report(); auto j=nlohmann::json::parse(report_json(demo));
        check(j["status"]=="demo" && j["schema_version"]==1,"demo and schema labels exported");
        check(j["registry"]["name"]=="TEST-NET-1" && j["registry"]["abuse_email"]=="abuse@example.net","demo registry row exported");
        check(!j["web"][0].contains("body"),"response body omitted from export");
        std::atomic_bool cancel=true;
        auto cancelled=scan(parse_target("127.0.0.1"),{},cancel);
        check(cancelled.status=="cancelled" && cancelled.services.empty(),"cancel before DNS and probes");
        std::cout<<checks<<" core checks passed.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
