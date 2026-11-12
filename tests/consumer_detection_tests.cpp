#include "vector/model.hpp"
#include "vector/overview.hpp"
#include "../src/frontend.hpp"
#include "../src/linked_page.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int checks=0;
void check(bool value,const std::string& label){++checks;if(!value)throw std::runtime_error(label);}
std::vector<Finding> script(const std::string& body){std::vector<Finding> f;identify_javascript("https://unseen.example/opaque",body,f);return f;}
bool has(const std::vector<Finding>& f,const std::string& name){return std::any_of(f.begin(),f.end(),[&](const auto& x){return x.name==name;});}
std::string core(const std::string& version="3.42.0") {
    return "var s=\"__core-js_shared__\",c=globalThis[s]||(globalThis[s]={});(c.versions||(c.versions=[])).push({version:\""+version+"\",mode:\"global\",license:\"https://github.com/zloirock/core-js/blob/v"+version+"/LICENSE\",source:\"https://github.com/zloirock/core-js\"});";
}
ScanReport request(const std::string& url,int status=200) {
    ScanReport r;r.target=parse_target("https://unseen.example/");BrowserRequest q;
    q.page=r.target.url();q.url=url;q.type="Script";q.method="GET";q.status=status;identify_browser_vendor(q,r);return r;
}
bool vendor(const ScanReport& r,const std::string& name){return std::any_of(r.vendors.begin(),r.vendors.end(),[&](const auto& v){return v.name==name;});}
#include "coverage_expansion_cases.hpp"
#include "site_comparison_cases.hpp"
#include "bybit_cases.hpp"
#include "discord_cases.hpp"
#include "fresh_cases.hpp"
#include "overview_page_cases.hpp"
int main(int argc,char** argv){try{
    if(argc==3&&(std::string(argv[1])=="--javascript"||std::string(argv[1])=="--stylesheet"||std::string(argv[1])=="--markup"||std::string(argv[1])=="--page"||std::string(argv[1])=="--report-headers")) {
        std::ifstream input(argv[2],std::ios::binary);if(!input)throw std::runtime_error("Missing source fixture");
        const std::string data{std::istreambuf_iterator<char>(input),{}};
        ScanReport r;r.status="offline_analysis";r.target=parse_target("https://unseen.example/opaque");
        if(std::string(argv[1])=="--javascript")r.findings=script(data);
        else if(std::string(argv[1])=="--stylesheet")identify_stylesheet(r.target.url(),data,r.findings);
        else if(std::string(argv[1])=="--markup") {HttpObservation h;h.url=r.target.url();h.status=200;h.body=data;identify_http(h,r.findings);identify_vendors(h,r);}
        else if(std::string(argv[1])=="--page") {HttpObservation h;h.url=r.target.url();h.status=200;h.body=data;absorb_page_markup(h,r.findings);identify_http(h,r.findings);identify_vendors(h,r);}
        else {const auto captured=nlohmann::json::parse(data);
            for(const auto& w:captured.at("web")) {
                HttpObservation h;h.url=w.at("url");h.status=w.value("status",0);
                for(const auto& value:w.at("headers"))h.headers.push_back({value.at("name"),value.at("value")});
                identify_http(h,r.findings);
            }
        }
        std::cout<<report_json(r);return 0;
    }
    for(const auto* version:{"3.42.0","3.38.1","3.49.7"}) {
        const auto f=script(core(version));check(has(f,"core-js"),"core-js generic shared-store signature");
        for(const auto& x:f)if(x.name=="core-js") {
            check(x.version==version,"advertised core-js version");
            check(x.evidence.size()==2&&x.evidence[1].value.find("license:")!=std::string::npos,"exact metadata evidence");
        }
    }
    const std::string example=core();
    check(!has(script("/*"+example+"*/"),"core-js"),"comment example rejected");
    check(!has(script("const example=`"+example+"`;"),"core-js"),"string example rejected");
    check(!has(script("const example=/__core-js_shared__/;"),"core-js"),"regex example rejected");
    check(!has(script(example.substr(0,example.size()-3)),"core-js"),"incomplete record rejected");
    for(const auto* field:{"mode:","source:","license:","__core-js_shared__"}) {
        auto wrong=example;const auto p=wrong.find(field);wrong.replace(p,std::string(field).size(),"unknown:");
        check(!has(script(wrong),"core-js"),"missing paired field rejected");
    }
    auto wrong=example;wrong.replace(wrong.find("/v3.42.0/"),9,"/v3.41.0/");
    check(!has(script(wrong),"core-js"),"mismatched license version rejected");
    check(!has(script(std::string(2*1024*1024,' ')+example),"core-js"),"no match beyond retained prefix");
    const std::string chunk="(self.webpackChunk_segment_analytics_next=self.webpackChunk_segment_analytics_next||[]).push([[5],{}]);";
    check(has(script(chunk+"analytics.track('opened');"),"Twilio Segment"),"Segment paired namespace and API");
    check(has(script(chunk+"var plugin={name:\"Segment.io\",type:\"destination\"};"),"Twilio Segment"),"Segment paired bundle metadata");
    check(!has(script(chunk+"var plugin={name:\"Other\",type:\"destination\"};"),"Twilio Segment"),"unrelated plugin metadata rejected");
    check(!has(script(chunk),"Twilio Segment"),"lone namespace insufficient");
    check(!has(script("/*"+chunk+"analytics.track('opened');*/"),"Twilio Segment"),"Segment comment example rejected");
    check(!has(script("const example=`"+chunk+"analytics.track('opened');`;"),"Twilio Segment"),"Segment literal rejected");
    check(!has(script(chunk+"const text='analytics.track';"),"Twilio Segment"),"API prose insufficient");
    const std::pair<const char*,const char*> paths[]={
        {"https://global.ketchcdn.com/web/v3/config/unseen/boot.js","Ketch"},
        {"https://cdn.ketchjs.com/ketchtag/stable/v2.12/ketch-sdk.js","Ketch"},
        {"https://www.datadoghq-browser-agent.com/datadog-rum-v6.js","Datadog"},
        {"https://www.datadoghq-browser-agent.com/us1/v7/datadog-rum.js","Datadog"},
        {"https://browser-intake-us5-datadoghq.com/api/v2/rum","Datadog"},
        {"https://browser-intake-datadoghq.com/api/v2/rum","Datadog"},
        {"https://api.ipify.org/","ipify"},{"https://api64.ipify.org/","ipify"},{"https://api6.ipify.org/","ipify"},
        {"https://cdn.cookielaw.org/consent/new-id/OtAutoBlock.js","OneTrust"}};
    for(const auto& [url,name]:paths) {
        auto r=request(std::string(url)+"?key=PRIVATE");check(vendor(r,name),std::string(name)+" exact service endpoint");
        check(report_json(r).find("PRIVATE")==std::string::npos,"request query value omitted");
        check(!vendor(request(url,0),name),"failed request never becomes observed provider");
        const auto slash=std::string(url).find('/',8);auto misleading=std::string(url);misleading.insert(slash,".unseen.example");
        check(!vendor(request(misleading),name),"lookalike suffix rejected");
        misleading=std::string(url);misleading.insert(8,"unseen.");check(!vendor(request(misleading),name),"unrelated subdomain rejected");
        check(!vendor(request(std::string(url)+".txt"),name),"unrelated endpoint path rejected");
    }
    check(vendor(request("https://browser-intake-us5-datadoghq.com/api/v2/rum",202),"Datadog"),"accepted RUM response attributed");
    expanded_checks();
    site_comparison_checks();
    bybit_checks();
    discord_checks();
    fresh_checks();
    ten_site_checks();
    twenty_site_checks();
    overview_page_checks();
    std::cout<<checks<<" consumer detection checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
