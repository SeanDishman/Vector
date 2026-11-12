#include "vector/model.hpp"
#include "markup.hpp"
#include "frontend.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <regex>
#include <string_view>

namespace vectorapp {
namespace {
std::string host_of(const std::string& url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return {};
    auto start = scheme + 3;
    if (start < url.size() && url[start] == '[') {
        const auto end = url.find(']', start);
        if (end == std::string::npos) return {};
        return lower(url.substr(start + 1, end - start - 1));
    }
    const auto colon = url.find(':', start);
    const auto slash = url.find('/', start);
    auto stop = url.size();
    if (colon != std::string::npos) stop = std::min(stop, colon);
    if (slash != std::string::npos) stop = std::min(stop, slash);
    return lower(url.substr(start, stop - start));
}
std::string resolve_ref(const std::string& page, std::string ref) {
    ref=trim(std::move(ref));
    if(ref.empty()||ref.starts_with("#"))return {};
    auto* handle=curl_url();if(!handle)return {};
    std::string result;
    if(curl_url_set(handle,CURLUPART_URL,page.c_str(),0)==CURLUE_OK &&
       curl_url_set(handle,CURLUPART_URL,ref.c_str(),0)==CURLUE_OK) {
        curl_url_set(handle,CURLUPART_FRAGMENT,nullptr,0);
        char* full=nullptr;
        if(curl_url_get(handle,CURLUPART_URL,&full,0)==CURLUE_OK){result=full;curl_free(full);}
    }
    curl_url_cleanup(handle);
    if(!result.starts_with("https://")&&!result.starts_with("http://"))return {};
    return result;
}
void remember_script(std::vector<std::string>& urls,const std::string& page,const std::string& ref) {
    auto absolute=resolve_ref(page,ref);
    if(absolute.empty()||absolute.size()>2048)return;
    if(std::find(urls.begin(),urls.end(),absolute)==urls.end())urls.push_back(std::move(absolute));
}
void import_urls(const markup::Tag& tag,const std::string& base,std::vector<std::string>& urls) {
    const auto map=nlohmann::json::parse(tag.body,nullptr,false);
    if(!map.is_object())return;
    auto entries=[&](const nlohmann::json& entries) {
        if(!entries.is_object())return;
        for(const auto& [key,value]:entries.items()) {
            if(value.is_string()&&!key.ends_with('/'))remember_script(urls,base,value.get<std::string>());
        }
    };
    if(map.contains("imports"))entries(map["imports"]);
    if(map.contains("scopes")&&map["scopes"].is_object())for(const auto& scope:map["scopes"])entries(scope);
}
std::string strip_fragment(std::string url) {
    const auto hash = url.find('#');
    if (hash != std::string::npos) url.resize(hash);
    return url;
}
std::string path_only(std::string url) {
    url = strip_fragment(std::move(url));
    const auto query = url.find('?');
    if (query != std::string::npos) url.resize(query);
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return lower(url);
    const auto path = url.find('/', scheme + 3);
    return path == std::string::npos ? "/" : lower(url.substr(path));
}
bool html_page_candidate(const std::string& url) {
    auto path = path_only(url);
    const auto slash = path.rfind('/');
    const auto file = slash == std::string::npos ? path : path.substr(slash + 1);
    const auto dot = file.rfind('.');
    if (dot == std::string::npos || dot + 1 >= file.size()) return true;
    const auto ext = file.substr(dot + 1);
    static const char* skip[] = {
        "js","mjs","css","map","json","jpg","jpeg","png","gif","webp","avif","ico","svg","bmp","tif","tiff",
        "woff","woff2","ttf","otf","eot","mp3","mp4","webm","mov","avi","mkv","pdf","zip","gz","tgz","rar","7z",
        "wasm","bin","exe","dmg","apk","iso"
    };
    for (const auto* s : skip) if (ext == s) return false;
    return true;
}
void remember_page(std::vector<std::string>& urls, const std::string& page, std::string ref) {
    auto absolute = strip_fragment(resolve_ref(page, std::move(ref)));
    if (absolute.empty() || absolute.size() > 2048) return;
    if (host_of(absolute) != host_of(page)) return;
    if (!html_page_candidate(absolute)) return;
    if (std::find(urls.begin(), urls.end(), absolute) == urls.end()) urls.push_back(std::move(absolute));
}
}

std::vector<std::string> page_html_urls(const HttpObservation& page) {
    std::vector<std::string> urls;
    if (page.url.empty() || page.body.empty()) return urls;
    for(const auto& tag:markup::tags(page.body))
        if(tag.name=="a"||tag.name=="area")remember_page(urls,page.url,tag.attr("href"));
    return urls;
}

std::vector<std::string> page_anchor_urls(const HttpObservation& page) {
    std::vector<std::string> urls;
    if (page.url.empty() || page.body.empty()) return urls;
    for(const auto& tag:markup::tags(page.body)) {
        if(tag.name!="a"&&tag.name!="area")continue;
        auto absolute=strip_fragment(resolve_ref(page.url,tag.attr("href")));
        if(absolute.empty()||absolute.size()>2048||!html_page_candidate(absolute))continue;
        if(std::find(urls.begin(),urls.end(),absolute)==urls.end())urls.push_back(std::move(absolute));
        if(urls.size()>=512)break;
    }
    return urls;
}

std::vector<std::string> sitemap_page_urls(const std::string& page_url, const std::string& body) {
    std::vector<std::string> urls;
    if (page_url.empty() || body.empty()) return urls;
    static const std::regex loc(R"(<\s*loc\s*>\s*([^<]{1,2048})\s*<\s*/\s*loc\s*>)", std::regex::icase);
    for (auto it = std::sregex_iterator(body.begin(), body.end(), loc); it != std::sregex_iterator(); ++it)
        remember_page(urls, page_url, trim(it->str(1)));
    return urls;
}

std::vector<std::string> robots_sitemap_urls(const std::string& page_url, const std::string& body) {
    std::vector<std::string> urls;
    if (page_url.empty() || body.empty()) return urls;
    static const std::regex line(R"(^\s*Sitemap\s*:\s*(\S+))", std::regex::icase);
    std::string text = body;
    for (auto& c : text) if (c == '\r') c = '\n';
    size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find('\n', start);
        const auto row = text.substr(start, (end == std::string::npos ? text.size() : end) - start);
        start = end == std::string::npos ? text.size() : end + 1;
        std::smatch match;
        if (!std::regex_search(row, match, line)) continue;
        remember_page(urls, page_url, match[1].str());
    }
    return urls;
}

