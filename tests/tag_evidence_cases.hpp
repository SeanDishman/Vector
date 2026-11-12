#pragma once
#include "vector/model.hpp"
#include "vector/overview.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace tag_evidence {
using namespace vectorapp;

inline BrowserRequest failed_request(const std::string& url,const std::string& error,const std::string& type="Script") {
    BrowserRequest request;request.page="https://fresh.example/";request.url=url;request.type=type;request.method="GET";
    request.status=0;request.state="failed";request.error=error;return request;
}
inline bool row(const ScanReport& r,const std::string& name,const std::string& kind,const std::string& level) {
    return std::any_of(r.vendors.begin(),r.vendors.end(),[&](const auto& v){return v.name==name&&v.relationship==kind&&v.evidence_level==level;});
}
inline bool named(const std::vector<Finding>& findings,const std::string& name,const std::string& category) {
    return std::any_of(findings.begin(),findings.end(),[&](const Finding& f){return f.name==name&&f.category==category;});
}
inline std::vector<Finding> script(const std::string& body) {
    std::vector<Finding> findings;identify_javascript("https://unrelated.example/assets/app.js",body,findings);return findings;
}

template<class Check> void run(Check check) {
    ScanReport r;
    identify_browser_vendor(failed_request("https://www.googletagmanager.com/gtm.js?id=GTM-SECRET","net::ERR_ADDRESS_INVALID"),r);
    check(row(r,"Google tagging","Script","Request attempt")&&r.vendors.size()==1,"an unreached request to a documented provider is kept as a request attempt");
    auto overview=infrastructure_entries(r);
    check(overview.size()==1&&overview[0].name=="Google tagging"&&overview[0].detail=="Tag management / Requested, no response",
        "a request attempt reaches the overview with its own basis");
    check(report_json(r).find("SECRET")==std::string::npos&&r.vendors[0].capability.find("Nothing was received")!=std::string::npos&&
        r.vendors[0].evidence[0].value.find("no response / net::ERR_ADDRESS_INVALID")!=std::string::npos,
        "a request attempt keeps the failure, omits the query and claims no contact");
    check(std::none_of(r.vendors.begin(),r.vendors.end(),[](const auto& v){return v.evidence_level=="Observed response";}),
        "a request attempt is never an observed response");
    auto answered=failed_request("https://www.googletagmanager.com/gtm.js","");answered.status=200;answered.state="complete";
    identify_browser_vendor(answered,r);overview=infrastructure_entries(r);
    check(overview.size()==1&&overview[0].detail=="Tag management / Observed contact","an observed response outranks a request attempt for the same provider");

    for(const char* error:{"","net::ERR_BLOCKED_BY_CLIENT","net::ERR_BLOCKED_BY_CSP","net::ERR_BLOCKED_BY_ORB","net::ERR_ABORTED","net::ERR_FAILED","ERR_ADDRESS_INVALID"}) {
        ScanReport none;identify_browser_vendor(failed_request("https://www.googletagmanager.com/gtm.js",error),none);
        check(none.vendors.empty(),"policy blocks, aborts and unexplained failures are not request attempts");
    }
    for(const char* state:{"pending","interrupted","blocked","cancelled","response","complete"}) {
        auto request=failed_request("https://www.googletagmanager.com/gtm.js","net::ERR_ADDRESS_INVALID");request.state=state;
        ScanReport none;identify_browser_vendor(request,none);
        check(none.vendors.empty(),"only a failed load is a request attempt");
    }
    {
        auto supplied=failed_request("https://www.googletagmanager.com/gtm.js","net::ERR_ADDRESS_INVALID");supplied.collection="captured_http";
        ScanReport none;identify_browser_vendor(supplied,none);
        check(none.vendors.empty(),"supplied documents are not network attempts");
        identify_browser_vendor(failed_request("https://unknown-tracker.example/t.js","net::ERR_ADDRESS_INVALID"),none);
        identify_browser_vendor(failed_request("https://www.googletagmanager.com.evil.test/gtm.js","net::ERR_NAME_NOT_RESOLVED"),none);
        identify_browser_vendor(failed_request("https://fresh.example/gtm.js","net::ERR_CONNECTION_REFUSED"),none);
        check(none.vendors.empty(),"unknown, lookalike and same-origin failures add no provider");
    }

    const std::pair<const char*,const char*> attributed[]={
        {"Google Analytics","https://analytics.google.com/g/collect"},{"Google Analytics","https://analytics.google.com/g/s/collect"},
        {"Google Analytics","https://stats.g.doubleclick.net/g/collect"},
        {"Google Ads","https://googleads.g.doubleclick.net/pagead/viewthroughconversion/123456789/"},
        {"Google Ads","https://www.googleadservices.com/pagead/conversion/123456789/"},
        {"DoubleClick Floodlight","https://ad.doubleclick.net/activity;src=1234567;type=a;cat=b"},
        {"DoubleClick Floodlight","https://ad.doubleclick.net/ddm/activity/src=1234567"},
        {"Google AdSense","https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js"},
        {"Google Publisher Tag","https://securepubads.g.doubleclick.net/tag/js/gpt.js"},
        {"Google Publisher Tag","https://www.googletagservices.com/tag/js/gpt.js"},
        {"Google Maps","https://maps.googleapis.com/maps/api/js"},
        {"TikTok Pixel","https://analytics.tiktok.com/i18n/pixel/events.js"},
        {"LinkedIn Insight Tag","https://px.ads.linkedin.com/collect"},{"LinkedIn Insight Tag","https://px.ads.linkedin.com/wa/"},
        {"X Ads","https://analytics.twitter.com/i/adsct"},{"Meta Pixel","https://www.facebook.com/tr"},{"Meta Pixel","https://www.facebook.com/tr/"},
        {"Facebook SDK","https://connect.facebook.net/en_US/sdk.js"},{"Pinterest Tag","https://s.pinimg.com/ct/core.js"},
        {"Snap Pixel","https://sc-static.net/scevent.min.js"},{"Microsoft Advertising","https://bat.bing.com/action/0"},
        {"Amplitude","https://cdn.amplitude.com/libs/analytics-browser-2.11.1-min.js.gz"},{"Amplitude","https://api2.amplitude.com/2/httpapi"},
        {"Mixpanel","https://cdn.mxpnl.com/libs/mixpanel-2-latest.min.js"},{"Heap","https://cdn.heapanalytics.com/js/heap-1234567.js"},
        {"Fullstory","https://edge.fullstory.com/s/fs.js"},{"LogRocket","https://cdn.logrocket.io/LogRocket.min.js"},
        {"New Relic","https://js-agent.newrelic.com/nr-loader-spa-1.270.0.min.js"},{"New Relic","https://bam.nr-data.net/1/abcdef"},
        {"Chartbeat","https://static.chartbeat.com/js/chartbeat.js"},{"Comscore","https://sb.scorecardresearch.com/beacon.js"},
        {"Taboola","https://cdn.taboola.com/libtrc/publisher/loader.js"},{"Outbrain","https://widgets.outbrain.com/outbrain.js"},
        {"The Trade Desk","https://js.adsrvr.org/up_loader.1.1.0.js"},{"The Trade Desk","https://js.adsrvr.org/conversionEvents-v1.min.js"},
        {"WordPress.com Stats","https://stats.wp.com/e-202646.js"},{"Branch","https://cdn.branch.io/branch-latest.min.js"},
        {"Sentry","https://browser.sentry-cdn.com/8.0.0/bundle.min.js"},{"Bugsnag","https://sessions.bugsnag.com/"},
        {"Rollbar","https://api.rollbar.com/api/1/item/"},{"HubSpot","https://js.hsforms.net/forms/embed/v2.js"},
        {"Drift","https://js.driftt.com/include/1700000000000/abc123.js"},{"Crisp","https://client.crisp.chat/l.js"},
        {"Zendesk","https://static.zdassets.com/ekr/snippet.js"},{"tawk.to","https://embed.tawk.to/abc/default"},
        {"LiveChat","https://cdn.livechatinc.com/tracking.js"},{"Cookiebot","https://consent.cookiebot.com/uc.js"},
        {"CookieYes","https://cdn-cookieyes.com/client_data/abc123/script.js"},{"Usercentrics","https://app.usercentrics.eu/browser-ui/latest/loader.js"},
        {"TrustArc","https://consent.trustarc.com/notice"},{"Osano","https://cmp.osano.com/abc/def/osano.js"},
        {"Didomi","https://sdk.privacy-center.org/abc/loader.js"},{"iubenda","https://cdn.iubenda.com/cs/iubenda_cs.js"},
        {"Termly","https://app.termly.io/resource-blocker/abc"},{"LaunchDarkly","https://app.launchdarkly.com/sdk/evalx/abc/contexts/def"},
        {"PayPal","https://www.paypal.com/sdk/js"},{"Braintree","https://js.braintreegateway.com/web/3.100.0/js/client.min.js"},
        {"Klarna","https://js.klarna.com/web-sdk/v1/klarna.js"},{"Apple Pay","https://applepay.cdn-apple.com/jsapi/v1/apple-pay-sdk.js"},
        {"Google Pay","https://pay.google.com/gp/p/js/pay.js"},{"Plaid","https://cdn.plaid.com/link/v2/stable/link-initialize.js"},
        {"Sign in with Apple","https://appleid.cdn-apple.com/appleauth/static/jsapi/appleid/1/en_US/appleid.auth.js"},
        {"Auth0","https://cdn.auth0.com/js/auth0-spa-js/2.1/auth0-spa-js.production.js"},
        {"Arkose Labs","https://client-api.arkoselabs.com/v2/ABCDEF12-3456/api.js"},{"DataDome","https://js.datadome.co/tags.js"},
        {"Fingerprint","https://fpjscdn.net/v3/abc123"},{"Sift","https://cdn.sift.com/s.js"},
        {"Wistia","https://fast.wistia.com/assets/external/E-v1.js"},{"JW Player","https://cdn.jwplayer.com/libraries/abc123.js"},
        {"Brightcove","https://players.brightcove.net/123/default_default/index.min.js"},{"Mux","https://stream.mux.com/abc.m3u8"},
        {"X widgets","https://platform.twitter.com/widgets.js"},{"Calendly","https://assets.calendly.com/assets/external/widget.js"},
        {"Typeform","https://embed.typeform.com/next/embed.js"},{"Cloudinary","https://res.cloudinary.com/demo/image/upload/sample.jpg"},
        {"imgix","https://assets.imgix.net/examples/kingfisher.jpg"},{"Shopify","https://cdn.shopify.com/s/files/1/0001/theme.js"},
        {"Adobe Fonts","https://use.typekit.net/abc1def.css"},{"Font Awesome","https://kit.fontawesome.com/abc123.js"},
        {"jQuery CDN","https://code.jquery.com/jquery-3.7.1.min.js"},{"Google Cloud Storage","https://storage.googleapis.com/bucket/app.js"},
        {"Azure Blob Storage","https://account.blob.core.windows.net/container/app.js"},
        {"Klaviyo","https://static.klaviyo.com/onsite/js/klaviyo.js"},{"Mailchimp","https://chimpstatic.com/mcjs-connected/js/users/abc/def.js"},
        {"Customer.io","https://assets.customer.io/assets/track.js"},{"Braze","https://js.appboycdn.com/web-sdk/5.5/braze.min.js"},
        {"Pendo","https://cdn.pendo.io/agent/static/abc-123/pendo.js"},{"Appcues","https://fast.appcues.com/12345.js"},
        {"OneSignal","https://cdn.onesignal.com/sdks/web/v16/OneSignalSDK.page.js"},
        {"Salesforce Account Engagement","https://pi.pardot.com/pd.js"},{"Demandbase","https://tag.demandbase.com/abc123.min.js"},
        {"Algolia","https://abc123-dsn.algolia.net/1/indexes/*/queries"},{"Mapbox","https://api.mapbox.com/mapbox-gl-js/v3.0.0/mapbox-gl.js"}
    };
    for(const auto& [name,url]:attributed) {
        ScanReport response;
        auto request=failed_request(std::string(url)+"?token=SECRET","");request.status=200;request.state="complete";
        identify_browser_vendor(request,response);
        check(response.vendors.size()==1&&response.vendors[0].name==name&&response.vendors[0].evidence_level=="Observed response"&&
            !response.vendors[0].attribution_source.empty(),"documented service endpoint is attributed with its source");
        check(infrastructure_entries(response).size()==1&&report_json(response).find("SECRET")==std::string::npos,
            "attributed endpoint reaches the overview without its query");
        ScanReport attempt;identify_browser_vendor(failed_request(url,"net::ERR_NAME_NOT_RESOLVED"),attempt);
        check(attempt.vendors.size()==1&&attempt.vendors[0].name==name&&attempt.vendors[0].evidence_level=="Request attempt",
            "the same endpoint is attributed when the request was not answered");
        check(attempt.vendors[0].evidence[0].value.find(';')==std::string::npos,"a request attempt omits path parameters");
        ScanReport lookalike;request.url=url;request.url.insert(request.url.find('/',8),".evil.test");
        identify_browser_vendor(request,lookalike);
        check(lookalike.vendors.size()==1&&lookalike.vendors[0].name=="Unattributed origin","a suffix lookalike host stays unattributed");
    }
    const std::pair<const char*,const char*> other_paths[]={
        {"Google Analytics","https://analytics.google.com/analytics/web/"},{"Meta Pixel","https://www.facebook.com/trends"},
        {"Meta Pixel","https://www.facebook.com/some.page"},{"DoubleClick Floodlight","https://ad.doubleclick.net/ccm/s/collect"},
        {"PayPal","https://www.paypal.com/checkoutnow"},{"Facebook SDK","https://connect.facebook.net/en_US/other.js"},
        {"Google Ads","https://www.googleadservices.com/other"},{"Google Maps","https://maps.googleapis.com/maps/api/staticmap"},
        {"X widgets","https://platform.twitter.com/embed/Tweet.html"},{"Zendesk","https://static.zdassets.com/other.js"}
    };
    for(const auto& [name,url]:other_paths) {
        ScanReport response;auto request=failed_request(url,"");request.status=200;request.state="complete";
        identify_browser_vendor(request,response);
        check(std::none_of(response.vendors.begin(),response.vendors.end(),[&](const auto& v){return v.name==name;}),
            "another path on a shared host is not attributed to the service");
    }

    auto found=script("window.dataLayer=window.dataLayer||[];function gtag(){dataLayer.push(arguments);}gtag('js',new Date());"
        "gtag('config','G-AB12CD34EF',{send_page_view:false});gtag(\"config\", \"AW-123456789\");gtag('config','DC-1234567');");
    check(named(found,"Google Analytics","Analytics")&&named(found,"Google Ads","Advertising")&&
        named(found,"DoubleClick Floodlight","Advertising measurement"),"configured Google tag destinations are named from the page's own calls");
    const auto analytics=std::find_if(found.begin(),found.end(),[](const Finding& f){return f.name=="Google Analytics";});
    check(analytics->evidence.size()==2&&analytics->evidence[0].value=="gtag('config','G-AB12CD34EF'"&&analytics->confidence=="Medium",
        "a configured destination keeps the matched call and the queue definition");
    for(const char* body:{
        "function gtag(){dataLayer.push(arguments);}const sample=\"gtag('config','G-AB12CD34EF')\";",
        "function gtag(){dataLayer.push(arguments);}/* gtag('config','G-AB12CD34EF'); */",
        "gtag('config','G-AB12CD34EF');",
        "const note='dataLayer.push(arguments)';gtag('config','G-AB12CD34EF');",
        "function gtag(){dataLayer.push(arguments);}mygtag('config','G-AB12CD34EF');other.gtag('config','G-AB12CD34EF');",
        "function gtag(){dataLayer.push(arguments);}gtag('config','G-ab12');gtag('config','UA-123456-1');gtag('config','AW-12AB56789');gtag('event','G-AB12CD34EF');",
        "function gtag(){dataLayer.push(arguments);}gtag('config',measurementId);"})
        check(script(body).empty(),"quoted, commented, unpaired, foreign and malformed Google tag calls identify nothing");

    const std::string container_body="\n// Copyright 2012 Google Inc. All rights reserved.\n(function(){\n\nvar data = {\n\"resource\": {\n"
        "  \"version\":\"7\",\n  \"macros\":[{\"function\":\"__c\",\"vtp_value\":\"G-AB12CD34EF\"},{\"function\":\"__e\"}],\n"
        "  \"tags\":[{\"function\":\"__googtag\",\"vtp_tagId\":[\"macro\",0],\"tag_id\":3},"
        "{\"function\":\"__awct\",\"vtp_conversionId\":\"123456789\",\"tag_id\":4},"
        "{\"function\":\"__html\",\"vtp_html\":\"\\u003Cscript\\u003E!function(f,b,e,v){}(window,document,'script','https:\\/\\/connect.facebook.net\\/en_US\\/fbevents.js');"
        "fbq('init','1');\\u003C\\/script\\u003E\",\"vtp_supportDocumentWrite\":false,\"tag_id\":5},"
        "{\"function\":\"__html\",\"vtp_html\":\"\\u003Cscript src=\\\"\\/\\/static.hotjar.com\\/c\\/hotjar-123.js?sv=6\\\"\\u003E\\u003C\\/script\\u003E\",\"tag_id\":6},"
        "{\"function\":\"__html\",\"vtp_html\":\"\\u003Cscript src='https:\\/\\/connect.facebook.net.evil.test\\/en_US\\/sdk.js'\\u003E\\u003C\\/script\\u003E\",\"tag_id\":7},"
        "{\"function\":\"__html\",\"vtp_html\":\"\\u003Cscript src='https:\\/\\/d1abcdefgh.cloudfront.net\\/tag.js'\\u003E\\u003C\\/script\\u003E"
        "\\u003Clink href='https:\\/\\/fonts.googleapis.com\\/css2' rel='stylesheet'\\u003E\",\"tag_id\":9},"
        "{\"function\":\"__fls\",\"vtp_groupTag\":\"a\",\"tag_id\":8}],\n"
        "  \"predicates\":[{\"function\":\"_eq\",\"arg0\":[\"macro\",1],\"arg1\":\"gtm.js\"},{\"function\":\"__sp\",\"vtp_x\":\"DC-7654321\"}],\n  \"rules\":[]\n}\n};\n})();";
    found=script(container_body);
    check(named(found,"Google Analytics","Analytics")&&named(found,"Google Ads","Advertising")&&named(found,"Meta Pixel","Advertising")&&
        named(found,"Hotjar","Analytics")&&named(found,"DoubleClick Floodlight","Advertising measurement")&&found.size()==5,
        "a Google tag container names its configured destinations, built-in tags and custom tag resources");
    check(!named(found,"Facebook SDK","Social / identity"),"a lookalike host inside a custom tag stays unnamed");
    check(!named(found,"CloudFront","Edge / CDN")&&!named(found,"Google Fonts","Fonts"),"a custom tag does not name the delivery host its file sits on");
    const auto pixel=std::find_if(found.begin(),found.end(),[](const Finding& f){return f.name=="Meta Pixel";});
    check(pixel->evidence[0].value.find("custom tag -> https://connect.facebook.net/en_US/fbevents.js")!=std::string::npos&&
        pixel->evidence[0].value.find("firing not observed")!=std::string::npos,"a container entry says it is configuration, not an observed request");
    const auto hotjar=std::find_if(found.begin(),found.end(),[](const Finding& f){return f.name=="Hotjar";});
    check(hotjar->evidence[0].value.find("sv=6")==std::string::npos,"a custom tag resource omits its query");
    std::string quoted_container="const sample='";
    for(const char c:container_body)if(c!='\''&&c!='\n'&&c!='\\')quoted_container+=c;
    quoted_container+="';";
    check(script(quoted_container).empty(),"a container quoted as text identifies nothing");
    check(script("var info = {\"resource\": {\"version\":\"7\",\"macros\":[],\"tags\":[{\"function\":\"__awct\"}],\"predicates\":[]}};").empty()&&
        script("var data = {\"resource\": {\"name\":\"x\",\"tags\":[{\"function\":\"__awct\",\"vtp_id\":\"G-AB12CD34EF\"}]}};").empty()&&
        script("var data = {\"resource\": {\"version\":\"7\",\"macros\":[],\"tags\":[{\"function\":\"__unknown\",\"other_id\":\"G-AB12CD34EF\",\"vtp_id\":\"G-12\"}],\"predicates\":[]}};").empty(),
        "other objects, missing envelope fields and unknown entries identify nothing");

    found=script("const l=new Promise(t=>{window.gtag(\"get\",\"G-5MVYWBPCBE\",e,e=>{t(e??null)})});globalThis.gtag('config',`AW-123456789`);");
    check(named(found,"Google Analytics","Analytics")&&named(found,"Google Ads","Advertising")&&found.size()==2,
        "a Google tag call on window names its destination without the queue definition");
    check(found[0].evidence.size()==1&&found[0].evidence[0].value=="window.gtag(\"get\",\"G-5MVYWBPCBE\"","a qualified call keeps the call as its evidence");
    for(const char* body:{
        "x.window.gtag(\"get\",\"G-5MVYWBPCBE\");", "window.gtag(\"event\",\"G-5MVYWBPCBE\");", "window.gtag(\"get\",id);",
        "window.gtag(\"get\",\"G-ABCDEFGHIJ\");", "const s='window.gtag(\"get\",\"G-5MVYWBPCBE\")';", "mywindow.gtag(\"get\",\"G-5MVYWBPCBE\");"})
        check(script(body).empty(),"foreign owners, other verbs, variables, placeholders and quoted calls identify nothing");

    for(const char* body:{
        "_linkedin_partner_id = \"1234567\";window._linkedin_data_partner_ids = window._linkedin_data_partner_ids || [];window._linkedin_data_partner_ids.push(_linkedin_partner_id);",
        "Object.assign(window,{_linkedin_partner_id:`7340812`,_linkedin_data_partner_ids:[`7340812`]});let n=document.createElement(`script`);"}) {
        found=script(body);
        check(named(found,"LinkedIn Insight Tag","Advertising")&&found.size()==1&&found[0].evidence.size()==2,
            "a LinkedIn partner ID set next to the partner list names the Insight Tag");
    }
    for(const char* body:{
        "_linkedin_partner_id = \"1234567\";", "if(_linkedin_partner_id==\"1234567\")window._linkedin_data_partner_ids.push(1);",
        "my_linkedin_partner_id=\"1234567\";window._linkedin_data_partner_ids=[];", "_linkedin_partner_id=\"abc\";window._linkedin_data_partner_ids=[];",
        "const s='_linkedin_partner_id = \"1234567\";window._linkedin_data_partner_ids=[]';",
        "n._linkedin_data_partner_id&&t.push(n._linkedin_data_partner_id);n._linkedin_data_partner_ids;"})
        check(script(body).empty(),"an unpaired, compared, foreign, non-numeric or quoted partner ID identifies nothing");

    found=script("window.__STATE__={\"config\":{\"googleAdsCode\":\"AW-17106321204\",\"googleAnalyticsCode\":\"G-7JY7T788PK\",\"title\":\"x\"}};");
    check(named(found,"Google Ads","Advertising")&&named(found,"Google Analytics","Analytics")&&found.size()==2,
        "Google destination IDs stored under named configuration keys are named");
    const auto stored=std::find_if(found.begin(),found.end(),[](const Finding& f){return f.name=="Google Analytics";});
    check(stored->evidence[0].value=="configuration value googleAnalyticsCode\":\"G-7JY7T788PK\"","a configuration value keeps its key and value");
    check(named(script("const env={NEXT_PUBLIC_FIREBASE_MEASUREMENT_ID:\"G-82RG1PXYVW\",OTHER:1};"),"Google Analytics","Analytics")&&
        named(script("initializeApp({apiKey:k, measurementId: 'G-82RG1PXYVW'});"),"Google Analytics","Analytics"),
        "a measurement ID in an environment or SDK configuration object is named");
    for(const char* body:{
        "window.__STATE__={\"googleAnalyticsCode\":\"G-XXXXXXXXXX\"};", "window.__STATE__={\"title\":\"G-7JY7T788PK\",\"code\":\"AW-17106321204\"};",
        "const s=\"{\\\"measurementId\\\":\\\"G-82RG1PXYVW\\\"}\";", "/* measurementId:\"G-82RG1PXYVW\" */var a=1;",
        "if(c.measurementId==\"G-82RG1PXYVW\")x();", "window.__STATE__={\"googleAnalyticsCode\":\"UA-12345-1\"};",
        "const list=[\"G-7JY7T788PK\",\"AW-17106321204\"];"})
        check(script(body).empty(),"placeholders, unnamed keys, escaped text, comments, comparisons and bare lists identify nothing");
}
}
