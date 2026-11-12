#include "vector/api_endpoints.hpp"
#include "markup.hpp"
#include "tag_config.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

namespace vectorapp {
namespace {
using Json=nlohmann::json;
constexpr size_t kCodeBytes=2*1024*1024;
constexpr size_t kLiteralBytes=400;
constexpr size_t kDescriptionOperations=128;
constexpr size_t kRobotsRules=48;

bool one_of(std::string_view value,std::initializer_list<std::string_view> list) {
    return std::find(list.begin(),list.end(),value)!=list.end();
}
bool alnum(char c){return std::isalnum(static_cast<unsigned char>(c))!=0;}
bool digit(char c){return c>='0'&&c<='9';}
bool letter(char c){return std::isalpha(static_cast<unsigned char>(c))!=0;}
bool blank(char c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f';}
bool word(char c){return alnum(c)||c=='_'||c=='$';}
std::string upper(std::string value) {
    for(auto& c:value)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
bool http_method(std::string_view value) {
    return one_of(value,{"GET","POST","PUT","PATCH","DELETE","HEAD","OPTIONS"});
}
std::vector<std::string> segments(std::string_view path) {
    std::vector<std::string> out;size_t at=0;
    while(at<path.size()) {
        const auto end=path.find('/',at);
        const auto part=path.substr(at,end==std::string_view::npos?path.size()-at:end-at);
        if(!part.empty())out.emplace_back(part);
        if(end==std::string_view::npos)break;
        at=end+1;
    }
    return out;
}

bool api_word(std::string_view value) {
    return one_of(value,{"api","apis","graphql","gql","rest","rpc","jsonrpc","json-rpc","trpc","ajax","wp-json","xmlrpc",
        "webapi","openapi","oauth","oauth2","grpc","odata"});
}
bool dynamic_extension(std::string_view value) {
    return one_of(value,{"php","asp","aspx","ashx","asmx","jsp","do","action","cgi","svc"});
}
bool content_word(std::string_view value) {
    return one_of(value,{"docs","doc","documentation","developers","developer","dev","reference","references","guides","guide","learn",
        "blog","help","support","product","products","solutions","features","resources","legal","about","changelog","pricing",
        "tutorials","tutorial","handbook","academy","news","posts","articles"});
}
bool content_host(std::string host) {
    host=lower(host.substr(0,host.find(':')));
    size_t at=0;
    while(at<host.size()) {
        const auto end=host.find('.',at);
        if(one_of(std::string_view(host).substr(at,end==std::string::npos?std::string::npos:end-at),
            {"docs","doc","documentation","developers","developer","support","help","blog","learn","status","community","forum"}))return true;
        if(end==std::string::npos)break;
        at=end+1;
    }
    return false;
}
bool asset_directory(std::string_view value) {
    return one_of(value,{"static","assets","asset","_next","_nuxt","_astro","dist","build","public","images","image","img","fonts","font",
        "css","js","media","locales","locale","i18n","translations","lang","icons","videos"});
}
bool locale_segment(std::string_view value) {
    if(value.size()!=2&&value.size()!=5)return false;
    if(!std::islower(static_cast<unsigned char>(value[0]))||!std::islower(static_cast<unsigned char>(value[1])))return false;
    return value.size()==2||(value[2]=='-'&&letter(value[3])&&letter(value[4]));
}
bool version_segment(std::string_view value) {
    if(value.size()<2||value.size()>6||(value[0]!='v'&&value[0]!='V'))return false;
    size_t i=1,digits=0;
    while(i<value.size()&&digit(value[i])){++i;++digits;}
    if(!digits||digits>2)return false;
    if(i==value.size())return true;
    if(value[i++]!='.')return false;
    digits=0;while(i<value.size()&&digit(value[i])){++i;++digits;}
    return i==value.size()&&digits>0&&digits<=2;
}
bool marker_segment(std::string segment) {
    segment=lower(std::move(segment));
    const auto dot=segment.rfind('.');
    if(dot!=std::string::npos&&dot>0) {
        const auto extension=std::string_view(segment).substr(dot+1);
        if(dynamic_extension(extension)||extension=="json")segment.resize(dot);
    }
    if(api_word(segment))return true;
    auto end=segment.size();size_t digits=0;
    while(end>0&&digit(segment[end-1])&&digits<3){--end;++digits;}
    if(digits) {
        if(end>0&&segment[end-1]=='v')--end;
        if(end>0&&(segment[end-1]=='-'||segment[end-1]=='_'||segment[end-1]=='.'))--end;
        if(end>0)segment.resize(end);
    }
    if(api_word(segment))return true;
    const auto separator=segment.find_first_of("-_.");
    if(separator==std::string::npos||separator==0)return false;
    if(!std::all_of(segment.begin(),segment.begin()+static_cast<std::ptrdiff_t>(separator),alnum))return false;
    return api_word(std::string_view(segment).substr(separator+1));
}
bool path_marker(std::string_view path) {
    const auto parts=segments(path);
    const auto prose=[&](size_t before) {
        for(size_t i=0;i<before;++i)if(content_word(lower(parts[i]))||locale_segment(parts[i]))return true;
        return false;
    };
    for(size_t i=0;i<parts.size()&&i<6;++i)if(marker_segment(parts[i]))return !prose(i);
    if(parts.size()>=2&&version_segment(parts[0]))return true;
    return parts.size()>=3&&version_segment(parts[1])&&!prose(1);
}
bool host_marker(std::string host) {
    host=lower(host.substr(0,host.find(':')));
    std::vector<std::string> labels;size_t at=0;
    while(at<=host.size()) {
        const auto end=host.find('.',at);
        labels.push_back(host.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    if(labels.size()<2)return false;
    labels.pop_back();
    for(auto label:labels) {
        while(!label.empty()&&digit(label.back()))label.pop_back();
        size_t start=0;
        while(start<=label.size()) {
            const auto end=label.find('-',start);
            const auto part=std::string_view(label).substr(start,end==std::string::npos?std::string::npos:end-start);
            if(one_of(part,{"api","apis","graphql","gql","rest","rpc","grpc","gateway"}))return true;
            if(end==std::string::npos)break;
            start=end+1;
        }
    }
    return false;
}
std::string extension_of(std::string_view path) {
    while(!path.empty()&&path.back()=='/')path.remove_suffix(1);
    const auto slash=path.rfind('/');
    const auto last=slash==std::string_view::npos?path:path.substr(slash+1);
    const auto dot=last.rfind('.');
    if(dot==std::string_view::npos||dot==0||dot+1==last.size())return {};
    const auto extension=last.substr(dot+1);
    if(extension.size()>12||!std::all_of(extension.begin(),extension.end(),alnum))return {};
    return lower(std::string(extension));
}
bool file_like(std::string_view path) {
    const auto extension=extension_of(path);
    return !extension.empty()&&!dynamic_extension(extension);
}
bool asset_path(std::string_view path) {
    return one_of(extension_of(path),{"js","mjs","cjs","css","map","png","jpg","jpeg","gif","svg","webp","avif","ico","bmp","woff","woff2",
        "ttf","otf","eot","mp4","webm","mov","mp3","wav","ogg","pdf","zip","gz","wasm","html","htm","md","txt","xml","lottie"});
}
std::string mime_of(const std::string& value) {
    return lower(trim(value.substr(0,value.find(';'))));
}
bool json_mime(const std::string& mime) {
    return mime=="application/json"||mime=="text/json"||mime=="application/x-ndjson"||mime=="application/ld+json"||
        (mime.starts_with("application/")&&mime.ends_with("+json")&&mime!="application/manifest+json");
}
bool page_mime(const std::string& mime) {
    return mime=="text/html"||mime=="application/xhtml+xml"||mime=="text/x-component";
}
bool asset_mime(const std::string& mime) {
    return page_mime(mime)||mime=="text/css"||mime=="text/javascript"||mime=="application/javascript"||mime=="application/x-javascript"||
        mime=="text/ecmascript"||mime=="application/ecmascript"||mime=="application/wasm"||mime=="application/manifest+json"||mime=="text/vtt"||
        mime.starts_with("image/")||mime.starts_with("font/")||mime.starts_with("audio/")||mime.starts_with("video/")||mime.starts_with("application/font-");
}

struct Parts { std::string scheme,host,path;unsigned port=0; };
unsigned default_port(const std::string& scheme){return scheme=="https"||scheme=="wss"?443U:80U;}
std::string origin(const Parts& p) {
    return p.scheme+"://"+p.host+(p.port==default_port(p.scheme)?std::string{}:":"+std::to_string(p.port));
}
std::optional<Parts> parse_url(const std::string& value,const std::string& base={}) {
    if(value.empty()||value.size()>4096)return {};
    for(unsigned char c:value)if(c<=32||c==127||c=='\\')return {};
    std::unique_ptr<CURLU,decltype(&curl_url_cleanup)> u(curl_url(),curl_url_cleanup);
    if(!u)return {};
    if(!base.empty()&&curl_url_set(u.get(),CURLUPART_URL,base.c_str(),CURLU_NON_SUPPORT_SCHEME)!=CURLUE_OK)return {};
    if(curl_url_set(u.get(),CURLUPART_URL,value.c_str(),CURLU_NON_SUPPORT_SCHEME)!=CURLUE_OK)return {};
    const auto get=[&](CURLUPart part) {
        char* data=nullptr;std::string out;
        if(curl_url_get(u.get(),part,&data,0)==CURLUE_OK&&data){out=data;curl_free(data);}
        return out;
    };
    Parts p;p.scheme=lower(get(CURLUPART_SCHEME));
    if(p.scheme!="http"&&p.scheme!="https"&&p.scheme!="ws"&&p.scheme!="wss")return {};
    if(!get(CURLUPART_USER).empty()||!get(CURLUPART_PASSWORD).empty())return {};
    p.host=lower(get(CURLUPART_HOST));
    while(!p.host.empty()&&p.host.back()=='.')p.host.pop_back();
    if(p.host.empty()||p.host.find('%')!=std::string::npos)return {};
    const auto port=get(CURLUPART_PORT);
    if(port.empty())p.port=default_port(p.scheme);
    else {
        if(port.size()>5||!std::all_of(port.begin(),port.end(),digit))return {};
        const auto number=std::stoul(port);if(!number||number>65535)return {};
        p.port=static_cast<unsigned>(number);
    }
    p.path=get(CURLUPART_PATH);if(p.path.empty())p.path="/";
    return p;
}
std::string without_query(const std::string& url){return url.substr(0,url.find_first_of("?#"));}
std::string clean(const std::string& url) {
    const auto p=parse_url(without_query(url));
    return p?printable(origin(*p)+p->path,1024):std::string{};
}

bool uuid(std::string_view value) {
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i) {
        const bool dash=i==8||i==13||i==18||i==23;
        if(dash?value[i]!='-':!std::isxdigit(static_cast<unsigned char>(value[i])))return false;
    }
    return true;
}
bool opaque_run(std::string_view run) {
    if(run.size()<16)return false;
    size_t digits=0,letters=0,changes=0;
    for(size_t i=0;i<run.size();++i) {
        digit(run[i])?++digits:++letters;
        if(i&&digit(run[i])!=digit(run[i-1]))++changes;
    }
    return digits>=2&&letters>=2&&changes>=3;
}
bool opaque_token(std::string_view value) {
    size_t start=0;
    for(size_t i=0;i<=value.size();++i) {
        if(i<value.size()&&alnum(value[i]))continue;
        if(opaque_run(value.substr(start,i-start)))return true;
        start=i+1;
    }
    return false;
}
bool opaque_segment(std::string_view value) {
    if(uuid(value))return true;
    if(value.size()>=6&&std::all_of(value.begin(),value.end(),digit))return true;
    return opaque_token(value);
}
std::string collapse_path(const std::string& path) {
    std::string out;size_t at=0;
    while(at<path.size()) {
        const auto end=path.find('/',at);
        const auto part=path.substr(at,end==std::string::npos?std::string::npos:end-at);
        out+=opaque_segment(part)?"{id}":part;
        if(end==std::string::npos)break;
        out+='/';at=end+1;
    }
    return out.empty()?"/":out;
}
std::string collapse_host(const std::string& host) {
    if(host.find(':')!=std::string::npos||std::all_of(host.begin(),host.end(),[](char c){return digit(c)||c=='.';}))return host;
    std::vector<std::string> labels;size_t at=0;
    while(at<=host.size()) {
        const auto end=host.find('.',at);
        labels.push_back(host.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    std::string out;
    for(size_t i=0;i<labels.size();++i) {
        if(i)out+='.';
        out+=i+2<labels.size()&&opaque_token(labels[i])?"{id}":labels[i];
    }
    return out;
}

bool placeholder_segment(std::string_view value) {
    return value.find('{')!=std::string_view::npos||value.starts_with(':')||(value.starts_with('[')&&value.ends_with(']'));
}
bool same_path(const std::vector<std::string>& a,const std::vector<std::string>& b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(a[i]!=b[i]&&!placeholder_segment(a[i])&&!placeholder_segment(b[i]))return false;
    return true;
}
bool path_prefix(const std::vector<std::string>& a,const std::vector<std::string>& b) {
    if(a.size()>=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(a[i]!=b[i])return false;
    return true;
}

bool valid_host(std::string host) {
    host=lower(std::move(host));
    const auto colon=host.find(':');
    if(colon!=std::string::npos) {
        const auto port=host.substr(colon+1);
        if(port.empty()||port.size()>5||!std::all_of(port.begin(),port.end(),digit))return false;
        const auto number=std::stoul(port);if(!number||number>65535)return false;
        host.resize(colon);
    }
    if(host.size()<4||host.size()>253)return false;
    std::vector<std::string> labels;size_t at=0;
    while(at<=host.size()) {
        const auto end=host.find('.',at);
        labels.push_back(host.substr(at,end==std::string::npos?std::string::npos:end-at));
        if(end==std::string::npos)break;
        at=end+1;
    }
    if(labels.size()<2)return false;
    for(const auto& label:labels) {
        if(label.empty()||label.size()>63||label.front()=='-'||label.back()=='-')return false;
        if(!std::all_of(label.begin(),label.end(),[](char c){return alnum(c)||c=='-';}))return false;
    }
    const auto& top=labels.back();
    if(top.size()<2||top.size()>24)return false;
    if(!top.starts_with("xn--")&&!std::all_of(top.begin(),top.end(),letter))return false;
    if(one_of(top,{"test","invalid","localhost","local"}))return false;
    const auto site=labels[labels.size()-2]+"."+top;
    return !one_of(site,{"example.com","example.org","example.net","domain.com","yourdomain.com","your-domain.com","mydomain.com","yoursite.com","mysite.com"});
}
bool code_path(std::string_view path,bool rooted) {
    if(path.empty()||path[0]!='/'||path.size()>200)return false;
    bool letters=false;
    for(size_t i=0;i<path.size();++i) {
        const char c=path[i];
        if(letter(c))letters=true;
        if(c=='{'){if(i+1>=path.size()||path[i+1]!='}')return false;++i;continue;}
        if(!alnum(c)&&std::string_view("._~%:@/[]*+-").find(c)==std::string_view::npos)return false;
        if(c=='/'&&i+1<path.size()&&path[i+1]=='/')return false;
    }
    if(path.find("..")!=std::string_view::npos)return false;
    const auto parts=segments(path);
    if(parts.size()>12)return false;
    for(const auto& part:parts)if(part.front()=='*'||std::none_of(part.begin(),part.end(),[](char c){return alnum(c)||c=='{';}))return false;
    return rooted||(letters&&!parts.empty());
}

struct StringEnd { size_t end=0;bool closed=false; };
StringEnd skip_string(std::string_view code,size_t start,int depth=0) {
    const char quote=code[start];size_t i=start+1;
    while(i<code.size()) {
        const char c=code[i];
        if(c=='\\'){i+=2;continue;}
        if(c==quote)return {i+1,true};
        if(c=='\n'&&quote!='`')return {i,false};
        if(quote=='`'&&c=='$'&&i+1<code.size()&&code[i+1]=='{') {
            i+=2;int braces=1;
            while(i<code.size()&&braces) {
                const char inner=code[i];
                if((inner=='\''||inner=='"'||inner=='`')&&depth<6) {
                    const auto nested=skip_string(code,i,depth+1);
                    if(!nested.closed&&inner=='`')return {code.size(),false};
                    i=std::max(nested.end,i+1);continue;
                }
                if(inner=='{')++braces;else if(inner=='}')--braces;
                ++i;
            }
            continue;
        }
        ++i;
    }
    return {code.size(),false};
}
struct Literal { std::string text;bool query=false,valid=false; };
Literal literal_text(std::string_view code,size_t start,size_t end) {
    Literal out;const char quote=code[start];
    size_t i=start+1;const size_t stop=end-1;
    while(i<stop) {
        const char c=code[i];
        if(c=='\\') {
            const auto rest=code.substr(i+1,std::min<size_t>(5,stop-i-1));
            if(rest.starts_with('/')){out.text+='/';i+=2;}
            else if(rest.size()>=5&&(rest[0]=='u')&&lower(std::string(rest.substr(1,4)))=="002f"){out.text+='/';i+=6;}
            else if(rest.size()>=3&&rest[0]=='x'&&lower(std::string(rest.substr(1,2)))=="2f"){out.text+='/';i+=4;}
            else return out;
            continue;
        }
        if(quote=='`'&&c=='$'&&i+1<stop&&code[i+1]=='{') {
            i+=2;int braces=1;
            while(i<stop&&braces) {
                const char inner=code[i];
                if(inner=='\''||inner=='"'||inner=='`'){i=std::max(skip_string(code,i,1).end,i+1);continue;}
                if(inner=='{')++braces;else if(inner=='}')--braces;
                ++i;
            }
            out.text+="{}";continue;
        }
        if(c=='?'||c=='#'){out.query=true;break;}
        if(static_cast<unsigned char>(c)<=32||static_cast<unsigned char>(c)>=127)return out;
        out.text+=c;++i;
        if(out.text.size()>kLiteralBytes)return out;
    }
    out.valid=out.text.size()>=2;
    return out;
}
bool candidate(std::string_view code,size_t start,size_t end) {
    if(end<start+4||end-start>kLiteralBytes*4)return false;
    const auto body=code.substr(start+1,end-start-2);
    if(body[0]=='/')return true;
    if(body.starts_with("${"))return code[start]=='`';
    if(body.size()<8)return false;
    const auto head=lower(std::string(body.substr(0,6)));
    return head.starts_with("http:")||head.starts_with("https:")||head.starts_with("ws://")||head.starts_with("wss://");
}

size_t back_blank(std::string_view code,size_t i){while(i>0&&blank(code[i-1]))--i;return i;}
size_t skip_blank(std::string_view text,size_t i){while(i<text.size()&&blank(text[i]))++i;return i;}
std::string method_key(std::string_view object) {
    for(const std::string_view key:{"method","type"}) {
        for(auto at=object.find(key);at!=std::string_view::npos;at=object.find(key,at+1)) {
            if(at>0&&word(object[at-1]))continue;
            size_t i=at+key.size();
            if(i<object.size()&&(object[i]=='"'||object[i]=='\''))++i;
            i=skip_blank(object,i);
            if(i>=object.size()||object[i]!=':')continue;
            i=skip_blank(object,i+1);
            if(i>=object.size()||(object[i]!='"'&&object[i]!='\''))continue;
            const char quote=object[i++];const auto close=object.find(quote,i);
            if(close==std::string_view::npos||close-i>7)continue;
            const auto value=upper(std::string(object.substr(i,close-i)));
            if(http_method(value))return value;
        }
    }
    return {};
}
std::string_view object_after(std::string_view code,size_t end,bool argument) {
    auto i=skip_blank(code,end);
    if(argument) {
        if(i>=code.size()||code[i]!=',')return {};
        i=skip_blank(code,i+1);
        if(i>=code.size()||code[i]!='{')return {};
    }
    const size_t start=i;int braces=argument?0:1;
    for(;i<code.size()&&i-start<240;++i) {
        const char c=code[i];
        if(c=='\''||c=='"'||c=='`'){i=std::max(skip_string(code,i,6).end,i+1)-1;continue;}
        if(c=='{')++braces;
        else if(c=='}'&&--braces<=0)break;
    }
    return code.substr(start,std::min(i,code.size())-start);
}

struct Context { std::string label,method,object;bool request=false,named=false,endpoint=false,writer=false; };
bool http_client(std::string object) {
    const auto dot=object.rfind('.');
    if(dot!=std::string::npos)object=object.substr(dot+1);
    object=lower(object);
    while(!object.empty()&&(object.front()=='$'||object.front()=='_'))object.erase(0,1);
    return one_of(object,{"axios","axiosinstance","http","httpclient","httpservice","api","apiclient","apiservice","client","restclient",
        "rest","request","superagent","ky","got","ofetch","fetcher","ajax","xhr","instance","service"});
}
bool fetch_function(const std::string& callee,const std::string& tail) {
    return tail=="ajax"||one_of(callee,{"$fetch","ofetch","ky","got","useFetch","useLazyFetch","useSWR","useSWRImmutable","useSWRInfinite",
        "useSWRMutation","request","superagent"});
}
bool request_keys(std::string_view object) {
    for(const std::string_view key:{"data","headers","body","params","dataType","contentType","xhrFields","withCredentials","responseType",
        "beforeSend","credentials","success"}) {
        for(auto at=object.find(key);at!=std::string_view::npos;at=object.find(key,at+1)) {
            if(at>0&&word(object[at-1]))continue;
            size_t i=at+key.size();
            if(i<object.size()&&(object[i]=='"'||object[i]=='\''))++i;
            i=skip_blank(object,i);
            if(i<object.size()&&object[i]==':')return true;
        }
    }
    return false;
}
size_t object_open(std::string_view code,size_t begin) {
    size_t i=begin;int depth=0;
    while(i>0&&begin-i<240) {
        const char c=code[--i];
        if(c=='"'||c=='\''||c=='`') {
            size_t j=i;bool found=false;
            while(j>0&&i-j<160){--j;if(code[j]==c&&(j==0||code[j-1]!='\\')){found=true;break;}}
            if(!found)return std::string_view::npos;
            i=j;continue;
        }
        if(c=='}'||c==']'||c==')')++depth;
        else if(c=='{'||c=='['||c=='(') {
            if(depth==0)return c=='{'?i:std::string_view::npos;
            --depth;
        }
    }
    return std::string_view::npos;
}
std::string owner_name(std::string_view code,size_t open) {
    auto i=back_blank(code,open);
    if(!i||(code[i-1]!=':'&&code[i-1]!='='))return {};
    if(code[i-1]=='='&&i>=2&&std::string_view("=!<>").find(code[i-2])!=std::string_view::npos)return {};
    i=back_blank(code,i-1);
    if(i&&(code[i-1]=='"'||code[i-1]=='\''))--i;
    auto begin=i;
    while(begin>0&&i-begin<48&&word(code[begin-1]))--begin;
    return lower(std::string(code.substr(begin,i-begin)));
}
bool endpoint_name(const std::string& name) {
    if(name.find("endpoint")!=std::string::npos)return true;
    if(!name.starts_with("api"))return false;
    return one_of(std::string_view(name).substr(3),{"","s","url","_url","uri","path","_path","paths","route","routes","_route","root","base","prefix"});
}
Context context_of(std::string_view code,size_t start,size_t end,const std::set<std::string>& clients) {
    Context out;
    const auto before=back_blank(code,start);
    if(!before)return out;
    const char previous=code[before-1];
    if(previous=='(') {
        const auto stop=back_blank(code,before-1);auto begin=stop;
        while(begin>0&&stop-begin<64&&(word(code[begin-1])||code[begin-1]=='.'))--begin;
        const auto callee=std::string(code.substr(begin,stop-begin));
        const auto dot=callee.rfind('.');
        const auto tail=dot==std::string::npos?callee:callee.substr(dot+1);
        const auto root=callee.substr(0,callee.find('.'));
        const auto lead=back_blank(code,begin);
        const bool constructed=lead>=3&&code.substr(lead-3,3)=="new"&&(lead==3||!word(code[lead-4]));
        if(callee=="fetch"||callee.ends_with(".fetch")){out.label="fetch() argument";out.request=true;out.method=method_key(object_after(code,end,true));}
        else if(callee=="axios"){out.label="axios() argument";out.request=true;out.method=method_key(object_after(code,end,true));}
        else if(dot!=std::string::npos&&one_of(tail,{"get","post","put","patch","delete","head","options"})) {
            out.label="."+tail+"() argument";out.method=upper(tail);out.object=callee.substr(0,dot);
            out.writer=one_of(tail,{"post","put","patch"});
            const bool known=one_of(root,{"axios","$","jQuery"});
            const bool client=http_client(out.object),learned=out.object.size()>=2&&clients.contains(out.object);
            out.request=out.writer||known||client||learned;
            if(!out.writer&&!known&&(client||learned))out.label+=client?" on an HTTP client":" on an object that also sends writes";
        }
        else if(fetch_function(callee,tail)){out.label=callee+"() argument";out.request=true;out.method=method_key(object_after(code,end,true));}
        else if(tail=="getJSON"){out.label="getJSON() argument";out.method="GET";out.request=true;}
        else if(tail=="sendBeacon"){out.label="sendBeacon() argument";out.method="POST";out.request=true;}
        else if(constructed&&(tail=="WebSocket"||tail=="EventSource")){out.label="new "+tail+"() argument";out.method="GET";out.request=true;}
        else if(constructed&&tail=="Request"){out.label="new Request() argument";out.request=true;out.method=method_key(object_after(code,end,true));}
    } else if(previous==',') {
        auto i=back_blank(code,before-1);
        if(i<2||(code[i-1]!='"'&&code[i-1]!='\''))return out;
        const char quote=code[i-1];auto open=i-1;
        while(open>0&&i-1-open<8&&code[open-1]!=quote)--open;
        if(!open||code[open-1]!=quote)return out;
        const auto method=upper(std::string(code.substr(open,i-1-open)));
        if(!http_method(method))return out;
        const auto call=back_blank(code,open-1);
        if(call>=6&&code.substr(call-6,6)==".open("){out.label="XMLHttpRequest.open() argument";out.method=method;out.request=true;}
    } else if(previous==':') {
        auto i=back_blank(code,before-1);
        if(i&&(code[i-1]=='"'||code[i-1]=='\''))--i;
        auto begin=i;
        while(begin>0&&i-begin<32&&word(code[begin-1]))--begin;
        const auto key=lower(std::string(code.substr(begin,i-begin)));
        if(one_of(key,{"baseurl","base_url","apiurl","api_url","apibase","apibaseurl","apihost","apiendpoint","api_endpoint","apiroot",
            "endpoint","graphqlurl","graphqlendpoint","graphql_url","wsurl","socketurl"})||endpoint_name(key)) {
            out.label="property "+std::string(code.substr(begin,i-begin));out.named=true;out.endpoint=endpoint_name(key);
        } else if(key=="url"||key=="uri") {
            out.label="property "+key;
            const auto after=object_after(code,end,false);
            const auto open=object_open(code,begin);
            const auto earlier=open==std::string_view::npos?std::string_view{}:code.substr(open+1,begin-open-1);
            out.method=method_key(after);
            if(out.method.empty())out.method=method_key(earlier);
            out.request=!out.method.empty()||request_keys(after)||request_keys(earlier);
        } else if(const auto open=object_open(code,begin);open!=std::string_view::npos) {
            const auto owner=owner_name(code,open);
            if(endpoint_name(owner)){out.label="property "+std::string(code.substr(begin,i-begin))+" of "+owner;out.named=true;out.endpoint=true;}
        }
    } else if(previous=='=') {
        const auto at=before-1;
        if(at>0&&std::string_view("=!<>+-*/%&|^?").find(code[at-1])!=std::string_view::npos)return out;
        const auto i=back_blank(code,at);
        auto begin=i;
        while(begin>0&&i-begin<48&&word(code[begin-1]))--begin;
        const auto name=lower(std::string(code.substr(begin,i-begin)));
        if(endpoint_name(name)||one_of(name,{"baseurl","base_url","apibaseurl","apihost","graphqlurl","graphql_url","wsurl","socketurl"})) {
            out.label="variable "+std::string(code.substr(begin,i-begin));out.named=true;out.endpoint=endpoint_name(name);
        }
    }
    return out;
}

std::optional<ApiReference> classify(const Literal& literal,const Context& context,const std::string& source) {
    auto text=trim(literal.text);
    if(text.size()<2||text.size()>300)return {};
    const auto folded=lower(text.substr(0,8));
    std::string scheme,host,path;bool base=false,relative_scheme=false;
    if(folded.starts_with("http://")||folded.starts_with("https://")||folded.starts_with("ws://")||folded.starts_with("wss://")||text.starts_with("//")) {
        const auto slashes=text.find("//");
        relative_scheme=slashes==0;
        scheme=relative_scheme?"https":folded.substr(0,slashes-1);
        const auto authority=slashes+2;const auto slash=text.find('/',authority);
        host=lower(text.substr(authority,slash==std::string::npos?std::string::npos:slash-authority));
        path=slash==std::string::npos?"/":text.substr(slash);
        if(!valid_host(host)||!code_path(path,true))return {};
    } else if(text[0]=='/') {
        path=text;if(!code_path(path,false))return {};
    } else if(text.starts_with("{}/")) {
        path=text.substr(2);base=true;if(!code_path(path,false))return {};
    } else return {};
    if(asset_path(path))return {};
    const bool marked=path_marker(path);
    if(file_like(path)&&!marked)return {};
    const bool shaped=marked||(!host.empty()&&host_marker(host));
    if(!host.empty()&&content_host(host)&&!context.request)return {};
    if(!shaped) {
        if(relative_scheme&&!context.request)return {};
        const auto parts=segments(path);
        if(host.empty()) {
            if(!context.request&&!(context.named&&context.endpoint))return {};
            if(path.size()<3||parts.empty()||asset_directory(lower(parts[0])))return {};
        } else if(!context.request&&!context.named)return {};
    }
    if(!host.empty()) {
        const auto colon=host.find(':');
        if(colon!=std::string::npos&&std::stoul(host.substr(colon+1))==default_port(scheme))host.resize(colon);
    }
    ApiReference reference;
    reference.kind="code";reference.method=context.method;reference.source=source;
    reference.value=host.empty()?path:scheme+"://"+host+path;
    reference.detail=(context.label.empty()?std::string("String literal"):context.label)+": \""+printable(text,160)+"\"";
    if(base)reference.detail+=" (after a base set at run time)";
    if(relative_scheme)reference.detail+=" (scheme not stated)";
    if(literal.query)reference.detail+=" (query omitted)";
    return reference;
}

bool remember(std::vector<ApiReference>& all,ApiReference reference,size_t& taken,bool& truncated,size_t limit=ApiEndpointList::references_per_source) {
    size_t same=0;
    for(const auto& old:all) {
        if(old.kind!=reference.kind||old.method!=reference.method||old.value!=reference.value)continue;
        if((old.source==reference.source&&old.detail==reference.detail)||++same>=3)return false;
    }
    if(all.size()>=ApiEndpointList::reference_limit||taken>=limit){truncated=true;return false;}
    ++taken;all.push_back(std::move(reference));
    return true;
}
template<class Visit> void each_literal(std::string_view code,Visit&& visit) {
    for(size_t i=0;i<code.size();) {
        const char c=code[i];
        if(c=='\''||c=='"'||c=='`') {
            const auto string=skip_string(code,i);
            if(string.closed&&candidate(code,i,string.end))visit(i,string.end);
            i=std::max(string.end,i+1);
        } else if(c=='/'&&i+1<code.size()&&code[i+1]=='*') {
            const auto end=code.find("*/",i+2);i=end==std::string_view::npos?code.size():end+2;
        } else if(c=='/'&&i+1<code.size()&&code[i+1]=='/') {
            const auto end=code.find('\n',i+2);i=end==std::string_view::npos?code.size():end;
        } else if(c=='/') {
            const auto previous=back_blank(code,i);
            auto keyword=previous;
            while(keyword>0&&previous-keyword<8&&word(code[keyword-1]))--keyword;
            const bool expression=previous==0||std::string_view("=([{,:;!?&|+*-~%^<>").find(code[previous-1])!=std::string_view::npos||
                one_of(code.substr(keyword,previous-keyword),{"return","typeof","case","in","of","void","delete","do","else","yield","await","throw"});
            ++i;
            if(expression) {
                bool character_class=false;
                while(i<code.size()&&code[i]!='\n') {
                    const char next=code[i++];
                    if(next=='\\'){if(i<code.size())++i;continue;}
                    if(next=='[')character_class=true;else if(next==']')character_class=false;
                    else if(next=='/'&&!character_class)break;
                }
            }
        } else ++i;
    }
}
void code_references(std::string_view code,const std::string& source,std::vector<ApiReference>& all,bool& truncated,bool absolute_only=false) {
    code=code.substr(0,kCodeBytes);
    std::set<std::string> clients;
    each_literal(code,[&](size_t start,size_t end) {
        const auto context=context_of(code,start,end,clients);
        if(context.writer&&context.object.size()>=2)clients.insert(context.object);
    });
    size_t taken=0;
    each_literal(code,[&](size_t start,size_t end) {
        const auto literal=literal_text(code,start,end);
        if(literal.valid)if(auto reference=classify(literal,context_of(code,start,end,clients),source))
            if(!absolute_only||reference->value.find("://")!=std::string::npos)remember(all,std::move(*reference),taken,truncated);
    });
}

bool token(const std::string& list,std::string_view wanted) {
    size_t at=0;const auto folded=lower(list);
    while(at<folded.size()) {
        at=skip_blank(folded,at);auto end=at;
        while(end<folded.size()&&!blank(folded[end]))++end;
        if(std::string_view(folded).substr(at,end-at)==wanted)return true;
        at=end;
    }
    return false;
}
std::string header(const HttpObservation& page,std::string_view name) {
    for(const auto& [key,value]:page.headers)if(lower(key)==name)return value;
    return {};
}
bool html_page(const HttpObservation& page) {
    const auto mime=mime_of(header(page,"content-type"));
    return mime=="text/html"||mime=="application/xhtml+xml"||(mime.empty()&&page.body.find('<')!=std::string::npos);
}
void markup_references(const HttpObservation& page,const std::string& prefix,std::vector<ApiReference>& all,bool& truncated) {
    const auto location=clean(page.url);
    if(location.empty())return;
    const auto base=without_query(page.url);size_t taken=0;
    const auto declare=[&](const std::string& value,std::string method,const std::string& detail,bool shaped_only) {
        const auto target=parse_url(trim(value),base);
        if(!target||asset_path(target->path))return;
        if(shaped_only&&!path_marker(target->path)&&!host_marker(target->host))return;
        remember(all,{"markup",std::move(method),origin(*target)+target->path,prefix+location,detail},taken,truncated);
    };
    for(const auto& tag:markup::tags(page.body.substr(0,3*1024*1024))) {
        if(tag.name=="link"&&!tag.attr("href").empty()) {
            const auto rel=tag.attr("rel"),type=lower(trim(tag.attr("type")));
            if(token(rel,"https://api.w.org/"))declare(tag.attr("href"),{},"link rel=\"https://api.w.org/\" (WordPress REST API root)",false);
            else if(token(rel,"edituri"))declare(tag.attr("href"),{},"link rel=\"EditURI\" (XML-RPC discovery document)",false);
            else if(token(rel,"alternate")&&type=="application/json")declare(tag.attr("href"),{},"link rel=\"alternate\" type=\"application/json\"",false);
            else if(token(rel,"alternate")&&type=="application/json+oembed")declare(tag.attr("href"),{},"link rel=\"alternate\" (oEmbed endpoint)",false);
        } else if(tag.name=="form"&&!trim(tag.attr("action")).empty()) {
            const auto method=upper(trim(tag.attr("method")));
            declare(tag.attr("action"),http_method(method)?method:"GET","form[action]",true);
        } else if(tag.name=="script"&&tag.attr("src").empty()&&!tag.body.empty()) {
            const auto type=lower(trim(tag.attr("type")));
            if(markup::javascript(tag)||type=="application/json"||type=="application/ld+json")
                code_references(tag.body,prefix+"Inline script "+location,all,truncated);
        }
        for(const std::string_view verb:{"get","post","put","patch","delete"}) for(const std::string_view lead:{"hx-","data-hx-"}) {
            const auto name=std::string(lead)+std::string(verb);
            const auto value=tag.attr(name.c_str());
            if(!trim(value).empty())declare(value,upper(std::string(verb)),name+" attribute on <"+tag.name+">",false);
        }
    }
    const auto link=header(page,"link");
    for(size_t at=link.find('<');at!=std::string::npos;at=link.find('<',at+1)) {
        const auto close=link.find('>',at);if(close==std::string::npos)break;
        const auto next=link.find('<',close);
        const auto parameters=lower(link.substr(close+1,next==std::string::npos?std::string::npos:next-close-1));
        if(parameters.find("https://api.w.org/")!=std::string::npos)
            declare(link.substr(at+1,close-at-1),{},"Link response header rel=\"https://api.w.org/\" (WordPress REST API root)",false);
        at=close;
    }
}
void robots_references(const HttpObservation& page,std::vector<ApiReference>& all,bool& truncated) {
    const auto location=parse_url(without_query(page.url));
    if(!location)return;
    size_t taken=0,start=0,rules=0;
    std::set<std::string> seen;
    while(start<page.body.size()) {
        const auto end=page.body.find('\n',start);
        auto line=page.body.substr(start,end==std::string::npos?std::string::npos:end-start);
        start=end==std::string::npos?page.body.size():end+1;
        line=trim(line.substr(0,line.find('#')));
        const auto colon=line.find(':');
        if(colon==std::string::npos)continue;
        const auto field=lower(trim(line.substr(0,colon)));
        if(field!="disallow"&&field!="allow")continue;
        auto value=trim(line.substr(colon+1));
        value=value.substr(0,value.find_first_of(" \t?"));
        while(!value.empty()&&(value.back()=='$'||value.back()=='*'))value.pop_back();
        if(value.find('*')!=std::string::npos||value.find('$')!=std::string::npos)continue;
        if(!code_path(value,false)||asset_path(value)||!path_marker(value)||!seen.insert(value).second)continue;
        if(++rules>kRobotsRules){truncated=true;break;}
        const auto rule=trim(line.substr(colon+1));
        remember(all,{"robots",{},origin(*location)+value,origin(*location)+location->path,
            (field=="allow"?"Allow: ":"Disallow: ")+printable(rule.substr(0,rule.find('?')),160)},taken,truncated,kRobotsRules);
    }
}
bool description_path(std::string_view path) {
    if(path.empty()||path[0]!='/'||path.size()>200)return false;
    return std::all_of(path.begin(),path.end(),[](char c){return alnum(c)||std::string_view("._~%:@/{}*+-").find(c)!=std::string_view::npos;});
}
bool description_references(const HttpObservation& page,std::vector<ApiReference>& all,bool& truncated) {
    if(page.body_truncated||page.body.size()>1024*1024)return false;
    const auto document=Json::parse(page.body,nullptr,false);
    if(!document.is_object()||!document.contains("paths")||!document["paths"].is_object())return false;
    std::string kind;
    if(document.contains("openapi")&&document["openapi"].is_string())kind="OpenAPI "+printable(document["openapi"].get<std::string>(),16);
    else if(document.contains("swagger")&&document["swagger"].is_string())kind="Swagger "+printable(document["swagger"].get<std::string>(),16);
    if(kind.empty())return false;
    const auto location=parse_url(without_query(page.url));
    if(!location)return false;
    auto root=origin(*location);std::string prefix;
    const auto text=[&](const Json& object,const char* key) {
        return object.is_object()&&object.contains(key)&&object[key].is_string()?object[key].get<std::string>():std::string{};
    };
    if(document.contains("servers")&&document["servers"].is_array()&&!document["servers"].empty()) {
        const auto server=text(document["servers"][0],"url");
        if(server.find('{')==std::string::npos)if(const auto target=parse_url(server,origin(*location)+location->path)) {
            root=origin(*target);prefix=target->path;
        }
    } else {
        const auto host=text(document,"host"),base=text(document,"basePath");
        if(!host.empty()&&valid_host(host)) {
            std::string scheme=location->scheme;
            if(document.contains("schemes")&&document["schemes"].is_array()&&!document["schemes"].empty()&&document["schemes"][0].is_string())
                scheme=lower(document["schemes"][0].get<std::string>());
            if(const auto target=parse_url(scheme+"://"+host+"/"))root=origin(*target);
        }
        if(description_path(base))prefix=base;
    }
    while(!prefix.empty()&&prefix.back()=='/')prefix.pop_back();
    if(!prefix.empty()&&!description_path(prefix))prefix.clear();
    const auto source="API description / "+origin(*location)+location->path;
    size_t taken=0;
    for(const auto& [path,item]:document["paths"].items()) {
        if(!description_path(path)||!item.is_object())continue;
        for(const auto& [verb,operation]:item.items()) {
            const auto method=upper(verb);
            if(!http_method(method)&&method!="TRACE")continue;
            auto detail=kind+" paths: "+method+" "+path;
            const auto id=text(operation,"operationId");
            if(!id.empty())detail+=" / operationId "+printable(id,80);
            remember(all,{"description",method,root+prefix+path,source,detail},taken,truncated,kDescriptionOperations);
            if(taken>=kDescriptionOperations&&truncated)return true;
        }
    }
    return true;
}
bool graphql_error(const HttpObservation& page) {
    if(page.status!=400&&page.status!=405)return false;
    const auto body=Json::parse(page.body.substr(0,65536),nullptr,false);
    if(!body.is_object()||!body.contains("errors")||!body["errors"].is_array()||body["errors"].empty())return false;
    const auto& first=body["errors"][0];
    return first.is_object()&&first.contains("message")&&first["message"].is_string();
}

std::string scope_of(const ScanReport& report,std::string host) {
    if(host.empty())return "Host not stated";
    if(host.front()=='['&&host.back()==']')host=host.substr(1,host.size()-2);
    Target destination;destination.host=host;
    destination.is_ip=host.find(':')!=std::string::npos||std::all_of(host.begin(),host.end(),[](char c){return digit(c)||c=='.';});
    if(same_website_host(report.target,destination))return "Same host";
    auto base=lower(report.target.host);
    if(base.ends_with('.'))base.pop_back();
    if(base.starts_with("www."))base.erase(0,4);
    if(!report.target.is_ip&&!destination.is_ip&&!base.empty()&&host.size()>base.size()+1&&host.ends_with("."+base))return "Subdomain";
    return "Other host";
}
int basis_rank(const std::string& basis) {
    const char* order[]={"Observed request","Collected response","Request attempt","API description","Page declaration","Code reference","robots.txt"};
    for(int i=0;i<7;++i)if(basis==order[i])return i;
    return 7;
}
int scope_rank(const ApiEndpoint& row) {
    if(row.scope=="Same host"||row.scope=="Subdomain")return 0;
    if(row.scope=="Host not stated")return 1;
    return row.provider.empty()?2:3;
}

struct Builder {
    const ScanReport& report;
    std::vector<ApiEndpoint> rows;
    std::vector<size_t> more;
    std::vector<std::vector<std::string>> parts;
    std::vector<std::optional<Parts>> places;
    std::vector<bool> shaped;
    std::set<std::string> page_urls,page_paths;
    bool truncated=false;
    Parts inspected;

    explicit Builder(const ScanReport& r):report(r) {
        inspected.scheme=r.target.scheme.empty()?"https":r.target.scheme;
        inspected.host=lower(r.target.authority());inspected.port=r.target.port?r.target.port:default_port(inspected.scheme);
    }
    void evidence(size_t index,Evidence item) {
        auto& row=rows[index];
        if(std::any_of(row.evidence.begin(),row.evidence.end(),[&](const auto& e){return e.source==item.source&&e.value==item.value;}))return;
        if(row.evidence.size()>=ApiEndpointList::evidence_limit){++more[index];return;}
        row.evidence.push_back(std::move(item));
    }
    std::string label(const std::string& method,const Parts* target,const std::string& host,const std::string& path,const std::string& scope) const {
        std::string name;
        if(target) {
            const bool web=target->scheme=="https"||target->scheme=="http";
            const bool here=scope=="Same host"&&web&&(target->port==inspected.port||
                (!report.target.explicit_port&&target->port==default_port(target->scheme)));
            const bool plain=target->scheme=="https"||target->scheme==inspected.scheme;
            const auto authority=host+(target->port==default_port(target->scheme)?std::string{}:":"+std::to_string(target->port));
            name=!plain?target->scheme+"://"+authority+path:here?path:authority+path;
        } else name=path;
        std::string padded=method;padded.resize(std::max<size_t>(6,padded.size()),' ');
        return padded+" "+name;
    }
    size_t row(const std::string& method,const Parts* target,const std::string& shown_host,const std::string& path,const std::string& basis) {
        const auto url=target?target->scheme+"://"+shown_host+(target->port==default_port(target->scheme)?std::string{}:":"+std::to_string(target->port))+path:path;
        for(size_t i=0;i<rows.size();++i)if(rows[i].method==method&&rows[i].url==url) {
            if(basis_rank(basis)<basis_rank(rows[i].basis))rows[i].basis=basis;
            return i;
        }
        ApiEndpoint created;created.method=method;created.url=url;created.host=target?shown_host:std::string{};created.path=path;created.basis=basis;
        created.scope=scope_of(report,target?target->host:std::string{});
        if(target&&created.scope=="Other host") {
            ResourceProvider provider;
            if(resource_provider(origin(*target)+target->path,provider))created.provider=provider.name;
        }
        created.label=label(method,target,shown_host,path,created.scope);
        rows.push_back(std::move(created));more.push_back(0);parts.push_back(segments(path));
        places.push_back(target?std::optional<Parts>(*target):std::nullopt);
        shaped.push_back(path_marker(target?target->path:path)||(target&&host_marker(target->host)));
        return rows.size()-1;
    }

    void browser() {
        std::set<std::string> excluded_pages,page_like;
        for(const auto& page:report.browser.pages)if(page.purpose=="disclosure") {
            excluded_pages.insert(page.url);if(!page.final_url.empty())excluded_pages.insert(page.final_url);
        }
        for(const auto& linked:report.linked_pages) {
            if(linked.url.empty())continue;
            excluded_pages.insert(linked.url);
            for(const auto& page:report.browser.pages)if(page.url==linked.url&&!page.final_url.empty())excluded_pages.insert(page.final_url);
        }
        for(const auto& q:report.browser.requests)if(page_mime(mime_of(q.mime))) {
            page_like.insert(q.url);
            if(excluded_pages.contains(q.page))continue;
            if(const auto target=parse_url(q.url)){page_urls.insert(origin(*target)+target->path);page_paths.insert(target->path);}
        }
        for(const auto& q:report.browser.requests) {
            if(excluded_pages.contains(q.page)||q.method=="OPTIONS")continue;
            const bool socket=q.type=="WebSocket";
            const bool programmatic=socket||one_of(q.type,{"XHR","Fetch","EventSource","Ping"});
            const bool loaded=one_of(q.type,{"Other","Prefetch","Document"});
            if(!programmatic&&!loaded)continue;
            const auto mime=mime_of(q.mime);
            if(asset_mime(mime))continue;
            const auto target=parse_url(q.url);
            if(!target)continue;
            const bool marked=path_marker(target->path);
            if(file_like(target->path)&&!marked)continue;
            const bool shaped=marked||host_marker(target->host);
            const bool answered=q.status>0;
            if(loaded&&!(json_mime(mime)&&(shaped||q.type=="Document")))continue;
            if(!socket&&(!answered||q.state=="redirect"||mime.empty())&&page_like.contains(q.url))continue;
            if(!socket&&(!answered||q.state=="redirect")&&q.method=="GET"&&!shaped)continue;
            const auto index=row(q.method.empty()?"GET":q.method,&*target,collapse_host(target->host),collapse_path(target->path),
                answered?"Observed request":"Request attempt");
            if(answered&&(rows[index].status<200||rows[index].status>=300))rows[index].status=q.status;
            std::string value=(q.method.empty()?"GET":q.method)+" "+origin(*target)+target->path+" / "+q.type+" / "+q.state;
            if(answered)value+=" / HTTP "+std::to_string(q.status);else value+=" / no response";
            if(!mime.empty())value+=" / "+printable(mime,80);
            if(!q.error.empty())value+=" / "+printable(q.error,160);
            const auto from=clean(q.page);
            evidence(index,{"Browser request / "+(from.empty()?printable(without_query(q.page),1024):from),value});
        }
    }
    void reference(const ApiReference& item,const std::string& basis) {
        const auto absolute=item.value.find("://")!=std::string::npos;
        std::optional<Parts> target;
        std::string path=item.value;
        if(absolute) {
            const auto slash=item.value.find('/',item.value.find("://")+3);
            path=slash==std::string::npos?"/":item.value.substr(slash);
            target=parse_url(item.value.substr(0,slash)+"/");
            if(!target)return;
            target->path=path;
        }
        if(basis=="Code reference"&&(target?page_urls.contains(origin(*target)+path):page_paths.contains(path)))return;
        const Evidence proof{item.source,item.detail};
        const auto wanted=segments(path);
        bool merged=false;
        for(size_t i=0;i<rows.size();++i) {
            const auto& old=rows[i];
            if(!item.method.empty()&&!old.method.empty()&&old.method!=item.method)continue;
            if(!same_path(parts[i],wanted))continue;
            if(target) {
                const auto& existing=places[i];
                if(!existing||existing->host!=target->host||existing->port!=target->port)continue;
                if(existing->scheme!=target->scheme&&basis!="Code reference")continue;
            } else if(old.host.empty()&&old.method!=item.method&&!item.method.empty())continue;
            evidence(i,proof);merged=true;
        }
        if(merged)return;
        if(rows.size()>=ApiEndpointList::row_limit){truncated=true;return;}
        const auto index=row(item.method,target?&*target:nullptr,target?target->host:std::string{},path,basis);
        evidence(index,proof);
    }
    void collected() {
        std::vector<ApiReference> described,declared,coded,listed;
        for(const auto& page:report.web) {
            if(page.status<=0)continue;
            if(std::any_of(report.linked_pages.begin(),report.linked_pages.end(),[&](const LinkedPage& linked){return linked.url==page.url;}))continue;
            const auto location=parse_url(without_query(page.url));
            if(!location)continue;
            const auto mime=mime_of(header(page,"content-type"));
            const bool success=page.status>=200&&page.status<300;
            const bool graphql=lower(location->path).ends_with("/graphql")&&json_mime(mime)&&graphql_error(page);
            if((success&&json_mime(mime))||graphql) {
                const bool description=success&&description_references(page,described,truncated);
                const auto index=row("GET",&*location,location->host,location->path,"Collected response");
                rows[index].status=page.status;
                evidence(index,{"Static read / "+origin(*location)+location->path,"GET "+origin(*location)+location->path+" / HTTP "+std::to_string(page.status)+
                    " / "+printable(mime,80)+" / "+std::to_string(page.body.size())+" body bytes"+
                    (description?" / API description document":graphql?" / GraphQL-style JSON error body":"")});
            } else if(success&&lower(location->path).ends_with("/robots.txt")&&!html_page(page))robots_references(page,listed,truncated);
            else if(page.status<400&&html_page(page))markup_references(page,{},declared,truncated);
        }
        for(const auto& item:described)reference(item,"API description");
        for(const auto& item:report.api_references)if(item.kind=="description")reference(item,"API description");
        for(const auto& item:declared)if(item.kind=="markup")reference(item,"Page declaration");
        for(const auto& item:report.api_references)if(item.kind=="markup")reference(item,"Page declaration");
        for(const auto& item:declared)if(item.kind=="code")coded.push_back(item);
        for(const auto& item:report.api_references)if(item.kind=="code")coded.push_back(item);
        std::stable_sort(coded.begin(),coded.end(),[](const auto& a,const auto& b) {
            const bool left=a.value.find("://")!=std::string::npos,right=b.value.find("://")!=std::string::npos;
            if(left!=right)return left;
            return !a.method.empty()&&b.method.empty();
        });
        for(const auto& item:coded)reference(item,"Code reference");
        for(const auto& item:listed)reference(item,"robots.txt");
        for(const auto& item:report.api_references)if(item.kind=="robots")reference(item,"robots.txt");
    }
    ApiEndpointList finish() {
        std::vector<bool> drop(rows.size(),false);
        for(size_t i=0;i<rows.size();++i) {
            if(!rows[i].host.empty()||rows[i].basis!="Code reference")continue;
            for(size_t k=0;k<rows.size()&&!drop[i];++k)
                if(k!=i&&!drop[k]&&path_prefix(parts[i],parts[k]))drop[i]=true;
        }
        ApiEndpointList list;list.truncated=truncated||report.api_references_truncated;
        std::vector<size_t> order;
        for(size_t i=0;i<rows.size();++i) {
            if(drop[i])continue;
            if(more[i])rows[i].evidence.push_back({"More evidence",std::to_string(more[i])+" further matching "+(more[i]==1?"observation":"observations")+" not listed"});
            order.push_back(i);
        }
        std::stable_sort(order.begin(),order.end(),[&](size_t left,size_t right) {
            const auto& a=rows[left];const auto& b=rows[right];
            if(scope_rank(a)!=scope_rank(b))return scope_rank(a)<scope_rank(b);
            if(basis_rank(a.basis)!=basis_rank(b.basis))return basis_rank(a.basis)<basis_rank(b.basis);
            if(shaped[left]!=shaped[right])return static_cast<bool>(shaped[left]);
            if((a.scope=="Same host")!=(b.scope=="Same host"))return a.scope=="Same host";
            if(a.host!=b.host)return a.host<b.host;
            if(a.path!=b.path)return a.path<b.path;
            return a.method<b.method;
        });
        if(order.size()>ApiEndpointList::row_limit){order.resize(ApiEndpointList::row_limit);list.truncated=true;}
        const bool methods=std::any_of(order.begin(),order.end(),[&](size_t i){return !rows[i].method.empty();});
        for(const auto i:order) {
            if(!methods&&rows[i].label.size()>7)rows[i].label.erase(0,7);
            list.rows.push_back(std::move(rows[i]));
        }
        return list;
    }
};
}

void collect_api_references(const std::string& script_url,const std::string& code,ScanReport& report,const std::string& label) {
    const auto location=clean(script_url);
    const auto script=parse_url(without_query(script_url));
    ResourceProvider provider;
    const bool provider_script=script&&scope_of(report,script->host)=="Other host"&&resource_provider(location,provider)&&
        !one_of(provider.category,{"Content delivery","Package CDN","Object storage","Edge / CDN","CMS","Commerce platform"});
    code_references(code,location.empty()?label:label+" "+location,report.api_references,report.api_references_truncated,provider_script);
}
void collect_api_references(const HttpObservation& page,ScanReport& report,const std::string& source_prefix) {
    if(page.status<200||page.status>=400||!html_page(page))return;
    markup_references(page,source_prefix,report.api_references,report.api_references_truncated);
}
ApiEndpointList api_endpoint_list(const ScanReport& report) {
    Builder builder(report);
    builder.browser();
    builder.collected();
    return builder.finish();
}
std::vector<ApiEndpoint> api_endpoints(const ScanReport& report) {
    return api_endpoint_list(report).rows;
}
std::string api_endpoint_tag(const ApiEndpoint& endpoint) {
    if(endpoint.basis=="Observed request")return "Observed";
    if(endpoint.basis=="Collected response")return "Answered";
    if(endpoint.basis=="Request attempt")return "Attempted";
    if(endpoint.basis=="API description")return "Described";
    if(endpoint.basis=="Page declaration")return "Declared";
    if(endpoint.basis=="Code reference")return "In code";
    return endpoint.basis;
}
std::string api_endpoint_meaning(const ApiEndpoint& endpoint) {
    if(endpoint.basis=="Observed request")return "The page's own code made this request in Vector's isolated browser and a response arrived. Vector did not call the endpoint itself. Query values, headers and bodies are not kept.";
    if(endpoint.basis=="Collected response")return "Vector's own read of this URL was answered with JSON. Nothing else was sent to it.";
    if(endpoint.basis=="Request attempt")return "The page's code tried this request and no response was observed. The endpoint is not confirmed to exist or answer.";
    if(endpoint.basis=="API description")return "Listed in an API description document this site serves. The listed operation was not requested.";
    if(endpoint.basis=="Page declaration")return "Named by the page's markup or response headers. It was not requested by this inspection.";
    if(endpoint.basis=="Code reference")return "Named in collected script text. It can be inactive code or a bundled library's own endpoint, and it was not requested.";
    return "Path named in the site's robots.txt. It was not requested by this inspection.";
}
}
