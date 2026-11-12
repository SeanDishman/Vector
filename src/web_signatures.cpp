#include "web_signatures.hpp"
#include "web_signatures_data.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace vectorapp {
namespace {
using Json=nlohmann::json;
std::string text(const Json& v,const char* key) {const auto i=v.find(key);return i!=v.end()&&i->is_string()?i->get<std::string>():"";}
bool identifier(const std::string& s) {
    if(s.empty()||s.size()>120)return false;
    bool first=true;
    for(const unsigned char c:s) {if(c=='.'){if(first)return false;first=true;continue;}
        if(!(std::isalpha(c)||c=='_'||c=='$'||(!first&&std::isdigit(c))))return false;
        first=false;}
    return !first;
}
bool version(const std::string& s) {
    if(s.empty()||s.size()>80||!std::isdigit(static_cast<unsigned char>(s[0])))return false;
    size_t i=0;int parts=0;
    while(i<s.size()) {const auto begin=i;while(i<s.size()&&std::isdigit(static_cast<unsigned char>(s[i])))++i;if(i==begin)return false;++parts;
        if(i==s.size())return parts>=2;
        if(s[i]=='.'){++i;continue;}
        if((s[i]=='-'||s[i]=='+')&&parts>=2) {++i;if(i==s.size())return false;for(;i<s.size();++i)if(!std::isalnum(static_cast<unsigned char>(s[i]))&&s[i]!='.'&&s[i]!='-'&&s[i]!='+')return false;return true;}
        return false;
    }return false;
}
bool hex(const std::string& s,size_t count) {return s.size()==count&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isxdigit(c)!=0;});}
std::string safe_source(std::string value) {auto at=value.find_first_of("?#");if(at!=std::string::npos)value.resize(at);return printable(value,2048);}
void require(bool ok) {if(!ok)throw std::runtime_error("Invalid web signature definition");}
}

