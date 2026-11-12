#pragma once
#include "vector/model.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace vectorapp {
struct ResourceProvider {std::string name,category;};
bool resource_provider(const std::string& url,ResourceProvider& provider);
}

namespace vectorapp::tag_config {
struct Destination {const char* name=nullptr;const char* category=nullptr;};

inline Destination destination(std::string_view id) {
    const auto tail=[&](size_t prefix,size_t low,size_t high,bool letters) {
        if(id.size()<prefix+low||id.size()>prefix+high)return false;
        return std::all_of(id.begin()+static_cast<std::ptrdiff_t>(prefix),id.end(),[&](unsigned char c){
            return std::isdigit(c)||(letters&&std::isupper(c));});
    };
    if(id.starts_with("G-")&&tail(2,6,14,true)&&std::any_of(id.begin(),id.end(),[](unsigned char c){return std::isdigit(c);}))
        return {"Google Analytics","Analytics"};
    if(id.starts_with("AW-")&&tail(3,6,14,false))return {"Google Ads","Advertising"};
    if(id.starts_with("DC-")&&tail(3,5,14,false))return {"DoubleClick Floodlight","Advertising measurement"};
    return {};
}

inline bool blank(char c){return c==' '||c=='\t'||c=='\r'||c=='\n';}
inline bool word(char c){return std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='$';}

inline void skip_blank(const std::string& body,size_t& i){while(i<body.size()&&blank(body[i]))++i;}
inline bool quoted(const std::string& body,size_t& i,std::string& value) {
    if(i>=body.size()||(body[i]!='\''&&body[i]!='"'&&body[i]!='`'))return false;
    const char quote=body[i++];const auto end=body.find(quote,i);
    if(end==std::string::npos||end-i>48)return false;
    value=body.substr(i,end-i);i=end+1;return true;
}
inline bool in_code(const std::string& body,const std::vector<unsigned char>& region,std::string_view text) {
    for(size_t from=0,count=0;count<16;++count) {
        const auto hit=body.find(text,from);if(hit==std::string::npos)return false;
        from=hit+1;if(hit<region.size()&&region[hit]==0)return true;
    }
    return false;
}

inline void configured_calls(const std::string& source,const std::string& body,const std::vector<unsigned char>& region,std::vector<Finding>& out) {
    static const std::string_view queue_text="dataLayer.push(arguments)";
    const bool queue=in_code(body,region,queue_text);
    for(size_t from=0,count=0;count<64;++count) {
        const auto hit=body.find("gtag(",from);if(hit==std::string::npos)break;
        from=hit+1;
        if(hit>=region.size()||region[hit]!=0)continue;
        size_t start=hit;
        if(hit&&word(body[hit-1]))continue;
        if(hit&&body[hit-1]=='.') {
            for(const std::string_view owner:{"window.","globalThis."})
                if(hit>=owner.size()&&body.compare(hit-owner.size(),owner.size(),owner)==0&&
                   (hit==owner.size()||(!word(body[hit-owner.size()-1])&&body[hit-owner.size()-1]!='.')))start=hit-owner.size();
            if(start==hit)continue;
        }
        if(start==hit&&!queue)continue;
        size_t i=hit+5;std::string verb,id;
        skip_blank(body,i);if(!quoted(body,i,verb)||(verb!="config"&&verb!="get"))continue;
        skip_blank(body,i);if(i>=body.size()||body[i]!=',')continue;
        ++i;skip_blank(body,i);if(!quoted(body,i,id))continue;
        const auto found=destination(id);if(!found.name)continue;
        Finding finding{found.name,found.category,"","Medium",{{source,body.substr(start,i-start)}}};
        if(start==hit)finding.evidence.push_back({source,std::string(queue_text)});
        add_finding(out,std::move(finding));
    }
}

inline void partner_id(const std::string& source,const std::string& body,const std::vector<unsigned char>& region,std::vector<Finding>& out) {
    static const std::string_view key="_linkedin_partner_id",list="_linkedin_data_partner_ids";
    if(!in_code(body,region,list))return;
    for(size_t from=0,count=0;count<32;++count) {
        const auto hit=body.find(key,from);if(hit==std::string::npos)break;
        from=hit+1;
        size_t i=hit+key.size();
        if(hit>=region.size()||region[hit]!=0||(hit&&word(body[hit-1]))||(i<body.size()&&word(body[i])))continue;
        skip_blank(body,i);if(i>=body.size()||(body[i]!='='&&body[i]!=':'))continue;
        ++i;skip_blank(body,i);std::string id;if(!quoted(body,i,id))continue;
        if(id.size()<4||id.size()>12||!std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isdigit(c);}))continue;
        add_finding(out,{"LinkedIn Insight Tag","Advertising","","Medium",{{source,body.substr(hit,i-hit)},{source,std::string(list)}}});
        return;
    }
}

