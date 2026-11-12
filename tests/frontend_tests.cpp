#include "frontend.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace vectorapp;
namespace {
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool has(const std::vector<Finding>& findings,const std::string& name){return std::any_of(findings.begin(),findings.end(),[&](const auto& f){return f.name==name;});}
std::vector<Finding> html(std::string body){HttpObservation page;page.url="https://unseen.example.test/";page.body=std::move(body);std::vector<Finding> findings;identify_http(page,findings);return findings;}
std::vector<Finding> css(std::string body){std::vector<Finding> findings;identify_stylesheet("https://unseen.example.test/opaque",body,findings);return findings;}
}
int main(int argc,char** argv){try {
    if(argc==3&&std::string(argv[1])=="--cancel") {
        const auto target=parse_target(argv[2]);if(target.host!="127.0.0.1")throw std::runtime_error("Cancellation fixture is loopback only");
        ScanOptions options;options.web_only=true;options.browser_runtime=false;std::atomic_bool cancel{false};std::thread stopper;
        const auto report=scan(target,options,cancel,{},[&](const ScanEvent& e){
            if(e.state==ActivityState::Running&&e.item.starts_with("Stylesheet ")&&!stopper.joinable())stopper=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(100));cancel=true;});
        });if(stopper.joinable())stopper.join();std::cout<<report_json(report);return 0;
    }
    if(argc==3){std::ifstream file(argv[2],std::ios::binary);if(!file)throw std::runtime_error("Cannot read retained source");std::string body((std::istreambuf_iterator<char>(file)),{});ScanReport r;r.target=parse_target("https://offline.example.test/");r.status="offline_analysis";r.findings=std::string(argv[1])=="--css"?css(body):html(body);std::cout<<report_json(r);return 0;}
    const std::string router="window.__reactRouterContext = {ssr:true};",modules="window.__reactRouterRouteModules={root:route0};";
    auto findings=html("<script>"+router+"</script><script type=module>"+modules+"</script>");
    check(has(findings,"React Router"),"paired cross-script router bootstrap");check(!has(findings,"React"),"router cannot invent a React version or finding");
    for(const auto& f:findings)for(const auto& e:f.evidence)check((router+modules).find(e.value)!=std::string::npos,"router retains matched source bytes");
    check(!has(html("<script>"+router+"</script>"),"React Router"),"one marker alone insufficient");
    const std::vector<std::string> negative={
        "<p>"+router+modules+"</p>","<!-- <script>"+router+modules+"</script> -->",
        "<script>/* "+router+modules+" */</script>","<script>const example='"+router+modules+"';</script>",
        "<script>const example=`"+router+modules+"`;</script>","<script>const example=/"+router+modules+"/;</script>",
        "<script type=application/json>{\"example\":\""+router+modules+"\"}</script>",
        "<template><script>"+router+modules+"</script></template>","<textarea><script>"+router+modules+"</script></textarea>",
        "<script src=/x>"+router+modules+"</script>","<script>fakewindow.__reactRouterContext={};window.__reactRouterRouteModules={};</script>"};
    for(const auto& text:negative)check(!has(html(text),"React Router"),"quoted or inactive router example");
    check(has(html("<script>window.__remixContext={};window.__remixRouteModules={};</script>"),"Remix"),"paired Remix bootstrap");
    check(!has(html("<script>window.__remixContext={};window.__reactRouterRouteModules={};</script>"),"Remix"),"cross-product markers do not combine");
    check(has(html("<span data-radix-focus-start=''></span>"),"Radix UI"),"component namespaced marker");
    check(has(html("<div data-radix-popper-content-wrapper></div>"),"Radix UI"),"alternate component marker");
    for(const auto& text:{"<p>data-radix-focus-start</p>","<div data-state=open></div>","<script>var x='<span data-radix-focus-start>';</script>","<template><span data-radix-focus-start></span></template>","<div data-radix-focus-start-example></div>"})check(!has(html(text),"Radix UI"),"unrelated/inert Radix text");
    nlohmann::json config={{"sdkInfo",{{"sdkType","statsig-server-core-node"},{"sdkVersion","0.30.0"}}},{"feature_gates",nlohmann::json::object()},{"dynamic_configs",nlohmann::json::object()},{"layer_configs",nlohmann::json::object()},{"user",{{"email","never-export@example.test"}}}};
    auto boot=[&](const nlohmann::json& j){return "<script type=application/json>"+j.dump()+"</script>";};
    auto f=html(boot({{"runtime",{{"statsigPayload",config.dump()}}}}));check(has(f,"Statsig"),"structured embedded bootstrap");
    for(const auto& item:f) {check(item.version.empty(),"server SDK version is not endpoint version");for(const auto& e:item.evidence)check(e.value.find("never-export")==std::string::npos,"bootstrap does not export user data");}
    check(!has(html(boot({{"example",config.dump()}})),"Statsig"),"arbitrary quoted JSON example ignored");
    config.erase("feature_gates");check(!has(html(boot(config)),"Statsig"),"incomplete SDK object ignored");
    check(!has(html("<script type=application/json>{\"statsigPayload\":</script>"),"Statsig"),"truncated JSON ignored");
    const std::string banner="/*! tailwindcss v4.3.3 | MIT License | https://tailwindcss.com */";
    f=css(banner+".a{display:flex}");check(has(f,"Tailwind CSS"),"stylesheet license banner");check(f[0].version=="4.3.3","only advertised version retained");
    const std::string tw=".a{--tw-translate-x:0;--tw-ring-shadow:0 0 #0000;--tw-shadow:0 0 #0000;}";
    check(has(css(tw),"Tailwind CSS"),"compiled CSS declarations without banner");
    check(!has(css("/* "+tw+" */"),"Tailwind CSS"),"CSS comment examples ignored");
    check(!has(css("a{content:'"+banner+"';}"),"Tailwind CSS"),"quoted license banner ignored");
    check(has(css("a{content:'"+banner+"';}"+banner),"Tailwind CSS"),"real banner still found after quoted example");
    check(!has(css("a{content:'"+tw+"';}"),"Tailwind CSS"),"quoted declarations ignored");
    check(!has(css(".a{--tw-shadow:none;}"),"Tailwind CSS"),"one shared variable insufficient");
    check(has(css(":root{--bs-body-font-family:system-ui}.row{--bs-gutter-x:1rem}.btn{--bs-btn-padding-x:1rem}"),"Bootstrap"),"compiled Bootstrap variables");
    check(has(css("/*! Bootstrap v5.3.8 (https://getbootstrap.com/) */"),"Bootstrap"),"Bootstrap banner");
    HttpObservation page;page.url="https://fixture.test/start";page.body="<base href='/assets/'><link rel='stylesheet' href='hash'><link rel=stylesheet href='hash#x'><link rel='preload' as=style href='pre'><link rel='alternate stylesheet' href='alt'><link rel=stylesheet disabled href='off'><template><link rel=stylesheet href='hidden'></template>";
    auto refs=page_stylesheet_references(page);check(refs.size()==1&&refs[0]=="https://fixture.test/assets/hash","stylesheets respect base, duplicates and inactive links");
    std::cout<<checks<<" frontend evidence checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
