#include "vector/model.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <array>
#include <cstddef>
#include <cstring>
#endif

namespace vectorapp {
#ifdef _WIN32
namespace {


#pragma pack(push,1)
struct EchoV6Address {
    USHORT port;
    ULONG flow;
    unsigned char address[16];
    ULONG scope;
};
#pragma pack(pop)
struct EchoV6Reply { EchoV6Address address; ULONG status, round_trip_ms; };
static_assert(offsetof(EchoV6Address,address)==6&&offsetof(EchoV6Address,scope)==22&&sizeof(EchoV6Address)==26);
static_assert(offsetof(EchoV6Reply,status)==28&&sizeof(EchoV6Reply)==36);
}
#endif
IcmpObservation inspect_icmp(const std::string& address,std::atomic_bool& cancel) {
    IcmpObservation result;result.address=address;
    if(cancel){result.state="cancelled";result.detail="ICMP check cancelled.";return result;}
#ifdef _WIN32
    sockaddr_in v4{};sockaddr_in6 v6{};
    const bool ipv4=inet_pton(AF_INET,address.c_str(),&v4.sin_addr)==1;
    if(!ipv4&&inet_pton(AF_INET6,address.c_str(),&v6.sin6_addr)!=1) {
        result.state="error";result.detail="ICMP requires a valid resolved IP address.";return result;
    }
    struct IcmpHandle {
        HANDLE value;
        ~IcmpHandle(){if(value!=INVALID_HANDLE_VALUE)IcmpCloseHandle(value);}
    } handle{ipv4?IcmpCreateFile():Icmp6CreateFile()};
    if(handle.value==INVALID_HANDLE_VALUE) {
        result.state="unavailable";result.status_code=GetLastError();result.detail="Windows ICMP is unavailable ("+std::to_string(result.status_code)+").";return result;
    }
    char payload[]="Vector endpoint check";
    alignas(ICMP_ECHO_REPLY) std::array<unsigned char,1024> buffer{};
    DWORD count=0;
    if(ipv4)count=IcmpSendEcho(handle.value,v4.sin_addr.s_addr,payload,sizeof(payload),nullptr,buffer.data(),static_cast<DWORD>(buffer.size()),750);
    else {
        sockaddr_in6 source{};source.sin6_family=AF_INET6;v6.sin6_family=AF_INET6;
        count=Icmp6SendEcho2(handle.value,nullptr,nullptr,nullptr,&source,&v6,payload,sizeof(payload),nullptr,buffer.data(),static_cast<DWORD>(buffer.size()),750);
    }
    if(cancel){result.state="cancelled";result.detail="ICMP check cancelled.";return result;}
    if(count) {
        char text[INET6_ADDRSTRLEN]{};
        if(ipv4) {
            const auto& reply=*reinterpret_cast<const ICMP_ECHO_REPLY*>(buffer.data());
            result.status_code=reply.Status;result.round_trip_ms=reply.RoundTripTime;result.ttl=reply.Options.Ttl;
            inet_ntop(AF_INET,&reply.Address,text,sizeof(text));result.reply_address=text;
            if(reply.Status==IP_SUCCESS&&(reply.Address!=v4.sin_addr.s_addr||reply.DataSize!=sizeof(payload)||!reply.Data||std::memcmp(reply.Data,payload,sizeof(payload))!=0)) {
                result.state="error";result.detail="ICMP reply did not match the requested echo.";return result;
            }
        } else {
            const auto& reply=*reinterpret_cast<const EchoV6Reply*>(buffer.data());
            result.status_code=reply.status;result.round_trip_ms=reply.round_trip_ms;
            inet_ntop(AF_INET6,reply.address.address,text,sizeof(text));result.reply_address=text;
            if(reply.status==IP_SUCCESS&&std::memcmp(reply.address.address,&v6.sin6_addr,16)!=0) {
                result.state="error";result.detail="ICMPv6 reply came from a different address.";return result;
            }
        }
    } else result.status_code=GetLastError();
    if(count&&result.status_code==IP_SUCCESS) {
        result.state="reply";result.detail=std::string(ipv4?"ICMP":"ICMPv6")+" echo reply from "+result.reply_address;
    } else if(result.status_code==IP_REQ_TIMED_OUT) {
        result.state="no_reply";result.detail="No ICMP echo reply within 750 ms. Filtering or packet loss is possible.";
    } else if(result.status_code==IP_DEST_NET_UNREACHABLE||result.status_code==IP_DEST_HOST_UNREACHABLE||
              result.status_code==IP_DEST_PROT_UNREACHABLE||result.status_code==IP_DEST_PORT_UNREACHABLE||
              result.status_code==IP_TTL_EXPIRED_TRANSIT||result.status_code==IP_TTL_EXPIRED_REASSEM) {
        result.state="unreachable";result.detail="ICMP reported an unreachable destination or expired TTL ("+std::to_string(result.status_code)+").";
    } else {
        result.state="error";result.detail="ICMP check failed (Windows status "+std::to_string(result.status_code)+").";
    }
#else
    result.state="unavailable";result.detail="ICMP checks are currently supported on Windows.";
#endif
    return result;
}
}
