#include "vector/proxy.hpp"
#include "vector/model.hpp"
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace vectorapp {
namespace {
bool controls(const std::string& value) {
    return std::any_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;});
}
}
std::string proxy_endpoint(const ProxyConfig& p) {
    const auto address=(p.host.find(':')!=std::string::npos?"["+p.host+"]":p.host)+":"+std::to_string(p.port);
    return p.scheme=="auto"?address:p.scheme+"://"+address;
}
void validate_proxy(const ProxyConfig& p) {
    if(!p.enabled)return;
    if(p.scheme!="auto"&&p.scheme!="http"&&p.scheme!="https"&&p.scheme!="socks5"&&p.scheme!="all")
        throw std::invalid_argument("Enter the proxy as host:port. A type is optional: http, https or socks5.");
    if(p.host.empty()||p.host.size()>253||!p.port||controls(p.host)||p.host.find_first_of(" /@?#\\")!=std::string::npos)
        throw std::invalid_argument("Enter a proxy hostname or IP and a port from 1 to 65535.");
    try { (void)parse_target((p.host.find(':')!=std::string::npos?"["+p.host+"]":p.host)+":"+std::to_string(p.port)); }
    catch(...) { throw std::invalid_argument("Enter a valid proxy hostname or IP address."); }
    if(controls(p.username)||controls(p.password)||p.username.size()>1024||p.password.size()>1024)
        throw std::invalid_argument("Proxy credentials are too long or contain a control character.");
    if(p.username.empty()&&!p.password.empty())throw std::invalid_argument("Enter a username for the proxy password.");
    if(p.scheme=="socks5"&&(p.username.size()>255||p.password.size()>255))throw std::invalid_argument("SOCKS5 credentials must each fit in 255 bytes.");
}
ProxyConfig parse_proxy(const std::string& input) {
    auto text=trim(input);if(text.empty())throw std::invalid_argument("Enter a proxy address.");
    if(text.find("://")==std::string::npos)text="auto://"+text;
    const auto begin=text.find("://")+3,at=text.find('@',begin);
    const auto first=text.find(':',begin),second=first==std::string::npos?first:text.find(':',first+1),third=second==std::string::npos?second:text.find(':',second+1);
    const bool bracketed=begin<text.size()&&text[begin]=='[';
    const bool named=text.find_first_not_of("0123456789abcdefABCDEF",begin)<first;
    if(third!=std::string::npos&&third<at&&named&&!bracketed&&second>first+1&&text.find_first_not_of("0123456789",first+1)==second&&third>second+1) {
        const auto escape=[](const std::string& value) {
            std::unique_ptr<char,decltype(&curl_free)> raw(curl_easy_escape(nullptr,value.c_str(),static_cast<int>(value.size())),curl_free);
            if(!raw)throw std::invalid_argument("Enter a proxy URL or host:port.");
            return std::string(raw.get());
        };
        text=text.substr(0,begin)+escape(text.substr(second+1,third-second-1))+":"+escape(text.substr(third+1))+"@"+text.substr(begin,second-begin);
    } else if(second!=std::string::npos&&at==std::string::npos&&!named&&!bracketed)text=text.substr(0,begin)+"["+text.substr(begin)+"]";
    std::unique_ptr<CURLU,decltype(&curl_url_cleanup)> url(curl_url(),curl_url_cleanup);
    if(!url||curl_url_set(url.get(),CURLUPART_URL,text.c_str(),CURLU_NON_SUPPORT_SCHEME)!=CURLUE_OK)
        throw std::invalid_argument("Enter a proxy URL or host:port.");
    auto part=[&](CURLUPart key,unsigned flags=0U){char* raw=nullptr;std::string value;const auto code=curl_url_get(url.get(),key,&raw,flags);if(code==CURLUE_OK){value=raw;curl_free(raw);}else if(code==CURLUE_URLDECODE)throw std::invalid_argument("Proxy credentials contain invalid URL encoding.");return value;};
    ProxyConfig p;p.enabled=true;p.scheme=lower(part(CURLUPART_SCHEME));
    if(p.scheme=="socks5h")p.scheme="socks5";
    p.host=part(CURLUPART_HOST);if(p.host.starts_with('[')&&p.host.ends_with(']'))p.host=p.host.substr(1,p.host.size()-2);
    const auto port=part(CURLUPART_PORT);p.port=port.empty()?(p.scheme=="https"?443:p.scheme=="socks5"?1080:p.scheme=="http"?80:8080):static_cast<uint16_t>(std::stoi(port));
    p.username=part(CURLUPART_USER,CURLU_URLDECODE);p.password=part(CURLUPART_PASSWORD,CURLU_URLDECODE);
    const auto path=part(CURLUPART_PATH);
    if((!path.empty()&&path!="/")||!part(CURLUPART_QUERY).empty()||!part(CURLUPART_FRAGMENT).empty())throw std::invalid_argument("A proxy address cannot include a path, query or fragment.");
    validate_proxy(p);return p;
}
std::filesystem::path proxy_settings_path() {
#ifdef _WIN32
    if(const auto* base=_wgetenv(L"LOCALAPPDATA"))return std::filesystem::path(base)/L"Vector"/L"proxy.dat";
#endif
    return {};
}
ProxyConfig load_proxy_settings(const std::filesystem::path& path) {
    if(path.empty()||!std::filesystem::exists(path))return {};
#ifdef _WIN32
    const auto size=std::filesystem::file_size(path);if(!size||size>32768)throw std::runtime_error("Saved proxy settings are invalid; save them again.");
    std::ifstream file(path,std::ios::binary);std::string bytes(static_cast<size_t>(size),'\0');
    if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("Could not read proxy settings.");
    DATA_BLOB input{static_cast<DWORD>(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},output{};
    if(!CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output))throw std::runtime_error("Windows could not unlock the saved proxy settings for this account.");
    std::string decoded(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);
    try {
        const auto j=nlohmann::json::parse(decoded);SecureZeroMemory(decoded.data(),decoded.size());ProxyConfig p;
        p.enabled=j.at("enabled");p.scheme=j.at("scheme");p.host=j.at("host");p.port=j.at("port").get<uint16_t>();p.username=j.at("username");p.password=j.at("password");validate_proxy(p);return p;
    }catch(...){SecureZeroMemory(decoded.data(),decoded.size());throw std::runtime_error("Saved proxy settings are invalid; save them again.");}
#else
    throw std::runtime_error("Protected proxy storage is supported on Windows only.");
#endif
}
void save_proxy_settings(const ProxyConfig& p,const std::filesystem::path& path) {
    validate_proxy(p);
#ifdef _WIN32
    if(path.empty())throw std::runtime_error("Windows user settings folder is unavailable.");
    std::string encoded=nlohmann::json{{"enabled",p.enabled},{"scheme",p.scheme},{"host",p.host},{"port",p.port},{"username",p.username},{"password",p.password}}.dump();
    DATA_BLOB input{static_cast<DWORD>(encoded.size()),reinterpret_cast<BYTE*>(encoded.data())},output{};
    const bool ok=CryptProtectData(&input,L"Vector proxy",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)!=FALSE;
    SecureZeroMemory(encoded.data(),encoded.size());if(!ok)throw std::runtime_error("Windows could not protect the proxy settings.");
    std::string encrypted(reinterpret_cast<char*>(output.pbData),output.cbData);LocalFree(output.pbData);
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    auto temporary=path;temporary+=L".tmp";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file.write(encrypted.data(),static_cast<std::streamsize>(encrypted.size()));file.flush();if(!file)throw std::runtime_error("Could not save proxy settings.");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace proxy settings.");
#else
    (void)path;throw std::runtime_error("Protected proxy storage is supported on Windows only.");
#endif
}
}
