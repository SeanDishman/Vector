

#include "../src/asset_scope.hpp"
#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace vectorapp;
using namespace vectorapp::asset_scope;
int checks=0;
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
void close_socket(curl_socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
sockaddr_in loopback(uint16_t port) {
    sockaddr_in in{};in.sin_family=AF_INET;in.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&in.sin_addr);return in;
}
bool pending(curl_socket_t listener) {
    fd_set set;FD_ZERO(&set);FD_SET(listener,&set);timeval wait{0,200000};
    return select(static_cast<int>(listener)+1,&set,nullptr,nullptr,&wait)>0;
}
int main() {
#ifdef _WIN32
    WSADATA data{};WSAStartup(MAKEWORD(2,2),&data);
#endif
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try {
        for(const char* a:{"8.8.8.8","1.1.1.1","2606:4700:4700::1111"})check(address_in_scope(AddressScope::Public,a),"public address allowed for a public page");
        for(const char* a:{"127.0.0.1","127.8.9.1","::1","0.0.0.0","::","10.0.0.1","172.16.0.1","192.168.1.1","169.254.169.254","100.64.0.1",
                           "fc00::1","fe80::1","::ffff:127.0.0.1","::ffff:10.0.0.1","192.0.2.1","224.0.0.1","fe80::1%12",""})
            check(!address_in_scope(AddressScope::Public,a),"local, private and reserved addresses refused for a public page");
        for(const char* a:{"127.0.0.1","127.0.0.2","::1","::ffff:127.0.0.1"})check(address_in_scope(AddressScope::Loopback,a),"loopback allowed for a loopback page");
        for(const char* a:{"8.8.8.8","10.0.0.1","192.168.1.1","0.0.0.0","::","12.7.0.1",""})check(!address_in_scope(AddressScope::Loopback,a),"non-loopback refused for a loopback page");

        const auto t=[](const char* url){return parse_target(url);};
        const auto site=t("https://site.example.org/");
        const char* open="93.184.216.34";
        check(other_host_scope(t("https://cdn.vendor.net/a.js"),site,open,false)==AddressScope::Public,"public page reads a named asset host under the public check");
        check(other_host_scope(t("https://8.8.8.8/a.js"),site,open,false)==AddressScope::Public,"public page reads a public literal");
        check(other_host_scope(t("http://localhost/a.js"),site,open,false)==AddressScope::Public,"a name is judged at connection time, not by spelling");
        for(const char* u:{"http://127.0.0.1/a.js","http://10.0.0.5/a.js","http://192.168.0.1/a.js","http://[::1]/a.js","http://169.254.169.254/latest","http://0.0.0.0/a.js","http://2130706433/a.js"})
            check(other_host_scope(t(u),site,open,false)==AddressScope::Any,"public page never reads a local literal");
        const auto local=t("http://127.0.0.1:8080/");
        check(other_host_scope(t("http://127.0.0.2:9000/a.js"),local,"127.0.0.1",false)==AddressScope::Loopback,"loopback page reads a loopback literal");
        check(other_host_scope(t("http://localhost:9000/a.js"),local,"127.0.0.1",false)==AddressScope::Loopback,"loopback page reads localhost under the loopback check");
        check(other_host_scope(t("http://[::1]:9000/a.js"),local,"::1",false)==AddressScope::Loopback,"IPv6 loopback page and asset");
        for(const char* u:{"https://cdn.vendor.net/a.js","https://8.8.8.8/a.js","http://192.0.2.10/a.js","http://localhost.vendor.net/a.js","http://198.51.100.7/a.js"})
            check(other_host_scope(t(u),local,"127.0.0.1",false)==AddressScope::Any,"loopback page causes no outside request");
        const auto lan=t("http://192.168.1.10/");
        for(const char* u:{"https://cdn.vendor.net/a.js","http://192.168.1.20/a.js","http://127.0.0.1/a.js","http://localhost/a.js"})
            check(other_host_scope(t(u),lan,"192.168.1.10",false)==AddressScope::Any,"private page reads no other host");
        check(other_host_scope(t("https://cdn.vendor.net/a.js"),site,"",false)==AddressScope::Any,"unknown page address reads no other host");
        check(other_host_scope(t("https://cdn.vendor.net/a.js"),site,"",true)==AddressScope::Public,"proxied public names are read");
        for(const char* u:{"http://localhost/a.js","http://intranet/a.js","http://printer.local/a.js","http://app.internal/a.js","http://10.0.0.5/a.js",
                           "http://127.0.0.1/a.js","https://cdn.example.invalid/a.js","http://host.test/a.js","http://router.lan/a.js"})
            check(other_host_scope(t(u),site,"",true)==AddressScope::Any,"proxied special-use names and local literals are not read");
        check(other_host_scope(t("https://cdn.vendor.net/a.js"),local,"",true)==AddressScope::Any,"proxied local endpoint reads no other host");
        check(other_host_scope(t("https://cdn.vendor.net/a.js"),t("http://app.internal/"),"",true)==AddressScope::Any,"proxied special-use endpoint reads no other host");

        const auto probe=[](AddressScope scope,const char* ip) {
            ScopeGuard guard{scope};curl_sockaddr address{};address.family=AF_INET;address.socktype=SOCK_STREAM;address.protocol=IPPROTO_TCP;
            sockaddr_in in{};in.sin_family=AF_INET;in.sin_port=htons(80);inet_pton(AF_INET,ip,&in.sin_addr);
            address.addrlen=sizeof(in);std::memcpy(&address.addr,&in,sizeof(in));
            const auto s=open_scoped_socket(&guard,CURLSOCKTYPE_IPCXN,&address);
            const bool opened=s!=CURL_SOCKET_BAD;if(opened)close_socket(s);
            return std::pair{opened,guard.blocked};
        };
        check(probe(AddressScope::Public,"127.0.0.1")==std::pair{false,true},"public scope refuses loopback");
        check(probe(AddressScope::Public,"10.0.0.1")==std::pair{false,true},"public scope refuses a private address");
        check(probe(AddressScope::Public,"169.254.169.254")==std::pair{false,true},"public scope refuses link-local metadata");
        check(probe(AddressScope::Public,"8.8.8.8")==std::pair{true,false},"public scope accepts a public address");
        check(probe(AddressScope::Loopback,"127.0.0.1")==std::pair{true,false},"loopback scope accepts loopback");
        check(probe(AddressScope::Loopback,"8.8.8.8")==std::pair{false,true},"loopback scope refuses a public address");

        const auto listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        auto bound=loopback(0);
        check(listener!=CURL_SOCKET_BAD&&bind(listener,reinterpret_cast<sockaddr*>(&bound),sizeof(bound))==0&&listen(listener,4)==0,"loopback listener");
        socklen_t length=sizeof(bound);getsockname(listener,reinterpret_cast<sockaddr*>(&bound),&length);
        const auto url="http://127.0.0.1:"+std::to_string(ntohs(bound.sin_port))+"/";
        const auto connect=[&](AddressScope scope) {
            ScopeGuard guard{scope};auto* curl=curl_easy_init();
            curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
            curl_easy_setopt(curl,CURLOPT_PROXY,"");curl_easy_setopt(curl,CURLOPT_CONNECT_ONLY,1L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,3000L);
            curl_easy_setopt(curl,CURLOPT_OPENSOCKETFUNCTION,open_scoped_socket);curl_easy_setopt(curl,CURLOPT_OPENSOCKETDATA,&guard);
            const auto result=curl_easy_perform(curl);curl_easy_cleanup(curl);
            return std::pair{result==CURLE_OK,guard.blocked};
        };
        check(connect(AddressScope::Public)==std::pair{false,true},"public-scope transfer to loopback is refused");
        check(!pending(listener),"refused transfer never reached the listener");
        check(connect(AddressScope::Loopback)==std::pair{true,false},"loopback-scope transfer connects");
        check(pending(listener),"allowed transfer reached the listener");
        close_socket(listener);
        std::cout<<checks<<" asset scope checks passed\n";
    } catch(const std::exception& error) { std::cerr<<"asset scope test failed: "<<error.what()<<'\n';return 1; }
    return 0;
}