WebSignatures::WebSignatures(Json definition):rules_(std::move(definition)) {
    require(rules_.is_object()&&rules_.value("schema_version",0)==1&&!text(rules_,"revision").empty()&&text(rules_,"revision").size()<=80);
    require(rules_.contains("rules")&&rules_["rules"].is_array()&&rules_["rules"].size()<=64);
    std::set<std::string> ids;
    for(const auto& r:rules_["rules"]) {
        require(r.is_object()&&!text(r,"id").empty()&&text(r,"id").size()<=80&&ids.insert(text(r,"id")).second);
        require(!text(r,"name").empty()&&text(r,"name").size()<=80);
        require(text(r,"category")=="Library"||text(r,"category")=="Framework"||text(r,"category")=="CMS"||text(r,"category")=="Bot protection"||
                text(r,"category")=="Fraud prevention");
        const auto kind=text(r,"source");
        if(kind=="runtime") {
            require(identifier(text(r,"version"))&&r.contains("requires")&&r["requires"].is_object()&&r["requires"].size()>=2&&r["requires"].size()<=6);
            for(const auto& [key,type]:r["requires"].items())require(identifier(key)&&type.is_string()&&(type=="function"||type=="object"));
        } else if(kind=="dom") {
            require(!text(r,"selector").empty()&&text(r,"selector").size()<=120&&r.contains("attributes")&&r["attributes"].is_object()&&r["attributes"].size()>=1&&r["attributes"].size()<=4);
            for(const auto& [key,type]:r["attributes"].items())require(!key.empty()&&key.size()<=80&&(type=="present"||type=="version"||type=="hex24"));
        } else if(kind=="cookies") {
            require(r.contains("names")&&r["names"].is_array()&&r["names"].size()>=1&&r["names"].size()<=4);
            for(const auto& name:r["names"])require(name.is_string()&&!name.get<std::string>().empty()&&name.get<std::string>().size()<=80);
            require(r["names"].size()>=2||(!text(r,"hex_suffix").empty()&&text(r,"hex_suffix").size()<=80));
        } else if(kind=="script_banner")require(!text(r,"prefix").empty()&&text(r,"prefix").size()<=80&&!text(r,"corroboration").empty()&&text(r,"corroboration").size()<=160);
        else require(false);
    }
}
void WebSignatures::emit(const Json& rule,const std::string& v,const std::string& source,std::vector<std::string> values,std::vector<Finding>& out) const {
    Finding found{text(rule,"name"),text(rule,"category"),v,text(rule,"source")=="runtime"||text(rule,"source")=="script_banner"?"High":"Medium",{}};
    const auto origin=safe_source(source)+" | rule "+text(rule,"id")+" @ "+text(rules_,"revision");
    for(const auto& value:values)found.evidence.push_back({origin,printable(value,300)});
    add_finding(out,std::move(found));
}
std::string WebSignatures::runtime_expression() const {
    Json paths=Json::array(),dom=Json::array();std::set<std::string> seen;
    for(const auto& r:rules_["rules"]) {
        if(text(r,"source")=="runtime") {
            const auto v=text(r,"version");if(seen.insert(v).second)paths.push_back(v);
            for(const auto& [p,t]:r["requires"].items()){(void)t;if(seen.insert(p).second)paths.push_back(p);}
        } else if(text(r,"source")=="dom")dom.push_back({{"id",r["id"]},{"selector",r["selector"]},{"attributes",r["attributes"]}});
    }
    return "(() => {const paths="+paths.dump()+",rules="+dom.dump()+R"JS(;
      const result={properties:{},dom:[],limited:false};
      const own=(o,k)=>{try{if(o===null||(typeof o!=='object'&&typeof o!=='function'))return undefined;const d=Object.getOwnPropertyDescriptor(o,k);return d&&'value' in d?d.value:undefined;}catch{return undefined;}};
      for(const path of paths){let v=globalThis;for(const key of path.split('.'))v=own(v,key);const type=v===null?'null':typeof v;
        result.properties[path]={type};if((type==='string'&&v.length<=80)||type==='number')result.properties[path].value=String(v);}
      for(const rule of rules){try{const nodes=document.querySelectorAll(rule.selector);let count=0;
        for(const el of nodes){if(++count>64){result.limited=true;break;}if(el.closest('template,noscript,pre,code,textarea'))continue;
          const attributes={};let ok=true;for(const [key,kind] of Object.entries(rule.attributes)){
            if(!el.hasAttribute(key)){ok=false;break;}if(kind==='present')attributes[key]=true;
            else{const v=el.getAttribute(key);if(v.length>80){ok=false;break;}attributes[key]=v;}}
          if(ok)result.dom.push({rule:rule.id,tag:el.localName,attributes});
        }}catch{result.limited=true;}}
      return result;
    })())JS";
}
void WebSignatures::runtime(const Json& observed,const std::string& source,std::vector<Finding>& out) const {
    if(!observed.is_object())return;
    const auto properties=observed.find("properties"),dom=observed.find("dom");
    for(const auto& r:rules_["rules"]) {
        if(text(r,"source")=="runtime"&&properties!=observed.end()&&properties->is_object()) {
            const auto value=properties->find(text(r,"version"));if(value==properties->end()||text(*value,"type")!="string"||!version(text(*value,"value")))continue;
            bool ok=true;std::vector<std::string> evidence{text(r,"version")+" = "+text(*value,"value")};
            for(const auto& [key,type]:r["requires"].items()) {const auto at=properties->find(key);if(at==properties->end()||text(*at,"type")!=type.get<std::string>()){ok=false;break;}evidence.push_back(key+" : "+type.get<std::string>());}
            if(ok)emit(r,text(*value,"value"),source,std::move(evidence),out);
        } else if(text(r,"source")=="dom"&&dom!=observed.end()&&dom->is_array()) {
            size_t count=0;for(const auto& item:*dom) {
                if(++count>256)break;
                if(text(item,"rule")!=text(r,"id")||!item.contains("attributes")||!item["attributes"].is_object())continue;
                bool ok=true;std::string v;std::vector<std::string> evidence;
                for(const auto& [key,kind]:r["attributes"].items()) {
                    const auto at=item["attributes"].find(key);if(at==item["attributes"].end()){ok=false;break;}
                    if(kind=="present") {if(!at->is_boolean()||!at->get<bool>()){ok=false;break;}evidence.push_back("<"+text(item,"tag")+"> attribute "+key+" present");}
                    else {if(!at->is_string()){ok=false;break;}const auto s=at->get<std::string>();if(kind=="version"?!version(s):!hex(s,24)){ok=false;break;}if(kind=="version")v=s;evidence.push_back(key+" = "+s);}
                }if(ok){emit(r,v,source+" | DOM",std::move(evidence),out);break;}
            }
        }
    }
}
void WebSignatures::cookies(const HttpObservation& page,std::vector<Finding>& out) const {
    if(!page.error.empty()||page.status<200||page.status>=400)return;
    std::set<std::string> names;
    for(const auto& [key,value]:page.headers)if(lower(key)=="set-cookie") {
        const auto equal=value.find('=');if(equal!=std::string::npos&&equal<=112){auto name=trim(value.substr(0,equal));if(!name.empty())names.insert(name);}
    }
    for(const auto& r:rules_["rules"])if(text(r,"source")=="cookies") {
        bool ok=true;std::vector<std::string> evidence;
        for(const auto& name:r["names"]) {if(!names.contains(name.get<std::string>())){ok=false;break;}evidence.push_back("Set-Cookie name: "+name.get<std::string>());}
        const auto prefix=text(r,"hex_suffix");if(!prefix.empty()) {auto it=std::find_if(names.begin(),names.end(),[&](const auto& n){return n.starts_with(prefix)&&hex(n.substr(prefix.size()),32);});if(it==names.end())ok=false;else evidence.push_back("Set-Cookie name: "+*it);}
        if(ok)emit(r,"",page.url,evidence,out);
    }
}
void WebSignatures::javascript(const std::string& source,const std::string& body,std::span<const unsigned char> regions,std::vector<Finding>& out) const {
    if(body.size()!=regions.size())return;
    size_t pos=0;
    while(pos<body.size()) {
        if(regions[pos]==2&&body.compare(pos,2,"//")==0) {const auto end=body.find('\n',pos+2);if(end==std::string::npos)break;pos=end+1;continue;}
        if(regions[pos]!=2||body.compare(pos,2,"/*")!=0){++pos;continue;}
        const auto end=body.find("*/",pos+2);if(end==std::string::npos)break;
        if(end-pos>8192){pos=end+2;continue;}
        std::string banner=body.substr(pos+2,end-pos-2);pos=end+2;
        const auto strip=[&]{auto first=banner.find_first_not_of(" \r\n\t*!");banner=first==std::string::npos?"":banner.substr(first);};strip();
        if(banner.starts_with("@preserve")){banner.erase(0,9);strip();}
        if(banner.starts_with("@license")){banner.erase(0,8);strip();}
        for(const auto& r:rules_["rules"])if(text(r,"source")=="script_banner") {
            const auto prefix=text(r,"prefix"),url=text(r,"corroboration");if(!banner.starts_with(prefix)||banner.find(url)==std::string::npos)continue;
            size_t n=prefix.size();while(n<banner.size()&&(std::isalnum(static_cast<unsigned char>(banner[n]))||banner[n]=='.'||banner[n]=='-'||banner[n]=='+'))++n;
            const auto v=banner.substr(prefix.size(),n-prefix.size());if(version(v))emit(r,v,source,{prefix+v,url},out);
        }
    }
}
const WebSignatures& web_signatures() {static const WebSignatures engine(Json::parse(web_signatures_data));return engine;}
}
