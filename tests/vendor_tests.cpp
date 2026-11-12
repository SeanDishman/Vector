#include "vector/model.hpp"
#include "vector/overview.hpp"
#include "tag_evidence_cases.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int checks=0;
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
ScanReport inspect(std::string body,std::vector<std::pair<std::string,std::string>> headers={}) {
    ScanReport r;r.target=parse_target("https://target.example/app");
    HttpObservation h;h.url=r.target.url();h.status=200;h.body=std::move(body);h.headers=std::move(headers);
    identify_vendors(h,r);return r;
}
bool has(const ScanReport& r,const std::string& name,const std::string& kind="",const std::string& level="") {
    return std::any_of(r.vendors.begin(),r.vendors.end(),[&](const auto& v){return v.name==name && (kind.empty()||v.relationship==kind) && (level.empty()||v.evidence_level==level);});
}
int main(int argc,char** argv){try{
    if(argc==4&&std::string(argv[1])=="--replay-html") {
        std::ifstream input(argv[2]);if(!input)throw std::runtime_error("Cannot open captured HTML");
        ScanReport replay;replay.target=parse_target(argv[3]);replay.status="offline_analysis";
        HttpObservation page;page.url=replay.target.url();page.status=200;
        page.body={std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};identify_vendors(page,replay);
        replay.notes={"Offline attribution of retained HTML only. No requests were sent; references do not prove responses or execution."};
        std::cout<<report_json(replay)<<'\n';return 0;
    }
    if(argc==3&&std::string(argv[1])=="--replay-browser") {
        std::ifstream input(argv[2]);if(!input)throw std::runtime_error("Cannot open captured report");
        const auto captured=nlohmann::json::parse(input);
        ScanReport replay;replay.target=parse_target("https://"+captured.at("target").at("host").get<std::string>()+"/");
        replay.status="offline_analysis";
        for(const auto& item:captured.at("browser").at("requests")) {
            const auto page=item.value("page","");
            const auto& pages=captured.at("browser").at("pages");
            if(std::any_of(pages.begin(),pages.end(),[&](const auto& p){return p.value("url","")==page&&p.value("purpose","")=="disclosure";}))continue;
            BrowserRequest request;request.page=page;request.url=item.at("url");request.status=item.value("status",0);
            request.method=item.value("method","");request.type=item.value("type","");
            identify_browser_vendor(request,replay);
        }
        replay.notes={"Offline attribution of saved endpoint browser responses only. No requests were sent; disclosure-purpose browser pages are excluded."};
        std::cout<<report_json(replay)<<'\n';return 0;
    }
    auto r=inspect(R"(<script src="https://js.stripe.com/v3/?key=SECRET"></script><script src="https://www.googletagmanager.com/gtm.js?id=SECRET"></script>)");
    check(has(r,"Stripe","Script","Page reference") && has(r,"Google tagging"),"direct vendor script references");
    check(report_json(r).find("SECRET")==std::string::npos,"destination credentials and identifiers omitted");
    check(r.vendors[0].capability.find("If loaded and executed")!=std::string::npos,"access conditional on script execution");
    check(r.vendors[0].evidence[0].source.ends_with("/app"),"source page retained so evidence can be located");
    {
        HttpObservation dom;dom.url=r.target.url();dom.status=200;
        dom.body=R"(<script src="https://js.stripe.com/v3/?key=SECRET"></script>)";
        identify_vendors(dom,r,"Rendered DOM / ");
        identify_vendors(dom,r,"Rendered DOM / ");
        const auto stripe=std::find_if(r.vendors.begin(),r.vendors.end(),[](const auto& v){return v.name=="Stripe";});
        check(r.vendors.size()==2&&stripe->evidence.size()==2,"rendered references merge with static references without duplicates");
        check(stripe->evidence_level=="Page reference"&&stripe->evidence.back().source=="Rendered DOM / "+dom.url,
            "rendered provenance preserves reference strength rather than claiming a response");
        identify_vendors(dom,r,"Rendered DOM (captured HTTP page) / ");
        check(stripe->evidence.back().source.starts_with("Rendered DOM (captured HTTP page) / "),"captured document provenance survives vendor collection");
        check(report_json(r).find("SECRET")==std::string::npos,"rendered references preserve destination redaction");
        ScanReport truncated;dom.body_truncated=true;identify_vendors(dom,truncated,"Rendered DOM / ");
        check(truncated.vendors_truncated,"rendered truncation remains explicit");
    }
    auto j=nlohmann::json::parse(report_json(r));
    check(j["vendors"][0]["access_status"]=="unverified" && j["vendor_coverage"]["account_access_verified"]==false,"exports do not claim account grants");
    r=inspect(R"(<script src="https://js.stripe.com.evil.example/a"></script><script src="https://notgoogletagmanager.com/a"></script><script src="/proxy?url=https://js.stripe.com"></script><a href="https://js.stripe.com">docs</a>)");
    check(!has(r,"Stripe") && !has(r,"Google tagging") && r.vendors.size()==2,"domain boundaries; query and links are not integrations");
    check(r.vendors[0].name=="Unattributed origin","unknown origins retained without invented owner");
    r=inspect(R"(<script src="https://JS.STRIPE.COM.:443/v3"></script><script src="//js.stripe.com/v3"></script>)");
    check(r.vendors.size()==1 && has(r,"Stripe"),"case terminal dot port and repeated references normalized");
    r=inspect(R"(<script src="/local.js"></script><script src="https://target.example:443/same.js"></script><script src="https://target.example:444/other.js"></script><script src="https://assets.target.example/other.js"></script>)");
    check(r.vendors.size()==2 && !has(r,"Stripe"),"same origin excluded; other origins do not imply different owners");
    r=inspect(R"(<!-- <script src="https://js.stripe.com"></script> --><textarea><script src="https://js.stripe.com"></script></textarea><template><script src="https://js.stripe.com"></script></template><script type="application/ld+json">{"url":"https://js.stripe.com"}</script><script src="/local.js">'https://js.stripe.com'</script>)");
    check(r.vendors.empty(),"comments inert HTML data scripts and ignored script children excluded");
    r=inspect(R"(<script>/* 'https://js.stripe.com' */ // 'https://cdn.segment.com'
        s.src='https://widget.intercom.io/widget/'+key; const sample="https://unknown.example/api";</script>)");
    check(has(r,"Intercom","Inline URL hint","Code hint") && !has(r,"Stripe") && !has(r,"Twilio Segment") && r.vendors.size()==2,"inline literals are weak hints; JS comments skipped");
    r=inspect(R"(<script>const misleading = '<script src="https://js.stripe.com">';</script><style>.x { content: '<script src="https://js.stripe.com">'; }</style>)");
    check(r.vendors.empty(),"fake markup inside raw text is not a page reference");
    r=inspect(R"(<base href="https://cdn.jsdelivr.net/npm/"><script src="pkg/index.js"></script><base href="https://js.stripe.com/"><link rel="preconnect" href="https://fonts.gstatic.com">)");
    check(has(r,"jsDelivr","Script") && has(r,"Google Fonts","Connection hint") && !has(r,"Stripe"),"first base URL and connection hints");
    r=inspect(R"(<script SRC='https://js&#46;stripe.com/v3/' integrity='sha384-test'></script><iframe src="https://widget.intercom.io/widget/a" sandbox></iframe><iframe src="https://js.stripe.com" srcdoc="local"></iframe>)");
    check(has(r,"Stripe","Script") && has(r,"Intercom","Embedded frame") && r.vendors.size()==2,"entities sandbox srcdoc and integrity semantics");
    check(r.vendors[0].evidence[0].value.find("integrity")!=std::string::npos && r.vendors[1].evidence[0].value.find("sandbox attribute present")!=std::string::npos,"visible controls preserved without claiming effective safety");
    r=inspect(R"(<form action='https://unknown.example/collect'></form><img src='https://www.google-analytics.com/pixel'><link rel=stylesheet href='https://fonts.googleapis.com/css'><link rel=modulepreload href='https://unpkg.com/pkg.js'>)");
    check(has(r,"Unattributed origin","Form destination") && has(r,"Google Analytics","Image / media") && has(r,"Google Fonts","Stylesheet") && has(r,"UNPKG","Resource preload"),"forms media styles and preloads have distinct capabilities");
    r=inspect("",{{"Content-Security-Policy","connect-src 'self' https://*.clarity.ms; script-src https://js.stripe.com 'nonce-abc' 'strict-dynamic'; frame-ancestors https://widget.intercom.io; base-uri https://cdn.segment.com"},{"Content-Security-Policy-Report-Only","script-src https://js.stripe.com"}});
    check(has(r,"Microsoft Clarity","Policy destination","CSP policy") && has(r,"Stripe","Policy destination","CSP report-only"),"enforced and report-only mentions separated");
    check(!has(r,"Intercom") && !has(r,"Twilio Segment") && r.vendors[0].domain=="*.clarity.ms","inbound frame ancestors excluded and wildcard preserved");
    check(r.vendors.size()==3,"same vendor in two policy modes is not collapsed");
    r=inspect("",{{"content-security-policy","default-src https: data: * 'none'; connect-src api.segment.io:*; report-uri /report"}});
    check(r.vendors.size()==1 && has(r,"Twilio Segment"),"scheme-only CSP sources excluded and host patterns recognized");
    r=inspect(R"(<meta http-equiv=Content-Security-Policy content="script-src https://js.stripe.com">)");
    check(has(r,"Stripe","Policy destination","CSP meta policy"),"meta policy remains policy evidence");
    r=inspect(R"({"example":"<script src='https://js.stripe.com'></script>"})",{{"content-type","application/json"}});
    check(r.vendors.empty(),"explicit JSON not parsed as HTML");
    r=inspect("",{{"cf-ray","trace"},{"server","cloudflare"},{"x-vercel-id","trace"}});
    check(has(r,"Cloudflare","Response handler") && has(r,"Vercel","Response handler") && r.vendors.size()==2 && r.vendors[0].evidence.size()==2,"response markers group provider evidence");
    check(r.vendors[0].confidence=="Medium","headers remain unverified provider attribution");
    r=inspect(R"(<script src="https://user:secret@js.stripe.com/a"></script><script src="data:text/javascript,x"></script><script src="file:///x"></script>)");
    check(r.vendors.empty(),"credentials and unsupported schemes rejected");
    r=inspect(R"(<form method="post"><input name="link" type="text" placeholder="youtube.com/watch?v=" required></form>)");
    check(has(r,"YouTube","Supported source","Form input"),"converter form identifies YouTube as a supported source");
    check(r.vendors[0].confidence=="Medium" && r.vendors[0].capability.find("no form was submitted")!=std::string::npos,
        "supported source keeps declared use separate from an observed connection");
    check(r.vendors[0].evidence[0].value.find("input[placeholder]")!=std::string::npos,
        "YouTube source retains the structured input evidence");
    r=inspect(R"(<form><input type="url" placeholder="https://www.youtube.com/watch?v=SECRET"></form>)");
    check(has(r,"YouTube") && report_json(r).find("SECRET")==std::string::npos,"source URL query values are not retained");
    r=inspect(R"(<p>YouTube to MP3</p><a href="https://youtube.com/watch?v=x">YouTube</a><input placeholder="youtube.com/watch?v=">
      <form><input placeholder="youtube.com.evil.test/watch?v="><input placeholder="notyoutube.com/watch?v=">
      <input placeholder="Use a youtube.com/watch?v= URL"><input type="hidden" placeholder="youtube.com/watch?v=">
      <input disabled placeholder="youtube.com/watch?v="><input readonly placeholder="youtube.com/watch?v=">
      <input placeholder="youtube.com"><input placeholder="youtube.com/channel?v=">
      <input placeholder="youtube.com/watch?other=1"><input placeholder="https://user:secret@youtube.com/watch?v="></form>
      <template><form><input placeholder="youtube.com/watch?v="></form></template>
      <noscript><form><input placeholder="youtube.com/watch?v="></form></noscript>
      <form></form><input placeholder="youtube.com/watch?v=">)");
    check(!has(r,"YouTube"),"prose links deceptive hosts inactive controls and inputs outside forms do not identify a source");
    r=inspect(R"(<iframe src="https://www.youtube-nocookie.com/embed/example"></iframe><script src="https://www.youtube.com/iframe_api"></script>)");
    check(has(r,"YouTube","Embedded frame","Page reference") && has(r,"YouTube","Script","Page reference"),
        "YouTube embeds and the player API are attributed");
    check(infrastructure_entries(r).size()==1 && infrastructure_entries(r)[0].name=="YouTube",
        "several references to one provider produce one infrastructure entry");
    auto player_report=r;
    player_report.findings={{"YouTube player","Library","","High",{}},{"YouTube web client","Platform","","High",{}}};
    check(infrastructure_entries(player_report).size()==1 && infrastructure_entries(player_report)[0].name=="YouTube",
        "player components and provider references share one YouTube overview entry");
    BrowserRequest request;request.page="https://target.example/app";request.url="https://www.youtube.com/iframe_api";
    request.status=200;request.type="Script";request.method="GET";identify_browser_vendor(request,r);
    const auto observed=infrastructure_entries(r);
    check(observed.size()==1 && observed[0].detail.find("Observed contact")!=std::string::npos &&
        r.vendors[observed[0].vendor].evidence_level=="Observed response","overview links the strongest observed provider evidence");
    r=inspect(R"(<form><input placeholder="youtube.com/watch?v="></form>)");
    r.findings={{"nginx","Server","1.24.0","High",{}},{"OpenSSH","Service","9.6p1","High",{}},
        {"Ubuntu OpenSSH package","Package","3ubuntu13.20","High",{}},{"Ubuntu Linux","OS hint","","Medium",{}},
        {"Header signature","Fingerprint","","Low",{}}};
    Service service;service.name="HTTP";service.protocol="tcp";service.port=80;service.state="open";r.services.push_back(service);
    const auto stack=infrastructure_entries(r);
    check(stack.size()==2 && stack[0].name=="nginx" && stack[1].name=="YouTube","overview separates the stack from protocol package and OS facts");
    check(stack[0].finding==0 && stack[1].vendor==0 && stack[0].detail=="Server","overview routes each stack item to its original evidence");
    check(r.findings.size()==5 && r.services.size()==1 && report_json(r).find("3ubuntu13.20")!=std::string::npos,
        "technical details and versions remain in the complete report");
    r=inspect(R"(<link rel="preconnect" href="https://www.youtube.com"><script>const hint='https://www.youtube.com/iframe_api';</script>)",
        {{"content-security-policy","frame-src https://www.youtube.com"}});
    check(has(r,"YouTube") && infrastructure_entries(r).empty(),"weak policy connection and code hints stay in full information");
    r=inspect(R"(<iframe src="https://www.youtube.com.evil.test/embed/example"></iframe>)");
    check(!has(r,"YouTube") && infrastructure_entries(r).empty(),"unknown and misleading domains stay out of the infrastructure list");
    r=inspect("",{{"cf-ray","trace"},{"server","cloudflare"}});
    r.findings={{"Cloudflare","Edge","","High",{}}};
    check(infrastructure_entries(r).size()==1 && infrastructure_entries(r)[0].finding==0,"server and provider evidence do not duplicate the same stack entry");
    const std::pair<const char*,const char*> attributed[]={
        {"Tealium","https://tags.tiqcdn.com/utag/account/main/prod/utag.js"},
        {"Optimizely","https://a123.cdn.optimizely.com/client_storage/a123.html"},
        {"Optimizely","https://logx.optimizely.com/v1/events"},
        {"Sentry","https://o123.ingest.sentry.io/api/456/envelope/"},
        {"Adobe Experience Cloud","https://dpm.demdex.net/id"}
    };
    for(const auto& [name,url]:attributed) {
        ScanReport response;response.target=parse_target("https://fresh.example/");
        BrowserRequest request;request.page=response.target.url();request.url=std::string(url)+"?visitor=SECRET";
        request.status=200;request.type="XHR";request.method="GET";identify_browser_vendor(request,response);
        check(has(response,name,"Network request","Observed response"),"observed service destination has documented provider attribution");
        check(infrastructure_entries(response).size()==1,"confirmed provider appears in the overview");
        check(report_json(response).find("SECRET")==std::string::npos&&response.vendors[0].capability.find("not account permissions")!=std::string::npos,
            "attribution preserves query redaction and does not imply account access");
        const auto slash=request.url.find('/',8);request.url.insert(slash,".evil.example");
        response.vendors.clear();identify_browser_vendor(request,response);
        check(!has(response,name),"suffix lookalikes cannot impersonate documented destinations");
        response.vendors.clear();request.url=url;request.status=0;identify_browser_vendor(request,response);
        check(response.vendors.empty(),"a failed request alone is not an observed provider response");
        r=inspect("",{{"content-security-policy",std::string("connect-src ")+url}});
        check(infrastructure_entries(r).empty(),"policy destinations remain outside the named overview");
    }
    r=inspect(R"(<script src="https://tags.tiqcdn.com/docs/utag.js"></script><script src="https://o123.ingest.sentry.io/docs/envelope/"></script>
        <a href="https://dpm.demdex.net/id">Adobe</a><p>Tealium Swiper Optimizely Sentry</p>)");
    check(!has(r,"Tealium")&&!has(r,"Sentry")&&!has(r,"Adobe Experience Cloud"),"wrong endpoint paths links and prose do not create integrations");
    const auto padding=std::string(600*1024,'x');
    const std::pair<const char*,const char*> integrations[]={
        {"Google Identity Services","https://accounts.google.com/gsi/client"},
        {"Contentful","https://images.ctfassets.net/space/asset/image.png"},
        {"Contentful","https://videos.ctfassets.net/space/asset/video.mp4"},
        {"Contentful","https://assets.ctfassets.net/space/asset/file.pdf"},
        {"Contentful","https://downloads.ctfassets.net/space/asset/file.zip"},
        {"Clearbit Reveal","https://reveal.clearbit.com/v1/companies/reveal"},
        {"Adobe Experience Platform Tags","https://assets.adobedtm.com/fresh/launch-fresh.min.js"},
        {"Marketo","https://munchkin.marketo.net/munchkin.js"},
        {"Reddit Ads","https://www.redditstatic.com/ads/pixel.js"},
        {"HubSpot Chat","https://js.usemessages.com/conversations-embed.js"},
        {"Meta Pixel","https://connect.facebook.net/en_US/fbevents.js"},
        {"Meta Pixel","https://connect.facebook.net/de_DE/fbevents.js"},
        {"Microsoft Advertising","https://bat.bing.com/bat.js"},
        {"X Ads","https://static.ads-twitter.com/uwt.js"},
        {"Hotjar","https://static.hotjar.com/c/hotjar-987654.js"},
        {"Contentsquare","https://t.contentsquare.net/uxa/fresh.js"},
        {"Contentsquare","https://t.contentsquare.net/releases/15.1/es2019/fresh.js"},
        {"Quora Pixel","https://a.quora.com/qevents.js"},
        {"Qualified","https://js.qualified.com/qualified.js"},
        {"Datadog","https://rum.browser-intake-datadoghq.com/api/v2/rum"},
        {"Sentry","https://o123.ingest.us.sentry.io/api/456/envelope/"},
        {"Sentry","https://o123.ingest.de.sentry.io/api/456/envelope/"},
        {"Sentry","https://o123.ingest.sentry.io/api/456/security/"},
        {"Sentry","https://o123.ingest.us.sentry.io/api/456/security/"},
        {"Sentry","https://o123.ingest.de.sentry.io/api/456/security/"},
        {"Vimeo","https://player.vimeo.com/video/123456"},
        {"Vimeo","https://player.vimeo.com/progressive_redirect/playback/123456/rendition/1080p/file.mp4"},
        {"CloudFront","https://d123example.cloudfront.net/media/asset.png"}
    };
    for(const auto& [name,url]:integrations) {
        r=inspect("<script src='"+std::string(url)+"?token=SECRET'></script>");
        check(has(r,name,"Script","Page reference"),"known integration is attributed from an active element");
        check(infrastructure_entries(r).size()==1&&report_json(r).find("SECRET")==std::string::npos,"integration keeps evidence and query redaction");
        r=inspect("<template><script src='"+std::string(url)+"'></script></template><a href='"+url+"'>Example</a>");
        check(r.vendors.empty(),"inert and ordinary links cannot invent an integration");
        r=inspect("",{{"content-security-policy",std::string("script-src ")+url}});
        check(infrastructure_entries(r).empty(),"policy hints alone are not infrastructure");
        BrowserRequest hit;hit.url=url;hit.page="https://fresh.example/";hit.status=200;hit.method="GET";hit.type="Script";
        ScanReport response;identify_browser_vendor(hit,response);
        check(has(response,name,"Script","Observed response"),"successful integration response is attributed");
        check(!response.vendors[0].attribution_source.empty(),"integration attribution retains its provider documentation");
        response.vendors.clear();hit.status=0;identify_browser_vendor(hit,response);
        check(response.vendors.empty(),"a failed integration request is not an observed response");
        hit.status=200;hit.url.insert(hit.url.find('/',8),".evil.test");identify_browser_vendor(hit,response);
        check(!has(response,name),"integration hostname suffix lookalikes remain unknown");
    }
    r=inspect(R"(<script src='https://accounts.google.com/ServiceLogin'></script><script src='https://reveal.clearbit.com/v1/docs'></script>
        <script src='https://assets.adobedtm.com/index.html'></script><script src='https://munchkin.marketo.net/munchkin.js.map'></script>
        <script src='https://www.redditstatic.com/ads/pixel.js.example'></script><script src='https://js.usemessages.com/docs'></script>)");
    check(std::none_of(r.vendors.begin(),r.vendors.end(),[](const auto& v){return v.confidence!="Unknown";}),"other paths on shared hosts do not match narrow service routes");
    r=inspect(R"(<script src='https://connect.facebook.net/en_US/sdk.js'></script>)");
    check(has(r,"Facebook SDK","Script","Page reference")&&!has(r,"Meta Pixel"),"the Facebook SDK script is named as the SDK, never as the pixel");
    r=inspect(R"(<script src='https://connect.facebook.net/en_US/sdk.js.map'></script><script src='https://bat.bing.com/bat.js.map'></script>
        <script src='https://static.ads-twitter.com/docs/uwt.js'></script><script src='https://static.hotjar.com/help/example.js'></script>
        <script src='https://t.contentsquare.net/about.js'></script><script src='https://a.quora.com/qevents.js.map'></script>
        <script src='https://js.qualified.com/qualified.js.map'></script><script src='https://rum.browser-intake-datadoghq.com/api/v2/rum/example'></script>
        <iframe src='https://player.vimeo.com/about'></iframe><script src='https://o123.ingest.us.sentry.io/docs/envelope/'></script>)");
    check(std::none_of(r.vendors.begin(),r.vendors.end(),[](const auto& v){return v.confidence!="Unknown";}),"unrelated resources do not become pixels or monitoring endpoints");
    r=inspect(R"(<iframe src='https://player.vimeo.com/video/123456'></iframe><img src='https://d123example.cloudfront.net/image.png'>)");
    check(has(r,"Vimeo","Embedded frame","Page reference")&&has(r,"CloudFront","Image / media","Page reference"),"video and delivery resources keep their actual roles");
    r.findings={{"CloudFront","Edge / CDN","","High",{{"Response header","Via: CloudFront"}}}};
    check(infrastructure_entries(r).size()==2,"delivery attribution coalesces with existing CloudFront evidence");
    r.findings.push_back({"Amazon CloudFront","DNS hosting alias","","High",{{"CNAME","d123example.cloudfront.net"}}});
    const auto coalesced=infrastructure_entries(r);
    check(coalesced.size()==2&&std::count_if(coalesced.begin(),coalesced.end(),[](const auto& row){return row.name=="CloudFront";})==1,
        "CloudFront DNS and delivery labels are one overview product");
    check(r.findings.size()==2&&report_json(r).find("Amazon CloudFront")!=std::string::npos,"alias coalescing preserves underlying DNS evidence");
    r=inspect("<div>"+padding+"</div><script src='https://js.stripe.com/v3/'></script>");
    check(has(r,"Stripe","Script","Page reference")&&!r.vendors_truncated,"late references in retained large markup are analyzed");
    r=inspect("<div>"+padding+"</div><template><script src='https://js.stripe.com/v3/'></script></template><p>Stripe YouTube</p>");
    check(r.vendors.empty()&&!r.vendors_truncated,"large-document prose and inert markup remain excluded");
    r=inspect("<div>"+std::string(3*1024*1024,'x')+"</div><script src='https://js.stripe.com/v3/'></script>");
    check(r.vendors.empty()&&r.vendors_truncated,"markup beyond the retained DOM bound is explicitly partial");
    std::string many;for(int i=0;i<400;++i)many+="<script src='https://vendor"+std::to_string(i)+".example/a'></script>";
    r=inspect(many);check(r.vendors.size()==256 && r.vendors_truncated,"relationship count bounded and partial coverage exported");
    r=inspect("<script src='https://js.stripe.com");check(r.vendors.empty(),"incomplete tags do not create evidence");
    auto demo=demo_report();check(has(demo,"Stripe") && has(demo,"Intercom") && has(demo,"Unattributed origin"),"demo covers vendor relationships");
    tag_evidence::run(check);
    std::cout<<checks<<" vendor checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
