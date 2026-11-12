#pragma once
#include "vector/model.hpp"
#include <string>

namespace vectorapp {


inline int crawl_priority(const std::string& url) {
    std::string path;
    try { path=lower(parse_target(url).path); } catch(...) { return 0; }
    path=path.substr(0,path.find_first_of("?#"));
    if(path=="/robots.txt")return 4;
    if(path=="/sitemap.xml"||path=="/sitemap_index.xml")return 1;
    while(path.size()>1&&path.back()=='/')path.pop_back();
    const auto end=path.find('/',1);
    auto section=path.substr(1,end==std::string::npos?path.size():end-1);
    if((section.size()==2||(section.size()==5&&section[2]=='-'))&&end!=std::string::npos) {
        path=path.substr(end);
        const auto next=path.find('/',1);
        section=path.substr(1,next==std::string::npos?path.size():next-1);
    }
    if(section=="docs"||section=="documentation"||section=="developers"||section=="developer") {
        const auto next=path.find('/',1);
        if(next==std::string::npos)return 5;
        const auto tail=path.substr(next+1);
        if(!tail.empty()&&tail.size()<16&&tail.find_first_not_of("0123456789.vx")==std::string::npos)return 5;
    }
    return 2;
}
}
