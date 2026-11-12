#include "vector/model.hpp"
#include "vector/overview.hpp"
#include "crawl_priority.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
using nlohmann::json;
namespace {
int checks=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
HttpObservation page(const std::string& body) {
    HttpObservation h;h.url="https://unseen.example/";h.status=200;h.body=body;h.headers={{"content-type","text/html"}};return h;
}
bool named(const ScanReport& r,const std::string& name) {
    const auto rows=infrastructure_entries(r);
    return std::any_of(rows.begin(),rows.end(),[&](const auto& v){return v.name==name;});
}
std::vector<DisclosureCandidate> links(const std::string& body) {
    const auto h=page(body);return disclosure_candidates(h,parse_target(h.url));
}
}
int main(int argc,char** argv){try {
    if(argc==3&&std::string(argv[1])=="--replay") {
        std::ifstream file(argv[2]);json original;file>>original;
        ScanReport r;r.target=parse_target(original.at("target").at("input").get<std::string>());
        for(const auto& q:original.at("browser").at("requests")) {
            BrowserRequest request;request.page=q.value("page","");request.url=q.value("url","");
            request.method=q.value("method","");request.type=q.value("type","");request.status=q.value("status",0L);
            identify_browser_vendor(request,r);
        }
        std::cout<<report_json(r);return 0;
    }
    if(argc==4&&std::string(argv[1])=="--links") {
        std::ifstream file(argv[2]);auto h=page(std::string(std::istreambuf_iterator<char>(file),{}));h.url=argv[3];
        json out=json::array();for(const auto& l:disclosure_candidates(h,parse_target(h.url)))
            out.push_back({{"url",l.url},{"label",l.label},{"priority",l.priority}});
        std::cout<<out.dump(2);return 0;
    }
    for(const auto& [url,name]:std::vector<std::pair<std::string,std::string>>{
        {"https://cdn.usefathom.com/script.js","Fathom Analytics"},
        {"https://plausible.io/js/script.outbound-links.js","Plausible Analytics"},
        {"https://cdn.cookielaw.org/scripttemplates/otSDKStub.js","OneTrust"},
        {"https://www.google.com/recaptcha/enterprise.js","reCAPTCHA"},
        {"https://www.gstatic.com/recaptcha/releases/abc/recaptcha__en.js","reCAPTCHA"},
        {"https://www.recaptcha.net/recaptcha/api.js","reCAPTCHA"},
        {"https://snap.licdn.com/li.lms-analytics/insight.min.js","LinkedIn Insight Tag"}}) {
        ScanReport r;identify_vendors(page("<script src='"+url+"?key=PRIVATE'></script>"),r);
        check(named(r,name),"active product resource recognized on an unrelated target");
        check(report_json(r).find("PRIVATE")==std::string::npos,"query values remain redacted");
        check(r.vendors.front().evidence.front().value.find("attribution path:")!=std::string::npos,"matched path retained as evidence");
        r={};BrowserRequest q;q.page="https://another-unseen.example/";q.url=url;q.status=200;q.method="GET";q.type="Script";
        identify_browser_vendor(q,r);check(named(r,name),"browser response supplies the same attribution");
        r={};q.status=0;identify_browser_vendor(q,r);check(!named(r,name),"unreceived request is not observed contact");
    }
    for(const auto* url:{"https://plausible.io/docs", "https://plausible.io/js/not-a-script.txt",
        "https://www.google.com/search?q=/recaptcha/api.js", "https://www.gstatic.com/other.js",
        "https://cdn.usefathom.com.evil.example/script.js", "https://child.cdn.usefathom.com/script.js",
        "https://cdn.usefathom.com/script.js/other/script.js", "https://snap.licdn.com/other.js",
        "https://cdn.cookielaw.org/unknown.js"}) {
        ScanReport r;identify_vendors(page(std::string("<script src='")+url+"'></script>"),r);
        check(infrastructure_entries(r).empty(),"lookalike host, query and unrelated shared-host path stay unattributed");
    }
    ScanReport hints;auto h=page("<link rel='preconnect' href='https://cdn.usefathom.com'><script>const example='https://plausible.io/js/script.js';</script>");
    h.headers.push_back({"content-security-policy","script-src https://cdn.cookielaw.org https://www.google.com/recaptcha/"});
    identify_vendors(h,hints);check(infrastructure_entries(hints).empty(),"CSP preconnect and code literals do not become observed integrations");
    auto found=links("<a href='https://proton.example/'>Privacy by default</a><a href='https://vpn.example/'>Best VPN for speed and security</a><a href='/blog/security-launch'>Read the launch</a><a href='/adapt'>Adapt</a>");
    check(found.empty(),"marketing titles and keyword substrings do not consume disclosure transfers");
    found=links("<a href='/compliance'>Compliance</a><a href='/legal'>Legal</a><a href='/list-of-subprocessors'>Providers</a>");
    check(found.size()==3&&found.front().priority==3,"compliance and explicit source paths discovered; provider list ranked first");
    found=links("<a href='https://hosted-trust.example/organization'>Explore our Trust Center</a><a href='https://policy.example/'>Read our privacy policy</a>");
    check(found.size()==2&&found.front().priority==2&&!found.front().chain.empty(),"explicit hosted trust and privacy links retain discovery chain");
    check(links("<a href='https://hosted-policy.example/opaque' aria-label='Privacy policy' title='Privacy policy'>Privacy policy</a>").size()==1,"repeated accessible and visible privacy label remains an explicit source link");
    check(links("<a href='https://showcase.example/' title='Privacy by default'>Privacy by default</a>").empty(),"repeated marketing title does not become a legal label");
    check(links("<a href='https://unrelated.example/subprocessors'>Customers</a><a href='http://policy.example/'>Privacy</a>").empty(),"external unlabeled paths and HTTPS downgrade excluded");
    check(links("<div hidden><a href='/subprocessors'>Subprocessors</a></div><script><a href='/compliance'>Compliance</a></script>").empty(),"hidden and script links excluded");
    for(const auto* path:{"/docs","/documentation/","/en/docs/","/docs/v4","/docs/4.x","/developers"})
        check(crawl_priority(std::string("https://unseen.example")+path)==5,"linked documentation landing pages precede crawl guesses");
    check(crawl_priority("https://unseen.example/blog/docs")<5&&crawl_priority("https://unseen.example/docs/a-post")<5,"documentation mentions and deep articles do not take root priority");
    check(crawl_priority("https://unseen.example/robots.txt")>crawl_priority("https://unseen.example/product"),"robots remains available before ordinary pages");
    check(crawl_priority("https://unseen.example/product")>crawl_priority("https://unseen.example/sitemap.xml"),"known linked pages precede guessed sitemaps");
    std::vector<Finding> generator;identify_http(page("<meta name='generator' content='Docusaurus v3.9.2'>"),generator);
    check(std::any_of(generator.begin(),generator.end(),[](const auto& f){return f.name=="Docusaurus"&&f.version=="3.9.2"&&!f.evidence.empty();}),"generator advertises Docusaurus and exact version");
    generator.clear();identify_http(page("<p>Docusaurus v3.9.2</p><meta name='description' content='Docusaurus v3.9.2'><meta name='generator' content='Not Docusaurus v3.9.2'>"),generator);
    check(std::none_of(generator.begin(),generator.end(),[](const auto& f){return f.name=="Docusaurus";}),"prose descriptions and misleading generators do not identify Docusaurus");
    std::cout<<checks<<" cross-site checks passed.\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
