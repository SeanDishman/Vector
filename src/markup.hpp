#pragma once
#include "vector/model.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <string_view>

namespace vectorapp::markup {
struct Tag {
    std::string name, opening, body;
    std::map<std::string,std::string> attributes;
    std::string attr(const char* key) const {
        const auto it=attributes.find(key);return it==attributes.end()?"":it->second;
    }
};
inline bool space(char c) { return std::isspace(static_cast<unsigned char>(c))!=0; }
inline size_t find_ci(const std::string& text,std::string_view needle,size_t from) {
    for(size_t i=from;i+needle.size()<=text.size();++i) {
        size_t n=0;while(n<needle.size() && std::tolower(static_cast<unsigned char>(text[i+n]))==needle[n])++n;
        if(n==needle.size())return i;
    }
    return std::string::npos;
}
inline std::string entities(std::string value) {
    struct Entity {const char* encoded;const char* decoded;};
    for(const auto& e: {Entity{"&quot;","\""},Entity{"&#39;","'"},Entity{"&#x27;","'"},Entity{"&#X27;","'"},Entity{"&apos;","'"},Entity{"&lt;","<"},Entity{"&gt;",">"},Entity{"&amp;","&"}}) {
        size_t at=0;while((at=value.find(e.encoded,at))!=std::string::npos){value.replace(at,std::char_traits<char>::length(e.encoded),e.decoded);++at;}
    }
    return value;
}
inline bool javascript(const Tag& tag) {
    const auto type=lower(trim(tag.attr("type")));
    return type.empty()||type=="module"||type=="text/javascript"||type=="application/javascript"||type=="text/ecmascript"||type=="application/ecmascript";
}


inline std::vector<Tag> tags(const std::string& html) {
    std::vector<Tag> result;size_t pos=0;int inert=0;
    while(pos<html.size()) {
        const auto start=html.find('<',pos);if(start==std::string::npos)break;
        if(html.compare(start,4,"<!--")==0){auto end=html.find("-->",start+4);pos=end==std::string::npos?html.size():end+3;continue;}
        size_t i=start+1;bool closing=false;if(i<html.size()&&html[i]=='/'){closing=true;++i;}
        const auto name_start=i;
        while(i<html.size()&&(std::isalnum(static_cast<unsigned char>(html[i]))||html[i]=='-'||html[i]==':'))++i;
        if(i==name_start){pos=i+1;continue;}
        Tag tag;tag.name=lower(html.substr(name_start,i-name_start));bool complete=false;
        while(i<html.size()) {
            while(i<html.size()&&space(html[i]))++i;
            if(i==html.size())break;
            if(html[i]=='>'){++i;complete=true;break;}
            if(html[i]=='/'){++i;continue;}
            const auto a=i;while(i<html.size()&&!space(html[i])&&html[i]!='='&&html[i]!='>'&&html[i]!='/')++i;
            if(a==i){++i;continue;}
            const auto key=lower(html.substr(a,i-a));while(i<html.size()&&space(html[i]))++i;
            std::string value;
            if(i<html.size()&&html[i]=='=') {
                ++i;while(i<html.size()&&space(html[i]))++i;
                if(i<html.size()&&(html[i]=='\''||html[i]=='"')) {
                    const char quote=html[i++];const auto v=i;while(i<html.size()&&html[i]!=quote)++i;
                    value=html.substr(v,i-v);if(i<html.size())++i;
                } else {const auto v=i;while(i<html.size()&&!space(html[i])&&html[i]!='>')++i;value=html.substr(v,i-v);}
            }
            if(key.size()<80&&value.size()<=4096&&tag.attributes.size()<64)tag.attributes.emplace(key,entities(value));
        }
        pos=i;if(!complete)break;
        if(tag.name=="template"||tag.name=="noscript") {inert=std::max(0,inert+(closing?-1:1));continue;}
        if(closing)continue;
        if(tag.name=="plaintext")break;
        tag.opening=html.substr(start,i-start);
        if(tag.name=="script"||tag.name=="style"||tag.name=="textarea"||tag.name=="title"||
           tag.name=="xmp"||tag.name=="iframe"||tag.name=="noembed"||tag.name=="noframes") {
            auto end=find_ci(html,"</"+tag.name,pos);
            while(end!=std::string::npos) {
                const auto after=end+2+tag.name.size();
                if(after==html.size()||space(html[after])||html[after]=='>'||html[after]=='/')break;
                end=find_ci(html,"</"+tag.name,after);
            }
            if(tag.name=="script")tag.body=html.substr(pos,(end==std::string::npos?html.size():end)-pos);
            else if(tag.name=="style")tag.body=html.substr(pos,std::min<size_t>((end==std::string::npos?html.size():end)-pos,512*1024));
            pos=end==std::string::npos?html.size():end;
        }
        if(!inert)result.push_back(std::move(tag));
    }
    return result;
}
}
