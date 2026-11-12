#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#endif
#include "proxy_transport.hpp"
#include "vector/model.hpp"
#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace vectorapp {
namespace {
#ifdef _WIN32
using Socket=SOCKET;constexpr Socket invalid=INVALID_SOCKET;
void close_socket(Socket s){closesocket(s);}
void nonblocking(Socket s){u_long mode=1;ioctlsocket(s,FIONBIO,&mode);}
bool again(){const auto e=WSAGetLastError();return e==WSAEWOULDBLOCK||e==WSAEINTR;}
#else
using Socket=int;constexpr Socket invalid=-1;
void close_socket(Socket s){close(s);}
void nonblocking(Socket s){fcntl(s,F_SETFL,fcntl(s,F_GETFL,0)|O_NONBLOCK);}
bool again(){return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR;}
#endif
struct Client {Socket socket;~Client(){if(socket!=invalid)close_socket(socket);}};
}
struct BrowserProxy::Impl {
    ProxyConfig proxy;
    std::atomic_bool& cancel;
    ProxyTunnel::Clock::time_point deadline;
    std::atomic_bool stop{false};
    Socket listener=invalid;
    uint16_t port=0;
    std::thread acceptor;
    struct Worker {std::thread thread;std::shared_ptr<std::atomic_bool> done;};
    std::vector<Worker> workers;
    Impl(const ProxyConfig& p,std::atomic_bool& c,ProxyTunnel::Clock::time_point d):proxy(p),cancel(c),deadline(d) {
        workers.reserve(64);
        listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(listener==invalid)throw std::runtime_error("Could not start the browser proxy relay.");
        sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(listener,reinterpret_cast<sockaddr*>(&local),sizeof(local))||listen(listener,64)) {close_socket(listener);listener=invalid;throw std::runtime_error("Could not bind the browser proxy relay.");}
#ifdef _WIN32
        int size=sizeof(local);
#else
        socklen_t size=sizeof(local);
#endif
        if(getsockname(listener,reinterpret_cast<sockaddr*>(&local),&size)){close_socket(listener);listener=invalid;throw std::runtime_error("Could not read the browser proxy relay address.");}
        port=ntohs(local.sin_port);nonblocking(listener);
        acceptor=std::thread([this]{serve();});
    }
    ~Impl(){stop=true;if(acceptor.joinable())acceptor.join();if(listener!=invalid)close_socket(listener);}
    void serve() {
        while(!stop&&!cancel&&ProxyTunnel::Clock::now()<deadline) {
            for(auto it=workers.begin();it!=workers.end();)if(*it->done){it->thread.join();it=workers.erase(it);}else ++it;
            const auto client=accept(listener,nullptr,nullptr);
            if(client!=invalid) {
                if(workers.size()>=64){close_socket(client);continue;}
                nonblocking(client);auto done=std::make_shared<std::atomic_bool>(false);
                try {workers.push_back({std::thread([this,client,done]{try{handle(client);}catch(...){ }*done=true;}),done});}
                catch(...){close_socket(client);}
            }else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        stop=true;for(auto& worker:workers)if(worker.thread.joinable())worker.thread.join();
    }
    void handle(Socket socket) {
        Client client{socket};std::string request;std::array<char,16384> chunk{};
        const auto header_deadline=std::min(deadline,ProxyTunnel::Clock::now()+std::chrono::seconds(6));
        size_t end=std::string::npos;
        while(!stop&&ProxyTunnel::Clock::now()<header_deadline&&request.size()<=65536) {
            const auto n=recv(socket,chunk.data(),static_cast<int>(chunk.size()),0);
            if(n>0){request.append(chunk.data(),static_cast<size_t>(n));end=request.find("\r\n\r\n");if(end!=std::string::npos)break;}
            else if(n==0||!again())return;else std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(end==std::string::npos||end>65536||stop)return;
        const auto line_end=request.find("\r\n");std::istringstream first(request.substr(0,line_end));std::string method,url,version,extra;
        first>>method>>url>>version;if(method.empty()||url.empty()||(version!="HTTP/1.1"&&version!="HTTP/1.0")||(first>>extra))return;
        Target target;try{target=parse_target(method=="CONNECT"?"http://"+url:url);}catch(...){return;}
        if(method!="CONNECT"&&target.scheme!="http")return;
        ProxyTunnel tunnel;
        if(!tunnel.connect(proxy,target.host,target.port,header_deadline,stop)) {
            constexpr char response[]="HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            ::send(socket,response,sizeof(response)-1,0);return;
        }
        std::string to_upstream,to_client;
        if(method=="CONNECT") {
            to_client="HTTP/1.1 200 Connection Established\r\n\r\n";
            to_upstream=request.substr(end+4);
        } else {
            to_upstream=method+" "+target.path+" "+version+"\r\n";bool upgrade=false;
            size_t at=line_end+2;
            while(at<end) {
                const auto next=request.find("\r\n",at);if(next==std::string::npos)return;
                const auto header=request.substr(at,next-at);const auto colon=header.find(':');if(colon==std::string::npos)return;
                const auto name=lower(trim(header.substr(0,colon)));
                if(name=="upgrade")upgrade=true;
                if(name!="proxy-authorization"&&name!="proxy-connection"&&name!="connection")to_upstream+=header+"\r\n";
                at=next+2;
            }
            to_upstream+=upgrade?"Connection: Upgrade\r\n\r\n":"Connection: close\r\n\r\n";
            to_upstream+=request.substr(end+4);
        }
        bool upstream_eof=false;
        while(!stop&&ProxyTunnel::Clock::now()<deadline) {
            bool progress=false;
            if(!to_upstream.empty()) {
                size_t n=0;const auto code=tunnel.send(to_upstream.data(),to_upstream.size(),n);
                if(code!=CURLE_OK&&code!=CURLE_AGAIN)return;
                if(n){to_upstream.erase(0,n);progress=true;}
            }
            if(!to_client.empty()) {
                const auto n=::send(socket,to_client.data(),static_cast<int>(to_client.size()),0);
                if(n>0){to_client.erase(0,static_cast<size_t>(n));progress=true;}else if(n==0||!again())return;
            }
            if(upstream_eof&&to_client.empty())return;
            if(!upstream_eof&&to_client.size()<65536) {
                size_t n=0;const auto code=tunnel.receive(chunk.data(),chunk.size(),n);
                if(n){to_client.append(chunk.data(),n);progress=true;}
                else if(code==CURLE_OK)upstream_eof=true;
                else if(code!=CURLE_AGAIN)return;
            }
            if(!upstream_eof&&to_upstream.size()<65536) {
                const auto n=recv(socket,chunk.data(),static_cast<int>(chunk.size()),0);
                if(n>0){to_upstream.append(chunk.data(),static_cast<size_t>(n));progress=true;}else if(n==0||!again())return;
            }
            if(!progress)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};
BrowserProxy::BrowserProxy(const ProxyConfig& p,std::atomic_bool& cancel,ProxyTunnel::Clock::time_point deadline):impl_(std::make_unique<Impl>(p,cancel,deadline)){}
BrowserProxy::~BrowserProxy()=default;
std::string BrowserProxy::endpoint()const{return "http://127.0.0.1:"+std::to_string(impl_->port);}
}
