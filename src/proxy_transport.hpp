#pragma once
#include "vector/proxy.hpp"
#include <curl/curl.h>
#include <atomic>
#include <chrono>
#include <memory>

namespace vectorapp {
void configure_proxy(CURL* handle,const ProxyConfig& proxy);
class ProxyTunnel {
public:
    using Clock=std::chrono::steady_clock;
    ProxyTunnel()=default;
    ~ProxyTunnel();
    ProxyTunnel(const ProxyTunnel&)=delete;
    ProxyTunnel& operator=(const ProxyTunnel&)=delete;
    bool connect(const ProxyConfig& proxy,const std::string& host,uint16_t port,Clock::time_point deadline,std::atomic_bool& cancel);
    bool open(const std::string& host,uint16_t port,Clock::time_point deadline,std::atomic_bool& cancel);
    CURLcode send(const char* data,size_t bytes,size_t& sent);
    CURLcode receive(char* data,size_t bytes,size_t& received);
    bool write(const std::string& data,Clock::time_point deadline,std::atomic_bool& cancel);
    std::string read(Clock::time_point deadline,std::atomic_bool& cancel);
    const std::string& error()const{return error_;}
private:
    bool start(const ProxyConfig* proxy,const std::string& host,uint16_t port,Clock::time_point deadline,std::atomic_bool& cancel);
    CURL* easy_=nullptr;
    CURLM* multi_=nullptr;
    std::string error_;
};
ProxyConfig resolve_proxy(ProxyConfig proxy,const std::string& host,uint16_t port,std::atomic_bool& cancel,std::string* detail=nullptr);

class BrowserProxy {
public:
    BrowserProxy(const ProxyConfig& proxy,std::atomic_bool& cancel,ProxyTunnel::Clock::time_point deadline);
    ~BrowserProxy();
    std::string endpoint()const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
