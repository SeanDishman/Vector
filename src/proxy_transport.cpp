#include "proxy_transport.hpp"
#include "vector/fingerprint.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <thread>
#include <vector>

namespace vectorapp {
void configure_proxy(CURL* h,const ProxyConfig& p) {
    if(!p.enabled){curl_easy_setopt(h,CURLOPT_PROXY,"");return;}
    const auto endpoint=std::string(p.scheme=="socks5"?"socks5h://":p.scheme=="https"?"https://":"http://")+
        (p.host.find(':')!=std::string::npos?"["+p.host+"]":p.host)+":"+std::to_string(p.port);
    curl_easy_setopt(h,CURLOPT_PROXY,endpoint.c_str());
    curl_easy_setopt(h,CURLOPT_PROXYTYPE,p.scheme=="socks5"?CURLPROXY_SOCKS5_HOSTNAME:p.scheme=="https"?CURLPROXY_HTTPS:CURLPROXY_HTTP);
    curl_easy_setopt(h,CURLOPT_NOPROXY,"");
    curl_easy_setopt(h,CURLOPT_HTTPPROXYTUNNEL,1L);
    curl_easy_setopt(h,CURLOPT_SUPPRESS_CONNECT_HEADERS,1L);
    curl_easy_setopt(h,CURLOPT_PROXY_SSL_VERIFYPEER,1L);
    curl_easy_setopt(h,CURLOPT_PROXY_SSL_VERIFYHOST,2L);
    curl_easy_setopt(h,CURLOPT_PROXYUSERNAME,p.username.c_str());
    curl_easy_setopt(h,CURLOPT_PROXYPASSWORD,p.password.c_str());
    curl_easy_setopt(h,CURLOPT_PROXYAUTH,CURLAUTH_BASIC|CURLAUTH_DIGEST);
}
ProxyTunnel::~ProxyTunnel(){if(multi_&&easy_)curl_multi_remove_handle(multi_,easy_);if(easy_)curl_easy_cleanup(easy_);if(multi_)curl_multi_cleanup(multi_);}
bool ProxyTunnel::connect(const ProxyConfig& p,const std::string& host,uint16_t port,Clock::time_point deadline,std::atomic_bool& cancel) {
    if(!p.enabled||cancel){error_="Proxy connection cancelled or disabled.";return false;}
    return start(&p,host,port,deadline,cancel);
}
bool ProxyTunnel::open(const std::string& host,uint16_t port,Clock::time_point deadline,std::atomic_bool& cancel) {
    if(cancel){error_="Proxy connection cancelled.";return false;}
    return start(nullptr,host,port,deadline,cancel);
}
bool ProxyTunnel::start(const ProxyConfig* p,const std::string& host,uint16_t port,Clock::time_point deadline,std::atomic_bool& cancel) {
    easy_=curl_easy_init();multi_=curl_multi_init();if(!easy_||!multi_){error_="Could not initialize proxy connection.";return false;}
    const auto url="http://"+(host.find(':')!=std::string::npos?"["+host+"]":host)+":"+std::to_string(port)+"/";
    const auto left=std::max<int64_t>(1,std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count());
    curl_easy_setopt(easy_,CURLOPT_URL,url.c_str());
    if(p)configure_proxy(easy_,*p);else {curl_easy_setopt(easy_,CURLOPT_PROXY,"");curl_easy_setopt(easy_,CURLOPT_NOPROXY,"*");}
    curl_easy_setopt(easy_,CURLOPT_CONNECT_ONLY,1L);curl_easy_setopt(easy_,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(easy_,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(left));curl_easy_setopt(easy_,CURLOPT_TIMEOUT_MS,static_cast<long>(left));
    if(curl_multi_add_handle(multi_,easy_)!=CURLM_OK){error_="Could not schedule proxy connection.";return false;}
    while(!cancel&&Clock::now()<deadline) {
        int running=0;if(curl_multi_perform(multi_,&running)!=CURLM_OK){error_="Proxy connection failed.";return false;}
        int queued=0;while(auto* message=curl_multi_info_read(multi_,&queued))if(message->msg==CURLMSG_DONE) {
            if(message->data.result==CURLE_OK)return true;
            error_="Proxy connection: "+std::string(curl_easy_strerror(message->data.result));return false;
        }
        int count=0;curl_multi_poll(multi_,nullptr,0,25,&count);
    }
    error_=cancel?"Proxy connection cancelled.":"Proxy connection timed out.";return false;
}
CURLcode ProxyTunnel::send(const char* data,size_t bytes,size_t& sent){return curl_easy_send(easy_,data,bytes,&sent);}
CURLcode ProxyTunnel::receive(char* data,size_t bytes,size_t& received){return curl_easy_recv(easy_,data,bytes,&received);}
bool ProxyTunnel::write(const std::string& data,Clock::time_point deadline,std::atomic_bool& cancel) {
    size_t offset=0;while(!cancel&&Clock::now()<deadline&&offset<data.size()){
        size_t sent=0;const auto code=send(data.data()+offset,data.size()-offset,sent);if(code!=CURLE_OK&&code!=CURLE_AGAIN)return false;offset+=sent;if(!sent)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }return offset==data.size();
}
std::string ProxyTunnel::read(Clock::time_point deadline,std::atomic_bool& cancel) {
    std::array<char,4096> bytes{};while(!cancel&&Clock::now()<deadline){size_t n=0;const auto code=receive(bytes.data(),bytes.size(),n);if(n)return {bytes.data(),n};if(code!=CURLE_AGAIN)return {};std::this_thread::sleep_for(std::chrono::milliseconds(5));}return {};
}
ProxyConfig resolve_proxy(ProxyConfig proxy,const std::string& host,uint16_t port,std::atomic_bool& cancel,std::string* detail) {
    if(!proxy.enabled||proxy.scheme!="auto"||cancel)return proxy;
    using Clock=ProxyTunnel::Clock;
    using Tick=std::chrono::milliseconds::rep;
    struct Probe{std::string bytes,reply;bool reached=false;Tick answered=0;std::atomic<Tick> sent{-1};std::atomic_bool done{false};};
    const auto address=proxy_endpoint(proxy);
    const auto authority=(host.find(':')!=std::string::npos?"["+host+"]":host)+":"+std::to_string(port);
    std::array<Probe,4> probes;
    probes[0].bytes="CONNECT "+authority+" HTTP/1.1\r\nHost: "+authority+"\r\n\r\n";
    probes[1].bytes="GET / HTTP/1.0\r\n\r\n";
    probes[2].bytes=std::string("\x05\x02\x00\x02",4);
    probes[3].bytes=tls_client_hello();
    const auto kind=[](const std::string& reply)->std::string {
        const auto byte=[&](size_t i){return static_cast<unsigned char>(reply[i]);};
        if(reply.size()>=2&&byte(0)==5&&(byte(1)==0||byte(1)==2||byte(1)==255))return "socks5";
        if(reply.size()>=3&&(byte(0)==0x16||byte(0)==0x15)&&byte(1)==3&&byte(2)<=4)return "https";
        if(reply.starts_with("HTTP/1."))return "http";
        return {};
    };
    std::string found,how;
    {
        std::atomic_bool stop{false};
        std::vector<std::thread> workers;
        struct Join {
            std::vector<std::thread>& threads;std::atomic_bool& halt;
            ~Join(){halt=true;for(auto& thread:threads)if(thread.joinable())thread.join();}
        } join{workers,stop};
        workers.reserve(probes.size());
        const auto began=Clock::now();
        const auto elapsed=[began]{return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-began).count();};
        for(auto& probe:probes)workers.emplace_back([&probe,&stop,elapsed,name=proxy.host,number=proxy.port] {
            try {
                ProxyTunnel wire;
                if(wire.open(name,number,Clock::now()+std::chrono::milliseconds(4000),stop)) {
                    probe.reached=true;
                    const auto until=Clock::now()+std::chrono::milliseconds(6000);
                    if(wire.write(probe.bytes,until,stop)) {
                        probe.sent=elapsed();
                        probe.reply=wire.read(until,stop);
                        probe.answered=elapsed();
                    }
                }
            } catch(...) {}
            probe.done=true;
        });
        while(!cancel) {
            const Tick now=elapsed();
            bool all=true,refusal=true;Tick quickest=1500;std::string socks,tls,http;
            for(size_t i=0;i<probes.size();++i) {
                if(!probes[i].done){all=false;continue;}
                const auto& reply=probes[i].reply;const auto type=kind(reply);
                if(type=="socks5"&&socks.empty())socks=i==2?"answered a SOCKS5 greeting":"answered with a SOCKS5 reply";
                if(type=="https"&&tls.empty())tls=i==3?"answered a TLS handshake":"answered plain text with a TLS alert";
                if(type=="http") {
                    const auto status=reply.size()>=12?reply.substr(9,3):std::string{};
                    if(http.empty())http=status=="407"?"asked for proxy credentials (HTTP 407)":status=="200"&&i==0?"opened a tunnel (HTTP 200)":
                        std::string(i==0?"answered a CONNECT request":i==1?"answered a plain HTTP request":"answered other traffic")+" with HTTP "+status;
                    refusal=refusal&&status=="400";
                    quickest=std::min(quickest,probes[i].answered-probes[i].sent.load());
                }
            }
            if(!socks.empty()){found="socks5";how=socks;}
            else if(!tls.empty()){found="https";how=tls;}
            else if(!http.empty()) {
                const Tick patience=refusal?Tick{1500}:std::clamp(quickest,Tick{250},Tick{1500});
                const auto heard=[&](const Probe& probe){const Tick at=probe.sent.load();return probe.done||(at>=0&&now-at>=patience);};
                if(heard(probes[2])&&heard(probes[3])){found="http";how=http;}
            }
            if(!found.empty()||all)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    if(cancel)return proxy;
    if(std::none_of(probes.begin(),probes.end(),[](const Probe& probe){return probe.reached;}))
        throw std::runtime_error("Could not connect to the proxy at "+address+". Check the address and port.");
    if(found.empty())throw std::runtime_error("The server at "+address+" did not answer as an HTTP, HTTPS or SOCKS5 proxy. Check the address and port.");
    proxy.scheme=found;
    validate_proxy(proxy);
    if(detail)*detail=std::string(found=="socks5"?"SOCKS5":found=="https"?"HTTPS":"HTTP")+" proxy: "+address+" "+how;
    return proxy;
}
}
