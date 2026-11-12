#include "vector/model.hpp"
#include "proxy_transport.hpp"
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace vectorapp;
int main(int argc,char** argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try {
        if(argc==4&&std::string(argv[1])=="--save") {save_proxy_settings(parse_proxy(argv[3]),std::filesystem::path(argv[2]));return 0;}
        if(argc==4&&std::string(argv[1])=="--trusted-proxy") {
            auto proxy=parse_proxy(argv[2]);if(proxy.host!="127.0.0.1"||proxy.scheme!="https")return 2;
            std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup);std::string body;
            configure_proxy(handle.get(),proxy);curl_easy_setopt(handle.get(),CURLOPT_PROXY_CAINFO,argv[3]);
            curl_easy_setopt(handle.get(),CURLOPT_URL,"http://tls-fixture.invalid:80/entry");curl_easy_setopt(handle.get(),CURLOPT_TIMEOUT_MS,3000L);
            const auto receive=+[](char* bytes,size_t size,size_t count,void* user)->size_t {const auto length=size*count;static_cast<std::string*>(user)->append(bytes,length);return length;};
            curl_easy_setopt(handle.get(),CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(handle.get(),CURLOPT_WRITEDATA,&body);
            const auto code=curl_easy_perform(handle.get());if(code!=CURLE_OK)throw std::runtime_error(curl_easy_strerror(code));
            if(body.find("WordPress 6.8")==std::string::npos)throw std::runtime_error("TLS proxy response missing");
            std::cout<<"Trusted HTTPS proxy authentication passed";return 0;
        }
        if(argc==3&&(std::string(argv[1])=="--cancel"||std::string(argv[1])=="--cancel-browser")) {
            const bool browser=std::string(argv[1])=="--cancel-browser";
            ScanOptions options;options.proxy=parse_proxy(argv[2]);if(options.proxy.host!="127.0.0.1")return 2;
            options.timeout_ms=10000;options.browser_runtime=browser;std::atomic_bool cancel{false};
            std::thread trigger([&]{std::this_thread::sleep_for(std::chrono::milliseconds(browser?1800:150));cancel=true;});
            const auto start=std::chrono::steady_clock::now();const auto r=scan(parse_target("http://fixture.invalid:80/entry"),options,cancel);trigger.join();
            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(r.status!="cancelled"||elapsed>(browser?8000:1200)||(browser&&r.browser.pages.empty()))throw std::runtime_error("Proxy cancellation did not finish within its bound");
            std::cout<<"{\"name\":\""<<(browser?"cancelled-browser-relay":"cancelled-tunnel")<<"\",\"milliseconds\":"<<elapsed<<"}";return 0;
        }
        if(argc==5&&std::string(argv[1])=="--detect") {
            auto proxy=parse_proxy(argv[2]);if(proxy.host!="127.0.0.1")return 2;
            std::atomic_bool cancel{false};std::string detail;
            const auto start=std::chrono::steady_clock::now();
            try {
                proxy=resolve_proxy(proxy,argv[3],static_cast<uint16_t>(std::stoi(argv[4])),cancel,&detail);
            } catch(const std::exception& e) {
                const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                std::cout<<"{\"scheme\":\"\",\"error\":\""<<e.what()<<"\",\"milliseconds\":"<<elapsed<<"}";return 3;
            }
            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"{\"scheme\":\""<<proxy.scheme<<"\",\"endpoint\":\""<<proxy_endpoint(proxy)<<"\",\"detail\":\""<<detail<<"\",\"milliseconds\":"<<elapsed<<"}";return 0;
        }
        if(argc==3&&std::string(argv[1])=="--metadata") {
            auto proxy=parse_proxy(argv[2]);if(proxy.host!="127.0.0.1")return 2;
            std::atomic_bool cancel{false};ScanReport r;r.target=parse_target("sample.example.com");
            r.dns=inspect_dns(r.target,"",cancel,proxy);
            r.registry=lookup_registry("8.8.8.8",500,cancel,proxy);
            r.geolocation=lookup_geolocation("8.8.8.8",500,cancel,proxy);
            std::cout<<report_json(r);return 0;
        }
        size_t checks=0;auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
        {const auto p=parse_proxy("http://127.0.0.1:8080");check(p.enabled&&p.port==8080&&p.scheme=="http","explicit HTTP type");}
        {const auto p=parse_proxy("127.0.0.1:3128");check(p.enabled&&p.port==3128&&p.scheme=="auto","host:port has no type until it is detected");}
        check(proxy_endpoint(parse_proxy("proxy.example:3128"))=="proxy.example:3128","undetected endpoint shows no type");
        check(parse_proxy("proxy.example").port==8080&&parse_proxy("http://proxy.example").port==80,"default ports");
        check(parse_proxy("user:secret@proxy.example:3128").scheme=="auto"&&parse_proxy("user:secret@proxy.example:3128").username=="user","credentials without a type");
        {
            const auto listed=parse_proxy("gate.proxy.example:7000:session-user:pa:ss@word/1");
            check(listed.scheme=="auto"&&listed.host=="gate.proxy.example"&&listed.port==7000,"host:port:user:password address");
            check(listed.username=="session-user"&&listed.password=="pa:ss@word/1","host:port:user:password credentials");
            const auto typed=parse_proxy("socks5://192.0.2.7:1080:user:secret");
            check(typed.scheme=="socks5"&&typed.host=="192.0.2.7"&&typed.port==1080&&typed.username=="user"&&typed.password=="secret","typed host:port:user:password");
            const auto six=parse_proxy("2001:db8::1");
            check(six.host=="2001:db8::1"&&six.port==8080&&proxy_endpoint(six)=="[2001:db8::1]:8080","bare IPv6 proxy address");
            const auto bracket=parse_proxy("[2001:1234:5::1]:3128");
            check(bracket.host=="2001:1234:5::1"&&bracket.port==3128&&bracket.username.empty(),"bracketed IPv6 proxy address with a port");
            const auto url=parse_proxy("user:8080:x@proxy.example:3128");
            check(url.host=="proxy.example"&&url.port==3128&&url.username=="user"&&url.password=="8080:x","credentials before the at sign stay credentials");
        }
        {
            std::atomic_bool cancel{false};std::string detail="unchanged";
            check(resolve_proxy(parse_proxy("socks5://127.0.0.1:9"),"fixture.invalid",80,cancel,&detail).scheme=="socks5"&&detail=="unchanged","a stated type is used as given, with no probe");
            ProxyConfig off=parse_proxy("127.0.0.1:9");off.enabled=false;
            check(resolve_proxy(off,"fixture.invalid",80,cancel).scheme=="auto","a disabled proxy is never probed");
            std::atomic_bool stopped{true};
            check(resolve_proxy(parse_proxy("127.0.0.1:9"),"fixture.invalid",80,stopped).scheme=="auto","cancelled detection sends nothing and decides nothing");
            std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup);
            configure_proxy(handle.get(),parse_proxy("127.0.0.1:3128"));
            check(handle!=nullptr,"an undetected proxy still configures a handle");
        }
        auto p=parse_proxy("socks5h://user%40test:pa%3Ass%40word@[::1]:1088");
        check(p.scheme=="socks5"&&p.host=="::1"&&p.port==1088,"SOCKS IPv6");
        check(p.username=="user@test"&&p.password=="pa:ss@word","encoded credentials");
        check(proxy_endpoint(p)=="socks5://[::1]:1088","credential-free endpoint");
        check(parse_proxy("https://proxy.example").port==443,"HTTPS default port");
        auto all=parse_proxy("all://127.0.0.1:9050");
        check(all.scheme=="all"&&all.host=="127.0.0.1"&&all.port==9050,"ALL scheme");
        check(proxy_endpoint(all)=="all://127.0.0.1:9050","ALL endpoint label");
        check(parse_proxy("all://proxy.example").port==8080,"ALL default port");
        {
            ProxyConfig configured=all;configured.enabled=true;
            std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup);
            check(handle!=nullptr,"ALL configure handle");
            configure_proxy(handle.get(),configured);
            check(true,"ALL configure_proxy accepts scheme all");
        }
        for(const auto* value:{"","ftp://example:80","http://example:0","http://example:65536","http://example:80/path","http://example:80/?x=1","http://example:80/#x","http://u:bad%0Asecret@example:80","proxy.example:99999:user:secret","socks4://example:1080"}) {
            bool rejected=false;try{parse_proxy(value);}catch(const std::exception&){rejected=true;}check(rejected,"invalid proxy accepted");
        }
        auto invalid=p;invalid.username.clear();bool rejected=false;try{validate_proxy(invalid);}catch(...){rejected=true;}check(rejected,"password without username");
        ScanReport report;report.proxy={true,true,proxy_endpoint(p)};const auto json=report_json(report);
        check(json.find(p.password)==std::string::npos&&json.find(p.username)==std::string::npos,"credentials in report");
#ifdef _WIN32
        const auto path=std::filesystem::temp_directory_path()/("vector-proxy-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".dat");
        save_proxy_settings(p,path);const auto read=load_proxy_settings(path);
        check(read.host==p.host&&read.password==p.password&&read.username==p.username&&read.enabled,"protected persistence round-trip");
        std::ifstream file(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(file)),{});file.close();
        check(bytes.find(p.password)==std::string::npos&&bytes.find(p.username)==std::string::npos,"plaintext credential storage");
        bytes[bytes.size()/2]^=0x40;{std::ofstream corrupt(path,std::ios::binary|std::ios::trunc);corrupt.write(bytes.data(),bytes.size());}
        rejected=false;try{load_proxy_settings(path);}catch(...){rejected=true;}check(rejected,"corrupted credentials accepted");
        std::filesystem::remove(path);
#endif
        std::cout<<checks<<" proxy checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