inline void configured_ids(const std::string& source,const std::string& body,const std::vector<unsigned char>& region,std::vector<Finding>& out) {
    static const std::string_view names[]={"measurementid","googleanalytics","gatrackingid","ga4id","gtagid","googleads","adwordsid"};
    for(const std::string_view prefix:{"G-","AW-"}) {
        int emitted=0;
        for(size_t from=0,count=0;count<128&&emitted<2;++count) {
            const auto hit=body.find(prefix,from);if(hit==std::string::npos)break;
            from=hit+1;
            if(hit<2||hit>=region.size()||region[hit]==2)continue;
            const char quote=body[hit-1];
            if((quote!='"'&&quote!='\''&&quote!='`')||body[hit-2]=='\\')continue;
            const auto end=body.find(quote,hit);if(end==std::string::npos||end-hit>20)continue;
            const auto found=destination(std::string_view(body).substr(hit,end-hit));if(!found.name)continue;
            size_t i=hit-1;
            while(i&&blank(body[i-1]))--i;
            if(!i||(body[i-1]!=':'&&body[i-1]!='='))continue;
            --i;while(i&&blank(body[i-1]))--i;
            if(i&&(body[i-1]=='"'||body[i-1]=='\''))--i;
            const auto key_end=i;
            while(i&&word(body[i-1])&&key_end-i<60)--i;
            std::string key;
            for(size_t at=i;at<key_end;++at)if(body[at]!='_')key+=static_cast<char>(std::tolower(static_cast<unsigned char>(body[at])));
            if(std::none_of(std::begin(names),std::end(names),[&](std::string_view name){return key.find(name)!=std::string::npos;}))continue;
            add_finding(out,{found.name,found.category,"","Medium",{{source,"configuration value "+body.substr(i,end+1-i)}}});
            ++emitted;
        }
    }
}

inline void container_urls(const std::string& source,std::string_view entry,std::vector<Finding>& out) {
    std::string text;text.reserve(entry.size());
    for(size_t i=0;i<entry.size();++i) {
        if(entry[i]=='\\'&&i+1<entry.size()&&entry[i+1]=='/')continue;
        text+=entry[i];
    }
    static const std::string_view stops="\"'\\<>() ,;\t\r\n";
    for(size_t from=0,count=0;count<64;++count) {
        const auto hit=text.find("//",from);if(hit==std::string::npos)break;
        from=hit+2;
        const bool scheme=(hit>=6&&text.compare(hit-6,6,"https:")==0)||(hit>=5&&text.compare(hit-5,5,"http:")==0);
        const bool relative=hit&&(text[hit-1]=='"'||text[hit-1]=='\'');
        if(!scheme&&!relative)continue;
        auto end=text.find_first_of(stops,hit+2);if(end==std::string::npos)end=text.size();
        if(end-hit<6||end-hit>300)continue;
        auto address="https:"+text.substr(hit,end-hit);
        address=address.substr(0,address.find_first_of("?#;"));
        ResourceProvider provider;
        if(!resource_provider(address,provider))continue;
        if(provider.category=="Edge / CDN"||provider.category=="Object storage"||provider.category=="Package CDN"||
           provider.category=="Content delivery"||provider.category=="Fonts")continue;
        add_finding(out,{provider.name,provider.category,"","Medium",
            {{source,"Google tag container custom tag -> "+printable(address,300)+" (configured; firing not observed)"}}});
    }
}

