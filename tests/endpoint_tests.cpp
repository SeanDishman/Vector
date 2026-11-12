#include "vector/model.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int checks=0;
void check(bool value,const char* name){if(!value)throw std::runtime_error(name);++checks;}
int main(int argc,char** argv) {
#ifdef _WIN32
    WSADATA data{};WSAStartup(MAKEWORD(2,2),&data);
#endif
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try {
        std::atomic_bool cancel{false};
        if(argc==3&&std::string(argv[1])=="--lookup-geolocation") {
            ScanReport r;r.address=argv[2];r.geolocation=lookup_geolocation(r.address,3000,cancel);
            std::cout<<report_json(r)<<'\n';return r.geolocation.state=="available"?0:1;
        }
        for(const auto* address:{"108.241.209.26","8.8.8.8","2606:4700:4700::1111"})check(geolocation_eligible(address),"public IP eligible");
        for(const auto* address:{"", "localhost", "invalid", "127.0.0.1", "10.0.0.1", "172.31.255.255", "192.168.2.3", "169.254.1.1", "0.0.0.0",
             "100.64.0.1", "100.127.255.255", "224.0.0.1", "255.255.255.255", "192.0.2.1", "198.51.100.2", "203.0.113.1", "198.18.0.1",
             "::", "::1", "fe90::1", "fc00::1", "ff02::1", "2001:db8::1", "::ffff:127.0.0.1", "::ffff:10.0.0.1", "1.1.1.1/a", "1.1.1.999"})
            check(!geolocation_eligible(address),"nonpublic or invalid address excluded");
        const std::string sample=R"({"ip":"108.241.209.26","success":true,"city":"Fixture City","region":"Fixture Region","country":"Example","country_code":"EX","timezone":{"id":"Example/Zone"},"connection":{"asn":1234,"isp":"Fixture ISP"}})";
        auto geo=parse_geolocation_json(sample,"108.241.209.26");
        check(geo.state=="available"&&geo.city=="Fixture City"&&geo.region=="Fixture Region"&&geo.country=="Example","city region country retained");
        check(geo.asn=="AS1234"&&geo.isp=="Fixture ISP"&&geo.timezone=="Example/Zone","network and timezone metadata retained");
        check(geo.source=="https://ipwho.is/108.241.209.26"&&!geo.checked_at.empty(),"provider and date retained");
        check(parse_geolocation_json(sample,"8.8.8.8").state=="unavailable","mismatched IP rejected");
        check(parse_geolocation_json("[]","8.8.8.8").state=="unavailable","nonobject rejected");
        check(parse_geolocation_json("not json","8.8.8.8").state=="unavailable","invalid JSON rejected");
        check(parse_geolocation_json(R"({"success":false,"message":"Rate limit"})","8.8.8.8").error=="Rate limit","provider rejection retained");
        check(parse_geolocation_json(R"({"success":"true","ip":"8.8.8.8","city":"Bogus"})","8.8.8.8").state=="unavailable","wrong success type rejected");
        check(parse_geolocation_json(R"({"success":true,"ip":"8.8.8.8","city":42})","8.8.8.8").state=="unavailable","missing location rejected");
        check(parse_geolocation_json(std::string(32769,' '),"8.8.8.8").state=="unavailable","oversized provider response rejected");
        check(lookup_geolocation("127.0.0.1",100,cancel).state=="skipped","local address makes no geolocation request");
        cancel=true;check(lookup_geolocation("8.8.8.8",100,cancel).state=="cancelled","cancelled lookup sends no request");
        check(inspect_icmp("127.0.0.1",cancel).state=="cancelled","pre-cancelled ICMP sends no echo");cancel=false;
        ScanReport r;check(summarize_reachability(r).state=="not_checked","empty report not checked");
        r.icmp.state="no_reply";check(summarize_reachability(r).state=="unknown","ICMP silence is not offline");
        r.icmp.state="unreachable";check(summarize_reachability(r).state=="unknown","gateway unreachable is not host response");
        r.services.push_back({80,"no response","tcp"});check(summarize_reachability(r).state=="unknown","TCP timeout is not a response");
        r.services.push_back({443,"closed","tcp"});check(summarize_reachability(r).state=="refused","TCP refusal retained as network response");
        r.services.push_back({22,"open","tcp"});check(summarize_reachability(r).state=="responding","TCP connection proves responding endpoint path");
        auto reach=summarize_reachability(r);check(reach.tcp_open==1&&reach.tcp_refused==1&&reach.tcp_no_response==1,"TCP outcomes remain separate");
        r.services.clear();r.services.push_back({1900,"open","udp"});check(summarize_reachability(r).state=="responding","UDP reply counts as response");
        r.services.clear();r.icmp.state="reply";check(summarize_reachability(r).state=="responding","echo reply counts as response");
        r.icmp=inspect_icmp("127.0.0.1",cancel);
#ifdef _WIN32
        check(r.icmp.state=="reply"&&r.icmp.reply_address=="127.0.0.1","real IPv4 loopback echo succeeds");
        const auto v6=inspect_icmp("::1",cancel);check(v6.state=="reply"&&v6.reply_address=="::1","real IPv6 loopback echo succeeds");
        check(inspect_icmp("not-an-ip",cancel).state=="error","invalid echo target rejected");
#else
        check(r.icmp.state=="unavailable","non-Windows ICMP support explicit");
#endif
        r.geolocation=geo;const auto json=nlohmann::json::parse(report_json(r));
        check(json["geolocation"]["city"]=="Fixture City"&&json["geolocation"]["source"]==geo.source,"geolocation exported");
        check(json["reachability"]["icmp"]["state"]==r.icmp.state,"ICMP exported");
        std::cout<<checks<<" endpoint checks passed.\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
    curl_global_cleanup();
#ifdef _WIN32
    WSACleanup();
#endif
}
