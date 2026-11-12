#pragma once
#include "vector/model.hpp"
#include "embedded_libraries.hpp"
#include "tag_config.hpp"
#include <regex>

namespace vectorapp::bundle_signatures {


inline void identify(const std::string& source,const std::string& body,
                     const std::vector<unsigned char>& region,std::vector<Finding>& findings) {
    embedded_libraries::identify(source,body,region,findings);
    tag_config::identify(source,body,region,findings);
    const auto code=[&](const std::smatch& m,size_t offset=0) {
        const auto pos=offset+static_cast<size_t>(m.position());return pos<region.size()&&region[pos]==0;
    };
    static const std::regex shared(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]__core-js_shared__["'`])rx");
    static const std::regex metadata(R"rx(\.push\s{0,16}\(\s{0,16}\{\s{0,16}version\s{0,16}:\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`][^{}]{0,900}\}\s{0,16}\))rx");
    if(body.find("__core-js_shared__")!=std::string::npos)
    for(auto it=std::sregex_iterator(body.begin(),body.end(),shared);it!=std::sregex_iterator();++it) {
        if(!code(*it))continue;
        const auto start=static_cast<size_t>(it->position()+it->length());
        const auto tail=body.substr(start,2048);std::smatch record;
        if(!std::regex_search(tail,record,metadata)||!code(record,start))continue;
        const auto value=record.str(),version=record[1].str();
        const auto field=[&](const std::regex& pattern) {
            std::smatch m;if(!std::regex_search(value,m,pattern))return false;
            return code(m,start+static_cast<size_t>(record.position()));
        };
        static const std::regex mode(R"rx(\bmode\s{0,16}:[^,]{0,80}["'`](?:pure|global)["'`])rx");
        static const std::regex origin(R"rx(\bsource\s{0,16}:\s{0,16}["'`]https://github\.com/zloirock/core-js["'`])rx");
        static const std::regex license(R"rx(\blicense\s{0,16}:\s{0,16}["'`]https://github\.com/zloirock/core-js/blob/v([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})/LICENSE["'`])rx");
        std::smatch license_value;
        if(!field(mode)||!field(origin)||!field(license)||!std::regex_search(value,license_value,license)||license_value[1].str()!=version)continue;
        add_finding(findings,{"core-js","Library",version,"High",{
            {source,it->str()},{source,value}}});
    }
    if(body.find("webpackChunk_segment_analytics_next")==std::string::npos)return;
    static const std::regex segment(R"rx(\b(?:self|window)\s{0,16}(?:\.webpackChunk_segment_analytics_next|\[\s{0,16}["'`]webpackChunk_segment_analytics_next["'`]\s{0,16}\])\s{0,16}=)rx");
    static const std::regex segment_api(R"rx(\b(?:analytics\.(?:track|identify|page|load)|AnalyticsBrowser|Segmentio|SEGMENT_API_HOST)\b|\bname\s{0,16}:\s{0,16}["'`]Segment\.io["'`]\s{0,16},\s{0,16}type\s{0,16}:\s{0,16}["'`]destination["'`])rx");
    for(auto it=std::sregex_iterator(body.begin(),body.end(),segment);it!=std::sregex_iterator();++it) {
        if(!code(*it))continue;
        for(auto api=std::sregex_iterator(body.begin(),body.end(),segment_api);api!=std::sregex_iterator();++api) {
            if(!code(*api))continue;
            add_finding(findings,{"Twilio Segment","Analytics",{},"Medium",{{source,it->str()},{source,api->str()}}});
            return;
        }
    }
}
}
