#include "vector/model.hpp"
#include "vector/overview.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <stdexcept>

using namespace vectorapp;
int checks=0;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
bool has(const std::vector<Finding>& findings,const std::string& name,const std::string& version="") {
    return std::any_of(findings.begin(),findings.end(),[&](const auto& f){return f.name==name&&(version.empty()||f.version==version);});
}
int main(int argc,char** argv) {
    try {
        if(argc==3&&std::string(argv[1])=="--replay-report") {
            std::ifstream input(argv[2]);if(!input)throw std::runtime_error("Cannot read captured report");
            const auto captured=nlohmann::json::parse(input);ScanReport report;
            for(const auto& row:captured.at("findings")) {
                Finding f{row.at("name"),row.at("category"),row.at("version"),row.at("confidence"),{}};
                for(const auto& e:row.at("evidence"))f.evidence.push_back({e.at("source"),e.at("value")});
                report.findings.push_back(std::move(f));
            }
            for(const auto& row:captured.at("vendors")) {
                VendorRelationship v;v.name=row.at("name");v.category=row.at("category");v.domain=row.at("domain");
                v.relationship=row.at("relationship");v.evidence_level=row.at("evidence_level");v.confidence=row.at("confidence");
                v.capability=row.at("capability");v.attribution_source=row.at("attribution_source");
                v.published_by=row.value("published_by","");v.recorded_at=row.value("recorded_at","");
                for(const auto& e:row.at("evidence"))v.evidence.push_back({e.at("source"),e.at("value")});
                report.vendors.push_back(std::move(v));
            }
            const auto names=[&] {nlohmann::json names=nlohmann::json::array();for(const auto& r:infrastructure_entries(report))names.push_back(r.name);return names;};
            const auto before=names();
            for(const auto& r:captured.at("browser").at("requests")) {
                const auto mime=r.value("mime","");
                if(r.value("status",0)>0 && (r.value("type","")=="Script"||mime.find("javascript")!=std::string::npos))
                    identify_script_reference(r.at("url"),"Browser request from "+r.at("page").get<std::string>(),report.findings);
            }
            auto output=nlohmann::json::parse(report_json(report));
            output["analysis"]="Offline replay of existing browser resource URLs; original findings and vendors retained. No new collection.";
            output["captured_at"]=captured.at("started_at");output["before"]=before;output["after"]=names();
            std::cout<<output.dump(2)<<'\n';return 0;
        }
        if(argc==4&&std::string(argv[1])=="--javascript") {
            std::ifstream input(argv[2],std::ios::binary);if(!input)throw std::runtime_error("Cannot read captured JavaScript");
            const std::string body{std::istreambuf_iterator<char>(input),{}};
            ScanReport report;report.target=parse_target(argv[3]);report.status="offline_analysis";
            identify_javascript(argv[3],body,report.findings);
            report.notes={"Offline signature analysis of saved JavaScript; no network or browser execution."};
            std::cout<<report_json(report)<<'\n';return 0;
        }
        if(argc==4&&std::string(argv[1])=="--html") {
            std::ifstream input(argv[2],std::ios::binary);if(!input)throw std::runtime_error("Cannot read retained HTML");
            ScanReport report;report.target=parse_target(argv[3]);report.status="offline_analysis";
            HttpObservation page;page.url=report.target.url();page.body.assign(std::istreambuf_iterator<char>(input),{});
            identify_http(page,report.findings);
            report.notes={"Offline signature analysis of retained HTML only; no network or browser execution."};
            std::cout<<report_json(report)<<'\n';return 0;
        }
        check(parse_target("https://example.test/npm/@polymer/polymer@3.5.2/index.js").host=="example.test","at signs in a package path are not credentials");
        bool rejected=false;try {parse_target("https://@example.test/path");}catch(...){rejected=true;}
        check(rejected,"empty userinfo still rejected");
        struct Case {const char* name;const char* js;const char* version;};
        const Case cases[]={
            {"Polymer","window.Polymer.version='3.5.2';","3.5.2"},
            {"Polymer","Polymer({is:'example-card',properties:{}});",""},
            {"lit-html","(globalThis.litHtmlVersions?" "?=[]).push('3.3.1');","3.3.1"},
            {"LitElement","(e.litElementVersions||(e.litElementVersions=[])).push('4.2.0');","4.2.0"},
            {"Lit ReactiveElement","(global.reactiveElementVersions?" "?=[]).push('2.1.0');","2.1.0"},
            {"React","const a=Symbol.for('react.transitional.element'),b=Symbol.for('react.fragment');",""},
            {"React","React.version='19.1.0';","19.1.0"},
            {"jQuery","jQuery.fn.jquery='3.7.1';","3.7.1"},
            {"ShadyCSS","window.ShadyCSS.prepareTemplate(template,'example-card');",""},
            {"Google Closure Library","goog.provide=function(name){};",""},
            {"webpack","(self.webpackChunk_demo=self.webpackChunk_demo||[]).push([[2],{}]);",""}
        };
        for(const auto& c:cases) {
            std::vector<Finding> f;identify_javascript("https://fixture.test/app.js",c.js,f);
            check(has(f,c.name,c.version),c.name);
            for(const auto& finding:f)for(const auto& evidence:finding.evidence)
                check(std::string(c.js).find(evidence.value)!=std::string::npos,"runtime evidence is from the actual bytes");
            f.clear();identify_javascript("example.js","/* documentation: "+std::string(c.js)+" */",f);
            check(!has(f,c.name),"runtime sample inside a comment must not match");
            f.clear();identify_http(HttpObservation{.url="https://fixture.test/",.body="<p>"+std::string(c.js)+"</p>"},f);
            check(!has(f,c.name),"runtime sample in ordinary page prose must not match");
        }
        std::vector<Finding> f;
        identify_javascript("large-bundle.js",std::string(600*1024,' ')+"Symbol.for('react.element');Symbol.for('react.fragment');",f);
        check(has(f,"React"),"browser script signatures beyond 512 KiB are analyzed");
        f.clear();identify_javascript("too-large.js",std::string(2*1024*1024,' ')+"Symbol.for('react.element');Symbol.for('react.fragment');",f);
        check(f.empty(),"2 MiB analysis boundary remains enforced");
        HttpObservation edge;edge.url="https://fixture.test/";edge.headers={{"x-served-by","cache-iad-kjyo7100031-IAD, cache-dfw-kdfw8210022-DFW"}};
        identify_http(edge,f);check(has(f,"Fastly"),"Fastly delivery node header identifies edge");
        f.clear();edge.headers={{"x-served-by","Fastly is mentioned in this unrelated text"}};identify_http(edge,f);
        check(!has(f,"Fastly"),"ordinary header prose does not identify Fastly");
        f.clear();
        identify_javascript("bundle.js","Symbol.for('react.element');Symbol.for('react.fragment');other.version='9.9.9';",f);
        check(has(f,"React")&&!has(f,"React","9.9.9"),"unrelated version is never assigned to React");
        check(f.front().version.empty(),"React version stays unknown when not advertised");
        f.clear();identify_javascript("doc.js",R"(const prose="react.element and react.fragment"; const example="Polymer.version='9.9.9'";)",f);
        check(f.empty(),"quoted runtime examples are not library implementations");
        f.clear();identify_javascript("doc.js",R"(const pattern=/Polymer.version='9.9.9'/;)",f);
        check(f.empty(),"regular expression literals are not runtime assignments");
        f.clear();identify_javascript("bundle.js","(e.litHtmlVersions=[]).push('3.3.1'),other.push('9.9.9');",f);
        check(has(f,"lit-html","3.3.1")&&!has(f,"lit-html","9.9.9"),"Lit version belongs to its own registration");
        f.clear();identify_javascript("bundle.js","polymer.version='9.9.9';",f);
        check(f.empty(),"JavaScript runtime names are case-sensitive");
        const std::string hydration=R"((self.$R=self.$R||{})["tsr"]=[];self.$_TSR={h(){},e(){},buffer:[]};$_TSR.router={manifest:void 0,matches:[]};)";
        f.clear();identify_javascript("hydration.js",hydration,f);
        check(has(f,"TanStack Router")&&!has(f,"React"),"paired TanStack SSR bootstrap identifies Router without inferring React");
        for(const auto& misleading: {std::string("/*")+hydration+"*/",std::string("const example=`")+hydration+"`;",
             std::string("self.$_TSR={};"),std::string("$_TSR.router={};")}) {
            f.clear();identify_javascript("example.js",misleading,f);
            check(!has(f,"TanStack Router"),"comments, quoted examples and single TanStack markers do not identify Router");
        }
        f.clear();identify_javascript("bundle.js",std::string(300000,' ')+"/*! jQuery v3.7.1 */",f);
        check(has(f,"jQuery","3.7.1"),"scan beyond the old 256 KiB cutoff");
        f.clear();identify_javascript("bundle.js","/* @license Copyright The Closure Library Authors. */",f);
        check(has(f,"Google Closure Library"),"named Closure license banner");
        HttpObservation h;h.url="https://fixture.test/section/page";
        h.body=R"(<script src="/s/desktop/abc123/jsbin/desktop_polymer.vflset/desktop_polymer.js"></script><script src="/s/player/def456/player_ias.vflset/en_US/base.js"></script>)";
        f.clear();identify_http(h,f);
        check(has(f,"YouTube web client")&&has(f,"YouTube player"),"YouTube client and player are distinct components");
        check(!has(f,"Polymer"),"legacy bundle name alone does not assert Polymer runtime");
        for(const auto& match:f)if(match.category!="Fingerprint")check(match.version.empty(),"build hashes are not versions");
        h.body=R"(<script src="https://www.youtube.com/iframe_api"></script>)";f.clear();identify_http(h,f);
        check(has(f,"YouTube player"),"extensionless official player API reference");
        h.body=R"(<script src="https://www.youtube.com.evil.test/iframe_api"></script>)";f.clear();identify_http(h,f);
        check(!has(f,"YouTube player"),"deceptive player host rejected");
        h.body=R"(<script src="/unrelated.js?next=/s/player/def456/player_ias.vflset/en_US/base.js"></script>)";f.clear();identify_http(h,f);
        check(!has(f,"YouTube player"),"query strings are not component paths");
        const std::pair<const char*,const char*> assets[]={
            {"Polymer","/bower_components/polymer/polymer.html"},
            {"Web Components polyfills","/webcomponents-bundle.js"},
            {"Web Components polyfills","/webcomponents-all-noPatch.vflset/webcomponents-all-noPatch.js"},
            {"Web Animations polyfill","/web-animations-next-lite.min.vflset/web-animations-next-lite.min.js"},
            {"Fetch polyfill","/fetch-polyfill.vflset/fetch-polyfill.js"},
            {"Intersection Observer polyfill","/intersection-observer.min.vflset/intersection-observer.min.js"},
            {"SPF","/spf.vflset/spf.js"},
            {"SPF","/spf/2.4.0/spf.js"},
            {"YouTube web client","/s/_/ytmainappweb/_/js/k=ytmainappweb.kevlar_base.en_US.hash.es5.O/d=1/m=kevlar_base_module,kevlar_main_module"},
            {"ShadyDOM","/shadydom.min.js"},{"ShadyCSS","/shadycss.js"}
        };
        for(const auto& [name,path]:assets) {
            h.body="<script src='"+std::string(path)+"'></script>";f.clear();identify_http(h,f);check(has(f,name),name);
            h.body="<!-- "+h.body+" -->";f.clear();identify_http(h,f);check(!has(f,name),"commented asset is not a match");
        }
        const std::pair<const char*,const char*> resource_cases[]={
            {"Web Components polyfills","https://assets.test/webcomponents-all-noPatch.js"},
            {"Web Animations polyfill","https://assets.test/web-animations-next-lite.min.js"},
            {"Fetch polyfill","https://assets.test/fetch-polyfill.js"},
            {"Intersection Observer polyfill","https://assets.test/intersection-observer.min.js"},
            {"SPF","https://assets.test/spf.vflset/spf.js"},
            {"Google Cast","https://www.gstatic.com/cv/js/sender/v1/cast_sender.js"},
            {"Google Cast","https://www.gstatic.com/eureka/clank/157/cast_sender.js"}
        };
        for(const auto& [name,url]:resource_cases) {
            f.clear();identify_script_reference(std::string(url)+"?cache=hash","Browser request from fixture",f);
            check(has(f,name),"loaded library URL is identified without downloading another body");
            check(f.front().version.empty(),"asset hashes and SDK path numbers are not library versions");
            for(const auto& misleading:{std::string(url)+".txt",std::string(url)+"/example",std::string("https://assets.test/loader.js?example=")+url}) {
                f.clear();identify_script_reference(misleading,"reference",f);
                check(!has(f,name),"suffixes and query text do not identify the library");
            }
        }
        f.clear();identify_script_reference("https://www.gstatic.com.evil.test/cv/js/sender/v1/cast_sender.js","reference",f);
        check(!has(f,"Google Cast"),"lookalike SDK origin is not Google Cast");
        h.body=R"(<script src="https://cdn.jsdelivr.net/npm/@polymer/polymer@3.5.2/polymer-element.js"></script><script src="https://cdn.jsdelivr.net/npm/lit-html@3.3.1/lit-html.js"></script>)";
        f.clear();identify_http(h,f);check(has(f,"Polymer","3.5.2")&&has(f,"lit-html","3.3.1"),"versioned component references");
        h.body=R"(<!-- <script src="/jquery.js"></script> --><template><script src="/vue.js"></script></template><noscript><script src="/react.js"></script></noscript><textarea><script src="/bootstrap.js"></script></textarea><script data-src="/jquery.js"></script><script type="application/json">{"name":"jQuery v3.7.1"}</script>)";
        f.clear();identify_http(h,f);check(f.empty()&&page_script_references(h).empty(),"inactive markup and data attributes do not identify or fetch scripts");
        h.body=R"(<script src="/download?kind=js&amp;v=2#part"></script><script src="/a/../download?kind=js&amp;v=2"></script><script src="https://elsewhere.test/runtime"></script>)";
        auto refs=page_script_references(h),same=page_script_urls(h);
        check(refs.size()==2&&same.size()==1,"extensionless and external references retained, same-host fetch list separated");
        check(same.front()=="https://fixture.test/download?kind=js&v=2","URL entities, fragments and dot segments normalized");
        h.body=R"(<base href="/assets/"><script type="importmap">{"imports":{"engine":"./engine.mjs","ignored/":"./prefix/"},"scopes":{"/app/":{"utility":"../lib/tool.js"}},"ignored":"/not-a-script.js"}</script><link rel="modulepreload" href="./runtime"></link>)";
        refs=page_script_references(h);check(refs.size()==3,"structured import map plus preload, no arbitrary strings or prefix directories");
        check(std::find(refs.begin(),refs.end(),"https://fixture.test/assets/engine.mjs")!=refs.end(),"base href used for modules");
        const std::pair<const char*,const char*> bundled_cases[]={
            {"Tealium",R"(utag.cfg={template:"ut4.54."};data.tealium_library_name="utag.js";utag.loader.ready_q.forEach(run);)"},
            {"Swiper",R"(const defaults={wrapperClass:"swiper-wrapper",slideClass:"swiper-slide"};swiper.slidesPerViewDynamic();)"},
            {"Optimizely",R"(window.optimizely.initialized=true;const api={getActiveExperimentIds:function(){},getVariationMap:function(){}};)"}
        };
        for(const auto& [name,code]:bundled_cases) {
            f.clear();identify_javascript("https://fresh.example/renamed-bundle",code,f);
            check(has(f,name),"paired implementation markers identify a renamed bundle on an arbitrary host");
            const auto found=std::find_if(f.begin(),f.end(),[&](const auto& item){return item.name==name;});
            check(found->version.empty()&&found->evidence.size()==3,"all three markers retained without guessing a version");
            for(const auto& misleading:{"/* "+std::string(code)+" */", "const documentation="+nlohmann::json(code).dump()+";",
                                       "const template=`"+std::string(code)+"`;"}) {
                f.clear();identify_javascript("docs.js",misleading,f);
                check(!has(f,name),"comments quoted examples and template text cannot become a library");
            }
            std::string partial=code;
            const auto last=partial.find(name==std::string("Tealium")?"utag.loader":name==std::string("Swiper")?"swiper.slidesPerViewDynamic":"getVariationMap");
            partial.erase(last);
            f.clear();identify_javascript("partial.js",partial,f);
            check(!has(f,name),"incomplete marker pairs cannot identify a bundled library");
        }
        h.body="<script>"+std::string(70000,' ')+"Polymer.version='3.5.2';</script>";
        f.clear();identify_http(h,f);check(has(f,"Polymer","3.5.2"),"inline script beyond old 64 KiB limit");
        h.body=R"(<script>const html='<script src="/jquery.js">';</script>)";
        check(page_script_references(h).empty(),"script strings cannot create fake script tags");
        h.body=R"(<script>const text="</scripture><script src='/jquery.js'>";</script><link rel="preload
modulepreload" href="/module.js">)";
        refs=page_script_references(h);check(refs.size()==1&&refs.front().ends_with("/module.js"),"raw-text closing boundaries and whitespace-separated preload tokens");
        std::cout<<checks<<" library checks passed.\n";
    }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
}