std::vector<std::string> page_script_references(const HttpObservation& page) {
    std::vector<std::string> urls;
    if(page.url.empty()||page.body.empty())return urls;
    const auto tags=markup::tags(page.body);std::string base=page.url;
    for(const auto& tag:tags)if(tag.name=="base"&&!tag.attr("href").empty()) {
        const auto resolved=resolve_ref(page.url,tag.attr("href"));if(!resolved.empty())base=resolved;break;
    }
    for(const auto& tag:tags) {
        if(tag.name=="script") {
            if(markup::javascript(tag)&&!tag.attr("src").empty())remember_script(urls,base,tag.attr("src"));
            else if(lower(trim(tag.attr("type")))=="importmap")import_urls(tag,base,urls);
        } else if(tag.name=="link") {
            auto rel=" "+lower(tag.attr("rel"))+" ";
            for(auto& c:rel)if(markup::space(c))c=' ';
            if(rel.find(" modulepreload ")!=std::string::npos ||
               (rel.find(" preload ")!=std::string::npos && lower(tag.attr("as"))=="script"))
                remember_script(urls,base,tag.attr("href"));
        }
    }
    return urls;
}
std::vector<std::string> page_script_urls(const HttpObservation& page) {
    auto urls=page_script_references(page);
    std::erase_if(urls,[&](const auto& url){return host_of(url)!=host_of(page.url);});
    return urls;
}
void absorb_page_markup(const HttpObservation& page,std::vector<Finding>& findings) {
    identify_frontend_markup(page,findings);
    for(const auto& tag:markup::tags(page.body)) {
        if(tag.name!="script"||!tag.attr("src").empty()||trim(tag.body).empty())continue;
        if(markup::javascript(tag))identify_javascript(page.url+" | inline script",tag.body,findings);
    }
    for(const auto& url:page_script_references(page))identify_script_reference(url,page.url+" | script reference",findings);
}
}