inline void container(const std::string& source,const std::string& body,const std::vector<unsigned char>& region,std::vector<Finding>& out) {
    const auto back=[&](size_t& i){while(i&&blank(body[i-1]))--i;};
    size_t macros=std::string::npos,end=0;
    for(size_t from=0,count=0;count<8&&macros==std::string::npos;++count) {
        const auto start=body.find("\"resource\"",from);if(start==std::string::npos)return;
        from=start+1;
        size_t i=start;back(i);if(!i||body[i-1]!='{')continue;
        --i;back(i);if(!i||body[i-1]!='=')continue;
        --i;back(i);if(i<4||body.compare(i-4,4,"data")!=0)continue;
        i-=4;const auto name=i;back(i);
        if(i==name||i<3||body.compare(i-3,3,"var")!=0||(i>3&&word(body[i-4])))continue;
        if(i-3>=region.size()||region[i-3]!=0)continue;
        const auto version=body.find("\"version\"",start),first=body.find("\"macros\"",start);
        if(version==std::string::npos||first==std::string::npos||version>first||first-start>160)continue;
        const auto tags=body.find("\"tags\"",first);if(tags==std::string::npos)continue;
        end=body.find("\"predicates\"",tags);if(end==std::string::npos)end=body.size();
        end=std::min(end,first+1536*1024);macros=first;
    }
    if(macros==std::string::npos)return;
    const std::string_view scope(body.data()+macros,end-macros);
    for(const std::string_view prefix:{"\":\"G-","\":\"AW-","\":\"DC-"}) {
        int emitted=0;
        for(size_t from=0,count=0;count<256&&emitted<2;++count) {
            const auto hit=scope.find(prefix,from);if(hit==std::string_view::npos)break;
            from=hit+1;
            const auto value_end=scope.find('"',hit+3);
            if(value_end==std::string_view::npos||value_end-hit>28)continue;
            const auto key_start=scope.rfind('"',hit?hit-1:0);
            if(key_start==std::string_view::npos||hit-key_start>48)continue;
            const auto key=scope.substr(key_start+1,hit-key_start-1),value=scope.substr(hit+3,value_end-hit-3);
            if(!key.starts_with("vtp_")||!std::all_of(key.begin(),key.end(),word))continue;
            const auto found=destination(value);if(!found.name)continue;
            add_finding(out,{found.name,found.category,"","Medium",
                {{source,"Google tag container \""+std::string(key)+"\":\""+std::string(value)+"\" (configured destination)"}}});
            ++emitted;
        }
    }
    struct Builtin {std::string_view type;const char* name;const char* category;};
    static const Builtin builtins[]={
        {"__gaawe","Google Analytics","Analytics"},{"__gaawc","Google Analytics","Analytics"},{"__ua","Google Analytics","Analytics"},
        {"__awct","Google Ads","Advertising"},{"__sp","Google Ads","Advertising"},
        {"__flc","DoubleClick Floodlight","Advertising measurement"},{"__fls","DoubleClick Floodlight","Advertising measurement"},
        {"__twitter_website_tag","X Ads","Advertising"}};
    static const std::string_view marker="\"function\":\"__";
    for(size_t from=0,count=0,custom=0;count<2048;++count) {
        const auto hit=scope.find(marker,from);if(hit==std::string_view::npos)break;
        from=hit+1;
        const auto type_start=hit+marker.size()-2,type_end=scope.find('"',type_start);
        if(type_end==std::string_view::npos||type_end-type_start>48)continue;
        const auto type=scope.substr(type_start,type_end-type_start);
        for(const auto& builtin:builtins)if(type==builtin.type)
            add_finding(out,{builtin.name,builtin.category,"","Medium",
                {{source,"Google tag container \"function\":\""+std::string(type)+"\" (configured tag; firing not observed)"}}});
        if((type=="__html"||type=="__img")&&custom++<256) {
            auto next=scope.find(marker,type_end);if(next==std::string_view::npos)next=scope.size();
            container_urls(source,scope.substr(type_end,std::min<size_t>(next-type_end,32768)),out);
        }
    }
}

inline void identify(const std::string& source,const std::string& body,const std::vector<unsigned char>& region,std::vector<Finding>& out) {
    if(body.find("gtag(")!=std::string::npos)configured_calls(source,body,region,out);
    if(body.find("_linkedin_partner_id")!=std::string::npos)partner_id(source,body,region,out);
    if(body.find("\"resource\"")!=std::string::npos)container(source,body,region,out);
    configured_ids(source,body,region,out);
}
}
