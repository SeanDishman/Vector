#include "proxy_transport.hpp"
#include "vector/model.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace vectorapp {
namespace {
struct Ip {
    int family = 0;
    std::array<unsigned char,16> bytes{};
};
Ip parse_ip(const std::string& value) {
    Ip ip;
    if(inet_pton(AF_INET,value.c_str(),ip.bytes.data())==1)ip.family=AF_INET;
    else if(inet_pton(AF_INET6,value.c_str(),ip.bytes.data())==1)ip.family=AF_INET6;
    return ip;
}
struct GeoBuffer { std::string body; std::atomic_bool* cancel; bool limited=false; };
size_t receive_geo(char* data,size_t size,size_t count,void* context) {
    auto& b=*static_cast<GeoBuffer*>(context);
    if(*b.cancel)return 0;
    const size_t bytes=size*count;
    if(bytes>32768-b.body.size()){b.limited=true;return 0;}
    b.body.append(data,bytes);return bytes;
}
int geo_progress(void* context,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {
    return *static_cast<std::atomic_bool*>(context)?1:0;
}
}
bool geolocation_eligible(const std::string& address) {
    auto ip=parse_ip(address);auto& b=ip.bytes;
    if(ip.family==AF_INET6&&std::all_of(b.begin(),b.begin()+10,[](auto c){return c==0;})&&b[10]==255&&b[11]==255) {
        std::copy(b.begin()+12,b.end(),b.begin());ip.family=AF_INET;
    }
    if(ip.family==AF_INET)return !(b[0]==0||b[0]==10||b[0]==127||b[0]>=224||
        (b[0]==100&&b[1]>=64&&b[1]<=127)||(b[0]==169&&b[1]==254)||(b[0]==172&&b[1]>=16&&b[1]<=31)||
        (b[0]==192&&(b[1]==168||(b[1]==0&&(b[2]==0||b[2]==2))))||
        (b[0]==198&&(b[1]==18||b[1]==19||(b[1]==51&&b[2]==100)))||
        (b[0]==203&&b[1]==0&&b[2]==113));
    if(ip.family==AF_INET6)return (b[0]&0xe0)==0x20&&
        !(b[0]==0x20&&b[1]==0x01&&((b[2]==0x0d&&b[3]==0xb8)||b[2]<2))&&
        !(b[0]==0x20&&b[1]==0x02);
    return false;
}
Geolocation parse_geolocation_json(const std::string& body,const std::string& address) {
    Geolocation g;g.address=address;g.source="https://ipwho.is/"+address;g.checked_at=utc_now();g.state="unavailable";
    if(body.size()>32768){g.error="Geolocation response exceeded 32 KiB.";return g;}
    const auto doc=nlohmann::json::parse(body,nullptr,false);
    if(!doc.is_object()){g.error="Geolocation provider returned invalid JSON.";return g;}
    const auto field=[](const nlohmann::json& object,const char* key) {
        const auto it=object.find(key);
        return it!=object.end()&&it->is_string()?printable(trim(it->get<std::string>()),160):std::string{};
    };
    if(!doc.contains("success")||!doc["success"].is_boolean()||!doc["success"].get<bool>()) {
        g.error=field(doc,"message");if(g.error.empty())g.error="Geolocation provider did not return a location.";return g;
    }
    const auto returned=parse_ip(field(doc,"ip")),requested=parse_ip(address);
    if(!requested.family||returned.family!=requested.family||returned.bytes!=requested.bytes) {
        g.error="Geolocation response did not match the inspected IP.";return g;
    }
    g.city=field(doc,"city");g.region=field(doc,"region");g.country=field(doc,"country");g.country_code=field(doc,"country_code");
    if(doc.contains("timezone")&&doc["timezone"].is_object())g.timezone=field(doc["timezone"],"id");
    if(doc.contains("connection")&&doc["connection"].is_object()) {
        const auto& c=doc["connection"];g.isp=field(c,"isp");
        if(c.contains("asn")&&c["asn"].is_number_unsigned()&&c["asn"].get<uint64_t>()<=UINT32_MAX)
            g.asn="AS"+std::to_string(c["asn"].get<uint64_t>());
    }
    if(g.country.empty()&&g.region.empty()&&g.city.empty()){g.error="Geolocation provider returned no location fields.";return g;}
    g.state="available";return g;
}
Geolocation lookup_geolocation(const std::string& address,int timeout_ms,std::atomic_bool& cancel,const ProxyConfig& proxy) {
    Geolocation g;g.address=address;g.checked_at=utc_now();
    if(cancel){g.state="cancelled";g.error="Geolocation cancelled.";return g;}
    if(!geolocation_eligible(address)){g.state="skipped";g.error="Local, reserved or invalid IP; no geolocation request sent.";return g;}
    g.source="https://ipwho.is/"+address;g.state="unavailable";
    auto* raw=curl_easy_init();if(!raw){g.error="Could not initialize geolocation lookup.";return g;}
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(raw,curl_easy_cleanup);
    GeoBuffer buffer{{},&cancel};
    curl_easy_setopt(raw,CURLOPT_URL,g.source.c_str());
    configure_proxy(raw,proxy);curl_easy_setopt(raw,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(raw,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(raw,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(raw,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(raw,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(raw,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(std::clamp(timeout_ms,100,3000)));
    curl_easy_setopt(raw,CURLOPT_TIMEOUT_MS,static_cast<long>(std::clamp(timeout_ms,100,3000)));
    curl_easy_setopt(raw,CURLOPT_USERAGENT,"Vector/0.1 (IP geolocation)");curl_easy_setopt(raw,CURLOPT_ACCEPT_ENCODING,"identity");
    curl_easy_setopt(raw,CURLOPT_WRITEFUNCTION,receive_geo);curl_easy_setopt(raw,CURLOPT_WRITEDATA,&buffer);
    curl_easy_setopt(raw,CURLOPT_XFERINFOFUNCTION,geo_progress);curl_easy_setopt(raw,CURLOPT_XFERINFODATA,&cancel);curl_easy_setopt(raw,CURLOPT_NOPROGRESS,0L);
    const auto result=curl_easy_perform(raw);long status=0;curl_easy_getinfo(raw,CURLINFO_RESPONSE_CODE,&status);
    if(cancel){g.state="cancelled";g.error="Geolocation cancelled.";return g;}
    if(buffer.limited){g.error="Geolocation response exceeded 32 KiB.";return g;}
    if(result!=CURLE_OK){g.error=std::string("Geolocation lookup failed: ")+curl_easy_strerror(result);return g;}
    if(status!=200){g.error=status==429?"Geolocation provider rate limit reached.":"Geolocation provider returned HTTP "+std::to_string(status)+".";return g;}
    return parse_geolocation_json(buffer.body,address);
}
Reachability summarize_reachability(const ScanReport& r) {
    Reachability reach;
    for(const auto& s:r.services) {
        if(s.protocol=="tcp") {
            if(s.state=="open")++reach.tcp_open;
            else if(s.state=="closed")++reach.tcp_refused;
            else if(s.state=="no response")++reach.tcp_no_response;
            else ++reach.tcp_other;
        } else if(s.protocol=="udp"&&s.state=="open")++reach.udp_replies;
    }
    if(r.icmp.state=="reply"||reach.tcp_open||reach.udp_replies) {
        reach.state="responding";reach.summary="Responding";
    } else if(reach.tcp_refused) {
        reach.state="refused";reach.summary="Network responded; TCP connections refused";
    } else if(r.icmp.state=="not_checked"&&r.services.empty()) {
        reach.state="not_checked";reach.summary="Not checked";
    } else {
        reach.state="unknown";reach.summary="Unknown - no endpoint reply";
    }
    return reach;
}
}
