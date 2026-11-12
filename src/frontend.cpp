#include "frontend.hpp"
#include "markup.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>

namespace vectorapp {
namespace {

std::string code_only(const std::string& text, bool javascript, std::vector<unsigned char>* regions=nullptr) {
    auto out=text;
    if(regions)regions->assign(text.size(),0);
    for(size_t i=0;i<text.size();) {
        const auto start=i;const char c=text[i];unsigned char data=0;
        if(c=='\''||c=='"'||(javascript&&c=='`')) {
            data=true;++i;
            while(i<text.size()) {if(text[i]=='\\'){i=std::min(i+2,text.size());continue;}if(text[i++]==c)break;}
        } else if(c=='/'&&i+1<text.size()&&text[i+1]=='*') {
            data=2;const auto end=text.find("*/",i+2);i=end==std::string::npos?text.size():end+2;
        } else if(!javascript&&c=='\\') {
            i=std::min(i+2,text.size());
        } else if(javascript&&c=='/'&&i+1<text.size()&&text[i+1]=='/') {
            data=true;const auto end=text.find('\n',i+2);i=end==std::string::npos?text.size():end;
        } else if(javascript&&c=='/') {
            size_t prev=i;while(prev&&markup::space(text[prev-1]))--prev;
            data=prev==0||std::string("=([{,:;!?&|+*-~%^<>").find(text[prev-1])!=std::string::npos;
            ++i;
            if(data) {bool bracket=false;while(i<text.size()&&text[i]!='\n') {
                const auto next=text[i++];if(next=='\\'){if(i<text.size())++i;continue;}
                if(next=='[')bracket=true;else if(next==']')bracket=false;else if(next=='/'&&!bracket)break;
            }}
        } else ++i;
        if(data) {std::fill(out.begin()+start,out.begin()+i,' ');if(regions)std::fill(regions->begin()+start,regions->begin()+i,data);}
    }
    return out;
}
bool marker(const std::string& code,const std::string& original,const std::regex& pattern,std::string& evidence) {
    std::smatch match;if(!std::regex_search(code,match,pattern))return false;
    evidence=original.substr(static_cast<size_t>(match.position()),static_cast<size_t>(match.length()));return true;
}
void statsig_config(const nlohmann::json& value,const std::string& source,std::vector<Finding>& findings,size_t& visited,int depth=0) {
    if(depth>12||++visited>4096)return;
    if(value.is_object()) {
        const auto sdk=value.find("sdkInfo"),gates=value.find("feature_gates"),configs=value.find("dynamic_configs"),layers=value.find("layer_configs");
        if(sdk!=value.end()&&sdk->is_object()&&gates!=value.end()&&gates->is_object()&&
           configs!=value.end()&&configs->is_object()&&layers!=value.end()&&layers->is_object()) {
            const auto type=sdk->find("sdkType");
            if(type!=sdk->end()&&type->is_string()&&type->get_ref<const std::string&>().starts_with("statsig-")&&type->get_ref<const std::string&>().size()<100)
                add_finding(findings,{"Statsig","Feature management",{},"Medium",{{source+" | JSON bootstrap","sdkInfo.sdkType: "+type->get<std::string>()+"; feature_gates, dynamic_configs and layer_configs objects"}}});
        }
        for(auto it=value.begin();it!=value.end()&&visited<=4096;++it) {
            if(it->is_structured())statsig_config(*it,source,findings,visited,depth+1);
            else if(it->is_string()&&(it.key()=="statsigPayload"||it.key()=="statsigValues"||it.key()=="statsig")) {
                const auto& text=it->get_ref<const std::string&>();
                if(text.size()>2*1024*1024)continue;
                auto decoded=nlohmann::json::parse(text,nullptr,false);
                if(decoded.is_object())statsig_config(decoded,source,findings,visited,depth+1);
            }
        }
    } else if(value.is_array())for(const auto& child:value) {if(visited>4096)break;statsig_config(child,source,findings,visited,depth+1);}
}
}

void identify_frontend_markup(const HttpObservation& page,std::vector<Finding>& findings) {
    std::string router_context,router_modules,remix_context,remix_modules;
    static const std::regex rc(R"(\bwindow\s{0,32}\.\s{0,32}__reactRouterContext\s{0,32}=\s{0,32}\{)"),rm(R"(\bwindow\s{0,32}\.\s{0,32}__reactRouterRouteModules\s{0,32}=\s{0,32}\{)");
    static const std::regex mc(R"(\bwindow\s{0,32}\.\s{0,32}__remixContext\s{0,32}=\s{0,32}\{)"),mm(R"(\bwindow\s{0,32}\.\s{0,32}__remixRouteModules\s{0,32}=\s{0,32}\{)");
    static const std::set<std::string> radix={"data-radix-focus-guard","data-radix-focus-start","data-radix-popper-content-wrapper","data-radix-portal","data-radix-scroll-area-viewport","data-radix-collection-item"};
    std::string inline_styles,base_ui_id,headless_id;std::map<std::string,std::string> base_ui;std::vector<std::string> headless;
    size_t lucide_icons=0;
    std::string lucide_sample;
    const auto lucide_classes=[](const std::string& value) {
        bool base=false,icon=false;
        for(size_t at=0;at<value.size();) {
            const auto end=value.find_first_of(" \t\r\n",at);
            const auto token=value.substr(at,end==std::string::npos?std::string::npos:end-at);
            if(token=="lucide")base=true;
            else if(token.size()>7&&token.size()<=64&&token.starts_with("lucide-")&&
                    token.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-",7)==std::string::npos)icon=true;
            if(end==std::string::npos)break;
            at=end+1;
        }
        return base&&icon;
    };
    for(const auto& tag:markup::tags(page.body)) {
        if(tag.name=="svg") {
            const auto classes=tag.attributes.find("class");
            if(classes!=tag.attributes.end()&&lucide_classes(classes->second)) {
                ++lucide_icons;
                if(lucide_sample.empty())lucide_sample=classes->second.substr(0,120);
            }
        }
        if(tag.name=="script") {
            if(!tag.attr("src").empty())continue;
            if(markup::javascript(tag)) {
                const auto code=code_only(tag.body,true);
                marker(code,tag.body,rc,router_context);marker(code,tag.body,rm,router_modules);
                marker(code,tag.body,mc,remix_context);marker(code,tag.body,mm,remix_modules);
            } else if(lower(trim(tag.attr("type")))=="application/json"&&tag.body.size()<=3*1024*1024) {
                auto data=nlohmann::json::parse(tag.body,nullptr,false);size_t visited=0;
                if(!data.is_discarded())statsig_config(data,page.url,findings,visited);
            }
        } else if(tag.name=="style") {
            if(inline_styles.size()<512*1024&&!tag.body.empty())
                inline_styles.append(tag.body,0,512*1024-inline_styles.size()).push_back('\n');
            static const std::regex release(R"(^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$)");
            const auto emotion_key=[](const std::string& value) {
                if(value.empty()||!std::islower(static_cast<unsigned char>(value.front()))||value.back()==' ')return false;
                char previous=0;
                for(const char c:value) {
                    if(c==' '){if(previous==' ')return false;}
                    else if(!std::islower(static_cast<unsigned char>(c))&&!std::isdigit(static_cast<unsigned char>(c))&&c!='-')return false;
                    previous=c;
                }
                return true;
            };
            const auto styled=tag.attributes.find("data-styled"),styled_version=tag.attributes.find("data-styled-version");
            if(styled!=tag.attributes.end()&&styled_version!=tag.attributes.end()&&std::regex_match(styled_version->second,release))
                add_finding(findings,{"styled-components","Library",styled_version->second,"High",{{page.url+" | style element","data-styled"},{page.url+" | style element","data-styled-version=\""+styled_version->second+"\""}}});
            const auto emotion=tag.attributes.find("data-emotion");
            if(emotion!=tag.attributes.end()&&emotion_key(emotion->second)&&
               (emotion->second.find(' ')!=std::string::npos||tag.attributes.contains("data-s")))
                add_finding(findings,{"Emotion","Library",{},"Medium",{{page.url+" | style element","data-emotion=\""+emotion->second.substr(0,120)+"\""}}});
        } else if(tag.name!="style"&&tag.name!="meta"&&tag.name!="link"&&tag.name!="textarea"&&tag.name!="title") {
            for(const auto& [name,value]:tag.attributes)if(radix.contains(name)) {
                add_finding(findings,{"Radix UI","Library",{},"Medium",{{page.url+" | component attribute",name}}});break;
            }
            for(const auto& [name,value]:tag.attributes) {
                if(name.starts_with("data-base-ui-")&&name.size()>16&&name.size()<=64&&base_ui.size()<2)base_ui.emplace(name,tag.name);
                else if(name=="id"&&base_ui_id.empty()&&value.starts_with("base-ui-")&&value.size()>12&&value.size()<=80)base_ui_id=value;
            }
            if(tag.attributes.contains("data-headlessui-state")&&headless.size()<2)headless.push_back(tag.name);
            const auto id=tag.attr("id");
            if(headless_id.empty()&&id.starts_with("headlessui-")&&id.size()>14&&id.size()<=100)headless_id=id;
        }
    }
    if(headless.size()>=2||(!headless.empty()&&!headless_id.empty())) {
        std::vector<Evidence> evidence{{page.url+" | component attribute","<"+headless[0]+"> data-headlessui-state"}};
        if(headless.size()>=2)evidence.push_back({page.url+" | component attribute","<"+headless[1]+"> data-headlessui-state"});
        if(!headless_id.empty())evidence.push_back({page.url+" | component id","id=\""+headless_id+"\""});
        add_finding(findings,{"Headless UI","Library",{},"Medium",std::move(evidence)});
    }
    if(base_ui.size()>=2||(!base_ui.empty()&&!base_ui_id.empty())) {
        std::vector<Evidence> evidence;
        for(const auto& [name,element]:base_ui)evidence.push_back({page.url+" | component attribute","<"+element+"> "+name});
        if(evidence.size()<2)evidence.push_back({page.url+" | component id","id=\""+base_ui_id+"\""});
        add_finding(findings,{"Base UI","Library",{},"Medium",std::move(evidence)});
    }
    if(lucide_icons>=2)
        add_finding(findings,{"Lucide","Library",{},"Medium",{{page.url+" | svg elements",std::to_string(lucide_icons)+" elements with the lucide class and an icon class"},
            {page.url+" | svg element","class=\""+lucide_sample+"\""}}});
    if(!inline_styles.empty())identify_stylesheet(page.url+" | inline style",inline_styles,findings);
    const auto source=page.url+" | inline framework bootstrap";
    if(!router_context.empty()&&!router_modules.empty())add_finding(findings,{"React Router","Framework",{},"High",{{source,router_context},{source,router_modules}}});
    if(!remix_context.empty()&&!remix_modules.empty())add_finding(findings,{"Remix","Framework",{},"High",{{source,remix_context},{source,remix_modules}}});
}

void identify_stylesheet(const std::string& url,const std::string& body,std::vector<Finding>& findings) {
    const auto sample=body.substr(0,512*1024);std::vector<unsigned char> regions;const auto code=code_only(sample,false,&regions);
    struct Banner {const char* name;const char* pattern;};
    static const Banner banners[]={
        {"Tailwind CSS",R"rx(/\*!\s{0,32}tailwindcss v([0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?)\s{0,32}\|\s{0,32}MIT License\s{0,32}\|\s{0,32}https://tailwindcss\.com\s{0,32}\*/)rx"},
        {"Bootstrap",R"rx(/\*!\s{0,32}\*?\s{0,32}Bootstrap\s+v([0-9]+\.[0-9]+\.[0-9]+)\s{0,32}\(https://getbootstrap\.com/?\))rx"},
        {"Bulma",R"rx(/\*!\s{0,32}bulma\.io v([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})\s{0,32}\|\s{0,32}MIT License\s{0,32}\|\s{0,32}github\.com/jgthms/bulma\s{0,32}\*/)rx"}
    };
    for(const auto& rule:banners) {
        const std::regex pattern(rule.pattern);
        for(auto it=std::sregex_iterator(sample.begin(),sample.end(),pattern);it!=std::sregex_iterator();++it)
            if(regions[static_cast<size_t>(it->position())]==2)
                add_finding(findings,{rule.name,"Library",(*it)[1].str(),"High",{{url+" | stylesheet banner",it->str()}}});
    }
    const auto declarations=[&](const char* name,std::initializer_list<const char*> properties) {
        std::vector<Evidence> evidence;
        for(const auto* property:properties) {
            std::string match;const std::regex pattern(std::string("(?:^|[;{])\\s{0,32}(")+property+")\\s{0,32}:");
            if(!marker(code,sample,pattern,match))return;
            evidence.push_back({url+" | stylesheet declarations",trim(match)});
        }
        add_finding(findings,{name,"Library",{},"Medium",std::move(evidence)});
    };
    declarations("Tailwind CSS",{"--tw-translate-x","--tw-ring-shadow","--tw-shadow"});
    declarations("Bootstrap",{"--bs-body-font-family","--bs-gutter-x","--bs-btn-padding-x"});
    static const std::regex katex_version(R"rx(\.katex\s{1,8}\.katex-version::?after\s{0,16}\{\s{0,16}content\s{0,16}:\s{0,16}["']([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'])rx");
    static const std::regex katex_font(R"rx(@font-face\s{0,16}\{[^{}]{0,200}\bfont-family\s{0,16}:\s{0,16}["']?KaTeX_Main\b)rx");
    std::smatch version,font;
    if(std::regex_search(sample,version,katex_version)&&regions[static_cast<size_t>(version.position())]==0&&
       std::regex_search(sample,font,katex_font)&&regions[static_cast<size_t>(font.position())]==0)
        add_finding(findings,{"KaTeX","Library",version[1].str(),"High",{{url+" | stylesheet rules",version.str()},{url+" | stylesheet rules",font.str().substr(0,160)}}});
    static const std::regex awesome_family(R"rx(--fa-family-(?:brands|classic|duotone|sharp|sharp-duotone)\s{0,16}:\s{0,16}["']Font Awesome ([0-9]{1,2}) (?:Brands|Free|Pro|Duotone|Sharp)[^"']{0,24}["'])rx");
    static const std::regex awesome_face(R"rx(@font-face\s{0,16}\{[^{}]{0,200}\bfont-family\s{0,16}:\s{0,16}["']Font Awesome [0-9]{1,2} (?:Brands|Free|Pro|Duotone|Sharp))rx");
    std::smatch family,face;
    if(std::regex_search(sample,family,awesome_family)&&regions[static_cast<size_t>(family.position())]==0&&
       std::regex_search(sample,face,awesome_face)&&regions[static_cast<size_t>(face.position())]==0)
        add_finding(findings,{"Font Awesome","Library",family[1].str(),"High",{{url+" | stylesheet rules",family.str()},{url+" | stylesheet rules",face.str().substr(0,160)}}});
    static const std::regex splide_state(R"rx(\.splide\.is-initialized:not\(\.is-active\)\s{1,8}\.splide__list\s{0,16}\{)rx");
    static const std::regex splide_track(R"rx(\.splide__track--(?:draggable|fade|ttb|nav)\b[^{}]{0,200}\{)rx");
    std::smatch state,track;
    if(std::regex_search(sample,state,splide_state)&&regions[static_cast<size_t>(state.position())]==0&&
       std::regex_search(sample,track,splide_track)&&regions[static_cast<size_t>(track.position())]==0)
        add_finding(findings,{"Splide","Library",{},"Medium",{{url+" | stylesheet rules",state.str()},{url+" | stylesheet rules",track.str().substr(0,160)}}});
    if(code.find("--radix-")!=std::string::npos) {
        static const std::regex radix_read(R"rx(^[;{]\s{0,32}(-{0,2}[A-Za-z_][A-Za-z0-9_-]{0,80})\s{0,32}:[^;{}]{0,240}?\bvar\(\s{0,16}(--radix-[a-z0-9]{2,40}(?:-[a-z0-9]{1,40}){1,6})\s{0,16}[,)])rx");
        static const std::regex radix_selector(R"rx(^\[data-radix-[a-z0-9]{2,40}(?:-[a-z0-9]{1,40}){0,6}\][^{};]{0,200}\{)rx");
        std::vector<std::pair<std::string,std::string>> reads;std::string selector;
        size_t visits=0;
        for(auto hit=code.find("--radix-");hit!=std::string::npos&&reads.size()<2&&++visits<=256;hit=code.find("--radix-",hit+8)) {
            const auto floor=hit>320?hit-320:0;const auto start=code.find_last_of(";{",hit);
            if(start==std::string::npos||start<floor)continue;
            const auto window=code.substr(start,hit-start+120);std::smatch read;
            if(!std::regex_search(window,read,radix_read))continue;
            const auto name=read[2].str();const auto component=name.substr(8,name.find('-',8)-8);bool known=false;
            for(const auto& entry:reads)known=known||entry.first==component;
            if(!known)reads.push_back({component,trim(read.str().substr(1)).substr(0,160)});
        }
        visits=0;
        for(auto hit=code.find("[data-radix-");hit!=std::string::npos&&++visits<=256;hit=code.find("[data-radix-",hit+12)) {
            if(hit&&code[hit-1]=='\\')continue;
            const auto window=code.substr(hit,280);std::smatch head;
            if(std::regex_search(window,head,radix_selector)){selector=head.str().substr(0,head.str().find(']')+1)+" selector";break;}
        }
        if(!reads.empty()&&(!selector.empty()||reads.size()>=2))
            add_finding(findings,{"Radix UI","Library",{},"Medium",{{url+" | stylesheet rules",reads[0].second},
                {url+" | stylesheet rules",selector.empty()?reads[1].second:selector}}});
    }
}

std::vector<std::string> page_stylesheet_references(const HttpObservation& page) {
    std::vector<std::string> result;const auto tags=markup::tags(page.body);std::string base=page.url;
    auto resolve=[](const std::string& base,const std::string& ref) {
        std::string result;if(ref.empty()||ref[0]=='#')return result;
        auto* handle=curl_url();if(!handle)return result;
        if(curl_url_set(handle,CURLUPART_URL,base.c_str(),0)==CURLUE_OK&&curl_url_set(handle,CURLUPART_URL,ref.c_str(),0)==CURLUE_OK) {
            curl_url_set(handle,CURLUPART_FRAGMENT,nullptr,0);char* full=nullptr;
            if(curl_url_get(handle,CURLUPART_URL,&full,0)==CURLUE_OK){result=full;curl_free(full);}
        }
        curl_url_cleanup(handle);
        if(result.size()>2048||(!result.starts_with("https://")&&!result.starts_with("http://")))result.clear();
        return result;
    };
    for(const auto& tag:tags)if(tag.name=="base") {auto ref=resolve(base,tag.attr("href"));if(!ref.empty())base=ref;break;}
    for(const auto& tag:tags)if(tag.name=="link"&&!tag.attributes.contains("disabled")) {
        auto rel=" "+lower(tag.attr("rel"))+" ";for(auto& c:rel)if(markup::space(c))c=' ';
        if(rel.find(" stylesheet ")==std::string::npos||rel.find(" alternate ")!=std::string::npos)continue;
        auto ref=resolve(base,tag.attr("href"));if(!ref.empty()&&std::find(result.begin(),result.end(),ref)==result.end())result.push_back(std::move(ref));
        if(result.size()>=128)break;
    }
    return result;
}
}
