#include "vector/model.hpp"
#include "tag_config.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace vectorapp {
namespace {
constexpr size_t kVendorMarkupBytes=3*1024*1024;
struct Provider {
    const char *domain, *name, *category, *source;
    const char* path_prefix="";
    const char* path_suffix="";
    bool exact_host=false;
};


const Provider providers[] = {
#include "advertising_providers.inc"
    {"youtube.com","YouTube","Video","https://developers.google.com/youtube/iframe_api_reference"},
    {"youtube-nocookie.com","YouTube","Video","https://support.google.com/youtube/answer/171780"},
    {"googletagmanager.com","Google tagging","Tag management","https://developers.google.com/tag-platform/gtagjs"},
    {"google-analytics.com","Google Analytics","Analytics","https://developers.google.com/tag-platform/security/guides/csp"},
    {"js.stripe.com","Stripe","Payments","https://github.com/stripe/stripe-js"},
    {"api.stripe.com","Stripe","Payments","https://github.com/stripe/stripe-js"},
    {"donorbox.org","Donorbox","Donations","https://donorbox.org/widget.js","/widget.js","/widget.js",true},
    {"hooks.stripe.com","Stripe","Payments","https://github.com/stripe/stripe-js"},
    {"widget.intercom.io","Intercom","Support / chat","https://developers.intercom.com/installing-intercom/web/installation"},
    {"intercomcdn.com","Intercom","Support / chat","https://www.intercom.com/help/en/articles/3894-using-intercom-with-content-security-policy"},
    {"api-iam.intercom.io","Intercom","Support / chat","https://www.intercom.com/help/en/articles/3894-using-intercom-with-content-security-policy"},
    {"cdn.segment.com","Twilio Segment","Customer data","https://www.twilio.com/docs/segment/connections/sources/catalog/libraries/website/javascript/faq"},
    {"api.segment.io","Twilio Segment","Customer data","https://www.twilio.com/docs/segment/connections/sources/catalog/libraries/website/javascript/faq"},
    {"js.hs-scripts.com","HubSpot","Marketing / CRM","https://knowledge.hubspot.com/reports/how-do-i-know-if-my-hubspot-tracking-code-is-working"},
    {"js.hs-analytics.net","HubSpot","Marketing / CRM","https://knowledge.hubspot.com/reports/how-do-i-know-if-my-hubspot-tracking-code-is-working"},
    {"clarity.ms","Microsoft Clarity","Analytics","https://learn.microsoft.com/en-us/clarity/setup-and-installation/clarity-csp"},
    {"fonts.googleapis.com","Google Fonts","Fonts","https://developers.google.com/fonts/docs/getting_started"},
    {"fonts.gstatic.com","Google Fonts","Fonts","https://developers.google.com/fonts/docs/developer_api"},
    {"cdn.jsdelivr.net","jsDelivr","Package CDN","https://www.jsdelivr.com/"},
    {"unpkg.com","UNPKG","Package CDN","https://unpkg.com/"},
    {"cdnjs.cloudflare.com","Cloudflare cdnjs","Package CDN","https://cdnjs.com/"},
    {"challenges.cloudflare.com","Cloudflare challenges","Bot protection","https://developers.cloudflare.com/turnstile/get-started/client-side-rendering/"},
    {"static.cloudflareinsights.com","Cloudflare Insights","Analytics","https://developers.cloudflare.com/web-analytics/get-started/"},
    {"cdn.usefathom.com","Fathom Analytics","Analytics","https://usefathom.com/docs/script/embed","/script.js","/script.js",true},
    {"plausible.io","Plausible Analytics","Analytics","https://plausible.io/docs/proxy/introduction","/js/",".js",true},
    {"cdn.cookielaw.org","OneTrust","Consent management","https://developer.onetrust.com/onetrust/docs/using-tcf-with-header-bidding","/scripttemplates/otSDKStub.js","/scripttemplates/otSDKStub.js",true},
    {"cdn.cookielaw.org","OneTrust","Consent management","https://docs.onetrust.com/en/consent--preferences/cookie-consent/managing-cookie-scripts-cookie-blocking-and-technical-implementation/downloading-and-deploying-scripts-from-local-hosting","/consent/","/OtAutoBlock.js",true},
    {"global.ketchcdn.com","Ketch","Consent management","https://github.com/ketch-sdk/ketch-web-api","/web/","/boot.js",true},
    {"cdn.ketchjs.com","Ketch","Consent management","https://docs.ketch.com/","/ketchtag/","/ketch-sdk.js",true},
    {"www.datadoghq-browser-agent.com","Datadog","Monitoring","https://docs.datadoghq.com/real_user_monitoring/application_monitoring/browser/setup/client/","/datadog-rum-v",".js",true},
    {"www.datadoghq-browser-agent.com","Datadog","Monitoring","https://docs.datadoghq.com/real_user_monitoring/application_monitoring/browser/setup/client/","/","/datadog-rum.js",true},
    {"browser-intake-us5-datadoghq.com","Datadog","Monitoring","https://docs.datadoghq.com/real_user_monitoring/application_monitoring/browser/advanced_configuration/","/api/v2/rum","/api/v2/rum",true},
    {"browser-intake-datadoghq.com","Datadog","Monitoring","https://docs.datadoghq.com/real_user_monitoring/application_monitoring/browser/advanced_configuration/","/api/v2/rum","/api/v2/rum",true},
    {"api.ipify.org","ipify","IP address API","https://www.ipify.org/","/","/",true},
    {"api64.ipify.org","ipify","IP address API","https://www.ipify.org/","/","/",true},
    {"api6.ipify.org","ipify","IP address API","https://www.ipify.org/","/","/",true},
    {"www.google.com","reCAPTCHA","Bot protection","https://developers.google.com/recaptcha/docs/display","/recaptcha/","",true},
    {"www.recaptcha.net","reCAPTCHA","Bot protection","https://developers.google.com/recaptcha/docs/faq","/recaptcha/","",true},
    {"www.gstatic.com","reCAPTCHA","Bot protection","https://developers.google.com/recaptcha/docs/faq","/recaptcha/","",true},
    {"snap.licdn.com","LinkedIn Insight Tag","Advertising","https://learn.microsoft.com/en-us/linkedin/marketing/conversions/deduplication","/li.lms-analytics/insight.min.js","/li.lms-analytics/insight.min.js",true},
    {"tags.tiqcdn.com","Tealium","Tag management","https://docs.tealium.com/platforms/javascript/install/","/utag/",".js",true},
    {"cdn.optimizely.com","Optimizely","Experimentation","https://docs.optimizely.com/web-experimentation/api/Ajgv-state"},
    {"logx.optimizely.com","Optimizely","Experimentation","https://docs.developers.optimizely.com/experimentation-data/reference/post_events","/v1/events","/v1/events",true},
    {"ingest.sentry.io","Sentry","Error monitoring","https://docs.sentry.io/api/organizations/list-an-organizations-client-keys/","/api/","/envelope/"},
    {"demdex.net","Adobe Experience Cloud","Analytics / identity","https://experienceleague.adobe.com/en/docs/audience-manager/user-guide/reference/demdex-calls"},
    {"cdn.tinypass.com","Piano","Subscriptions","https://docs.piano.io/en/subscriptions/how-to-optimize-loading-and-executions-of-piano-scripts","/api/tinypass.min.js","/api/tinypass.min.js",true},
    {"static.elfsight.com","Elfsight","Widgets","https://help.elfsight.com/article/1581-why-my-widget-doesnt-display-on-my-website","/platform/platform.js","/platform/platform.js",true},
    {"elfsightcdn.com","Elfsight","Widgets","https://help.elfsight.com/article/1581-why-my-widget-doesnt-display-on-my-website","/platform.js","/platform.js",true},
    {"dev.visualwebsiteoptimizer.com","VWO","Experimentation","https://help.vwo.com/hc/en-us/articles/360020745993-Integrate-VWO-With-WordPress","/j.php","/j.php",true},
#include "integration_providers.inc"
#include "tracker_providers.inc"
};
bool under(const std::string& host,const std::string& domain) {
    return host==domain || (host.size()>domain.size() && host.ends_with("."+domain));
}
bool s3_host(const std::string& host) {
    static const std::string suffix=".amazonaws.com";
    if(host.size()<=suffix.size()||!host.ends_with(suffix))return false;
    std::vector<std::string> labels;std::istringstream parts(host.substr(0,host.size()-suffix.size()));
    for(std::string label;std::getline(parts,label,'.');)labels.push_back(label);
    const auto region=[](const std::string& label) {
        if(label.size()<9||label.size()>20)return false;
        int dashes=0;
        for(const char c:label){if(c=='-')++dashes;else if(!std::islower(static_cast<unsigned char>(c))&&!std::isdigit(static_cast<unsigned char>(c)))return false;}
        return dashes>=2&&std::islower(static_cast<unsigned char>(label.front()))&&std::isdigit(static_cast<unsigned char>(label.back()));
    };
    if(labels.empty()||labels.size()>8)return false;
    const auto& last=labels.back();
    if(last=="s3"||last=="s3-accelerate")return true;
    if(labels.size()>=2&&region(last)) {
        const auto& before=labels[labels.size()-2];
        if(before=="s3")return true;
        if(before=="dualstack"&&labels.size()>=3&&labels[labels.size()-3]=="s3")return true;
    }
    return last.starts_with("s3-")&&region(last.substr(3));
}
const Provider* provider_for(const std::string& host,const std::string& path) {
    static const Provider s3{"amazonaws.com","Amazon S3","Object storage","https://docs.aws.amazon.com/AmazonS3/latest/userguide/VirtualHosting.html"};
    if(s3_host(host))return &s3;
    for(const auto& p:providers)
        if((p.exact_host?host==p.domain:under(host,p.domain))&&path.starts_with(p.path_prefix)&&path.ends_with(p.path_suffix)&&
           (!*p.path_prefix||std::string_view(p.path_prefix)!=p.path_suffix||path==p.path_prefix))return &p;
    return nullptr;
}
struct Url { std::string host,origin,full,scheme,path; };
std::string source_url(const Url& page) { return printable(page.full.substr(0,page.full.find_first_of("?#")),1024); }
std::optional<Url> resolve(const std::string& value,const std::string& base) {
    if(value.empty() || value.size()>4096 || value.find('\\')!=std::string::npos)return {};
    for(unsigned char c:value)if(c<32 || c==127)return {};
    std::unique_ptr<CURLU,decltype(&curl_url_cleanup)> u(curl_url(),curl_url_cleanup);
    if(!u || curl_url_set(u.get(),CURLUPART_URL,base.c_str(),CURLU_NON_SUPPORT_SCHEME) ||
        curl_url_set(u.get(),CURLUPART_URL,value.c_str(),CURLU_NON_SUPPORT_SCHEME))return {};
    auto get=[&](CURLUPart part,unsigned flags=0U) {
        char* data=nullptr; std::string result;
        if(curl_url_get(u.get(),part,&data,flags)==CURLUE_OK){result=data;curl_free(data);}return result;
    };
    Url result;result.scheme=lower(get(CURLUPART_SCHEME));
    if(result.scheme!="http" && result.scheme!="https" && result.scheme!="ws" && result.scheme!="wss")return {};
    if(!get(CURLUPART_USER).empty() || !get(CURLUPART_PASSWORD).empty())return {};
    result.host=lower(get(CURLUPART_HOST));
    while(!result.host.empty() && result.host.back()=='.')result.host.pop_back();
    if(result.host.empty() || result.host.find('%')!=std::string::npos)return {};
    const auto port=get(CURLUPART_PORT);
    const bool standard=port.empty() || ((result.scheme=="https" || result.scheme=="wss")?port=="443":port=="80");
    result.origin=result.scheme+"://"+result.host+(standard?"":":"+port);
    result.full=get(CURLUPART_URL);
    result.path=get(CURLUPART_PATH);
    return result;
}
void add(ScanReport& report,VendorRelationship row) {
    for(auto& old:report.vendors) {
        if(old.name!=row.name || old.domain!=row.domain || old.relationship!=row.relationship || old.evidence_level!=row.evidence_level)continue;
        for(const auto& e:row.evidence) {
            if(std::any_of(old.evidence.begin(),old.evidence.end(),[&](const Evidence& x){return x.source==e.source && x.value==e.value;}))continue;
            if(old.evidence.size()>=16){report.vendors_truncated=true;break;}
            old.evidence.push_back(e);
        }
        return;
    }
    if(report.vendors.size()>=256){report.vendors_truncated=true;return;}
    report.vendors.push_back(std::move(row));
}
std::string capability(const std::string& kind) {
    if(kind=="Supported source")return "The page's form asks for a YouTube video URL. This identifies a supported content source; no form was submitted and no conversion or server-side connection was verified.";
    if(kind=="Script")return "If loaded and executed, this code runs in the page context and may read or change page content and accessible browser data. Execution and data transfer were not tested.";
    if(kind=="Embedded frame")return "If loaded, the vendor serves a separate embedded document. Browser origin rules and sandbox settings restrict interaction; this does not establish access to the parent page.";
    if(kind=="Form destination")return "The markup names a possible recipient of submitted form fields. No form was submitted; browser policy or script may change or block submission.";
    if(kind=="Image / media")return "If requested, this origin receives the resource request and associated request metadata. The reference alone does not identify tracking or prove data collection.";
    if(kind=="Stylesheet")return "If loaded, the stylesheet can affect presentation and reference other resources. It does not by itself prove arbitrary script execution.";
    if(kind=="Connection hint")return "The page suggests a connection or DNS lookup. This hint does not prove a request, integration activation, or data access.";
    if(kind=="Resource preload")return "The page hints that a resource may be fetched. A preload is not proof of execution or use.";
    if(kind=="Inline URL hint")return "Executable script text contains a URL literal. It may be used by a loader or API call, but could be inactive code or a string example. Execution, requests and data access are unverified.";
    if(kind=="Response handler")return "Response metadata advertises this infrastructure provider. It may serve or proxy traffic; headers can be configured or spoofed and do not reveal account permissions.";
    return "A browser policy mentions this origin. It does not prove a request or effective permission: other policies, nonces, directive precedence and browser behavior may restrict it.";
}
void reference(ScanReport& report,const Url& page,const std::string& base,const std::string& value,
               const std::string& kind,const std::string& level,const std::string& detail,bool wildcard=false) {
    auto url=resolve(trim(value),base);
    if(!url || (!wildcard && url->origin==page.origin))return;
    const auto* p=provider_for(url->host,url->path);
    VendorRelationship row;
    row.name=p?p->name:"Unattributed origin";row.category=p?p->category:"Ownership unknown";
    row.domain=(wildcard?"*.":"")+url->host;
    row.relationship=kind;row.evidence_level=level;row.confidence=p?"High":"Unknown";
    row.capability=capability(kind);row.attribution_source=p?p->source:"";
    if(level=="CSP report-only")row.capability="This report-only policy is monitoring configuration, not an enforced permission. A mention does not prove a request, active integration or data access.";
    const auto attribution_path=p&&*p->path_prefix?" / attribution path: "+printable(url->path,256):std::string{};
    row.evidence.push_back({source_url(page),detail+" -> "+(wildcard?"*."+url->host:url->origin)+attribution_path+
        (attribution_path.empty()?" (destination path and query omitted)":" (query omitted)")});
    add(report,std::move(row));
}



using Attributes=std::map<std::string,std::string>;
struct Tag { std::string name; Attributes attrs; bool closing=false; };
bool space(char c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f';}
bool name_char(char c){return std::isalnum(static_cast<unsigned char>(c))||c=='-'||c==':'||c=='_';}
std::string entities(const std::string& value) {
    std::string out;
    for(size_t i=0;i<value.size();++i) {
        if(value[i]!='&'){out+=value[i];continue;}
        const auto end=value.find(';',i+1);
        if(end==std::string::npos || end-i>12){out+='&';continue;}
        auto e=value.substr(i+1,end-i-1);int code=0;
        if(e=="amp" || e=="AMP")code='&';else if(e=="quot" || e=="QUOT")code='"';
        else if(e=="apos")code='\'';else if(e=="lt")code='<';else if(e=="gt")code='>';
        else if(e.starts_with("#")) {
            try {size_t used=0;auto digits=e.substr(e.size()>1 && (e[1]=='x'||e[1]=='X')?2:1);
                auto n=std::stoul(digits,&used,e.size()>1 && (e[1]=='x'||e[1]=='X')?16:10);
                if(used==digits.size() && n>0 && n<128)code=static_cast<int>(n);
            }catch(...){}
        }
        if(code){out+=static_cast<char>(code);i=end;}else out+='&';
    }
    return out;
}
std::optional<Tag> next_tag(const std::string& html,size_t& pos) {
    while(pos<html.size()) {
        pos=html.find('<',pos);if(pos==std::string::npos)return {};
        if(html.compare(pos,4,"<!--")==0){auto end=html.find("-->",pos+4);pos=end==std::string::npos?html.size():end+3;continue;}
        size_t i=pos+1;Tag tag;
        if(i<html.size() && html[i]=='/'){tag.closing=true;++i;}
        const size_t start=i;
        while(i<html.size() && name_char(html[i]))++i;
        if(i==start){++pos;continue;}
        tag.name=lower(html.substr(start,i-start));
        bool complete=false;
        while(i<html.size()) {
            while(i<html.size() && space(html[i]))++i;
            if(i>=html.size())break;
            if(html[i]=='>'){++i;complete=true;break;}
            if(html[i]=='/'){++i;continue;}
            size_t a=i;
            while(i<html.size() && !space(html[i]) && html[i]!='=' && html[i]!='>' && html[i]!='/')++i;
            if(i==a){++i;continue;}
            const auto key=lower(html.substr(a,i-a));
            while(i<html.size() && space(html[i]))++i;
            std::string value;
            if(i<html.size() && html[i]=='=') {
                ++i;while(i<html.size() && space(html[i]))++i;
                if(i<html.size() && (html[i]=='\'' || html[i]=='"')) {
                    const char quote=html[i++];a=i;while(i<html.size() && html[i]!=quote)++i;
                    value=html.substr(a,i-a);if(i<html.size())++i;
                } else {a=i;while(i<html.size() && !space(html[i]) && html[i]!='>')++i;value=html.substr(a,i-a);}
            }
            if(key.size()<80 && value.size()<=4096 && tag.attrs.size()<64)tag.attrs.emplace(key,entities(value));
        }
        pos=i;if(complete)return tag;return {};
    }
    return {};
}
std::string attr(const Tag& tag,const char* name) {
    auto it=tag.attrs.find(name);return it==tag.attrs.end()?"":it->second;
}
bool token(const std::string& value,const std::string& wanted) {
    std::istringstream in(lower(value));std::string part;
    while(in>>part)if(part==wanted)return true;
    return false;
}
void supported_source(const Tag& tag,const Url& page,ScanReport& report) {
    const auto type=lower(attr(tag,"type"));
    if((!type.empty()&&type!="text"&&type!="url"&&type!="search")||tag.attrs.contains("disabled")||tag.attrs.contains("readonly"))return;
    const auto placeholder=trim(attr(tag,"placeholder"));
    if(placeholder.empty())return;
    auto candidate=placeholder;
    if(candidate.starts_with("//"))candidate="https:"+candidate;
    else if(candidate.find("://")==std::string::npos)candidate="https://"+candidate;
    const auto url=resolve(candidate,page.full);if(!url)return;
    const auto* provider=provider_for(url->host,url->path);
    if(!provider||std::string_view(provider->domain)!="youtube.com")return;
    const auto path_start=url->full.find('/',url->full.find("://")+3);
    if(path_start==std::string::npos)return;
    const auto path=url->full.substr(path_start);
    if(!path.starts_with("/watch?"))return;
    const auto query=path.substr(7);bool video_key=false;std::istringstream parts(query);std::string part;
    while(std::getline(parts,part,'&'))if(part.starts_with("v="))video_key=true;
    if(!video_key)return;
    add(report,{"YouTube","Video","youtube.com","Supported source","Form input","Medium",capability("Supported source"),provider->source,
        {{source_url(page),"form input[placeholder] -> "+url->origin+"/watch?v= (query values omitted)"}}});
}
void policies(const std::string& policy,const std::string& source,const Url& page,ScanReport& report) {
    std::istringstream list(policy);std::string directive;
    const std::set<std::string> supported={"default-src","script-src","script-src-elem","connect-src","frame-src","child-src","img-src","style-src","style-src-elem","font-src","media-src","object-src","worker-src","form-action","report-uri"};
    size_t count=0;
    while(std::getline(list,directive,';') && count<1024) {
        std::istringstream words(directive);std::string name,value;words>>name;name=lower(name);
        if(!supported.contains(name))continue;
        while(words>>value) {
            if(++count>1024){report.vendors_truncated=true;break;}
            if(value.empty() || value[0]=='\'' || value=="*" || value.ends_with(':'))continue;
            bool wildcard=false;auto scheme=value.find("://");auto host_start=scheme==std::string::npos?0:scheme+3;
            if(value.compare(host_start,2,"*.")==0){value.erase(host_start,2);wildcard=true;}
            const auto starport=value.find(":*",host_start);if(starport!=std::string::npos)value.erase(starport,2);
            if(value.find('*')!=std::string::npos)continue;
            if(scheme==std::string::npos && name!="report-uri")value=page.scheme+"://"+value;
            reference(report,page,page.full,value,"Policy destination",source, name+" source",wildcard);
        }
    }
}
void infrastructure(const HttpObservation& h,const Url& page,ScanReport& report) {
    for(const auto& [rawkey,value]:h.headers) {
        const auto key=lower(rawkey),v=lower(value);std::string vendor,source;
        if(key=="cf-ray" || (key=="server" && v=="cloudflare")){vendor="Cloudflare";source="https://developers.cloudflare.com/fundamentals/reference/http-request-headers/";}
        else if(key=="x-vercel-id"){vendor="Vercel";source="https://vercel.com/docs/headers/response-headers";}
        else if(key=="x-nf-request-id"){vendor="Netlify";source="https://docs.netlify.com/manage/monitoring/request-ids/";}
        else if(key=="x-amz-cf-id" || key=="x-amz-cf-pop"){vendor="Amazon CloudFront";source="https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/RequestAndResponseBehaviorCustomOrigin.html";}
        if(vendor.empty() || value.empty())continue;
        add(report,{vendor,"Edge / hosting",page.host,"Response handler","Response header","Medium",capability("Response handler"),source,
            {{source_url(page),"HTTP "+key+" marker present (header value omitted)"}}});
    }
}
void inline_urls(std::string_view code,const Url& page,const std::string& base,ScanReport& report) {
    size_t count=0;
    for(size_t i=0;i<code.size();) {
        if(i+1<code.size() && code[i]=='/' && code[i+1]=='/') {auto end=code.find('\n',i+2);i=end==std::string_view::npos?code.size():end;continue;}
        if(i+1<code.size() && code[i]=='/' && code[i+1]=='*') {auto end=code.find("*/",i+2);i=end==std::string_view::npos?code.size():end+2;continue;}
        if(code[i]!='\'' && code[i]!='"' && code[i]!='`'){++i;continue;}
        char quote=code[i++];std::string literal;bool valid=true,closed=false;
        while(i<code.size()) {
            char c=code[i++];if(c==quote){closed=true;break;}
            if(c=='\\' && i<code.size()) {c=code[i++];if(c!='/' && c!='\\' && c!='\'' && c!='"')valid=false;}
            if(literal.size()<4096)literal+=c;else valid=false;
        }
        if(!valid || !closed || literal.find("${")!=std::string::npos)continue;
        const auto folded=lower(literal);
        if(!folded.starts_with("https://") && !folded.starts_with("http://") && !folded.starts_with("wss://") && !folded.starts_with("//"))continue;
        if(++count>256){report.vendors_truncated=true;break;}
        reference(report,page,base,literal,"Inline URL hint","Code hint","script text URL literal");
    }
}
bool unreached(const BrowserRequest& request) {
    static const std::string_view errors[]={"net::ERR_ADDRESS_INVALID","net::ERR_ADDRESS_UNREACHABLE","net::ERR_NAME_NOT_RESOLVED",
        "net::ERR_NAME_RESOLUTION_FAILED","net::ERR_CONNECTION_REFUSED","net::ERR_CONNECTION_RESET","net::ERR_CONNECTION_CLOSED",
        "net::ERR_CONNECTION_FAILED","net::ERR_CONNECTION_TIMED_OUT","net::ERR_TIMED_OUT","net::ERR_EMPTY_RESPONSE"};
    return request.status<=0&&request.state=="failed"&&request.collection=="network"&&
        std::find(std::begin(errors),std::end(errors),std::string_view(request.error))!=std::end(errors);
}
}

bool resource_provider(const std::string& url,ResourceProvider& provider) {
    const auto resolved=resolve(url,url);if(!resolved)return false;
    const auto* found=provider_for(resolved->host,resolved->path);if(!found)return false;
    provider={found->name,found->category};return true;
}

void identify_browser_vendor(const BrowserRequest& request,ScanReport& report) {
    const bool attempt=unreached(request);
    if(request.status<=0&&!attempt)return;
    const auto page=resolve(request.page,request.page), destination=resolve(request.url,request.page);
    if(!page || !destination || page->origin==destination->origin)return;
    const auto* provider=provider_for(destination->host,destination->path);
    std::string kind=request.type=="Script"?"Script":request.type=="Document"?"Embedded frame":request.type=="Stylesheet"?"Stylesheet":"Network request";
    if(attempt) {
        if(!provider)return;
        add(report,{provider->name,provider->category,destination->host,kind,"Request attempt","High",
            "The page asked the browser for this resource and the connection failed before any response. This shows the page's own code requests it. Nothing was received or executed, and no data transfer was observed. A local block list, DNS filter or network fault can cause the failure.",
            provider->source,
            {{"Browser network / "+source_url(*page),request.method+" "+source_url(*destination).substr(0,source_url(*destination).find(';'))+
                " / no response / "+request.error+" / "+request.type}}});
        return;
    }
    const std::string access=request.type=="Script"&&request.status>=200&&request.status<300?
        "The browser received this script during page execution. If executed, it can interact with the page context. This does not verify account access or what data the code read.":
        "A response from this destination was observed during page execution. This establishes browser contact, not account permissions or server-side access.";
    add(report,{provider?provider->name:"Unattributed origin",provider?provider->category:"External origin",destination->host,kind,
        "Observed response",provider?"High":"Unknown",access,provider?provider->source:"",
        {{"Browser network / "+source_url(*page),request.method+" "+source_url(*destination)+" / HTTP "+std::to_string(request.status)+" / "+request.type}}});
}

void identify_vendors(const HttpObservation& h,ScanReport& report,const std::string& evidence_prefix) {
    if(!evidence_prefix.empty()) {
        ScanReport collected;identify_vendors(h,collected);
        for(auto& row:collected.vendors) {
            for(auto& evidence:row.evidence)evidence.source=evidence_prefix+evidence.source;
            add(report,std::move(row));
        }
        report.vendors_truncated=report.vendors_truncated||collected.vendors_truncated;
        return;
    }
    if(!h.status)return;
    const auto page=resolve(h.url,h.url);if(!page)return;
    infrastructure(h,*page,report);
    bool html_type=false,has_type=false;
    for(const auto& [key,value]:h.headers) {
        const auto k=lower(key);
        if(k=="content-security-policy")policies(value,"CSP policy",*page,report);
        if(k=="content-security-policy-report-only")policies(value,"CSP report-only",*page,report);
        if(k=="content-type"){has_type=true;const auto mime=lower(trim(value.substr(0,value.find(';'))));html_type=mime=="text/html";}
    }
    if(has_type && !html_type)return;
    const std::string html=h.body.substr(0,kVendorMarkupBytes),folded=lower(html);
    std::string base=page->full;bool base_seen=false;
    size_t pos=0,tags=0;unsigned templates=0;bool in_form=false;
    while(auto tag=next_tag(html,pos)) {
        if(++tags>8192){report.vendors_truncated=true;break;}
        const auto& n=tag->name;
        if(n=="template"){if(tag->closing){if(templates)--templates;}else ++templates;continue;}
        if(n=="form"&&!templates)in_form=!tag->closing;
        if(tag->closing)continue;
        const bool raw=n=="script"||n=="style"||n=="textarea"||n=="title"||n=="xmp"||n=="iframe"||n=="noembed"||n=="noframes"||n=="noscript";
        size_t text_start=pos,text_end=pos;
        if(raw) {
            auto end=pos;
            while((end=folded.find("</"+n,end))!=std::string::npos) {
                auto after=end+2+n.size();if(after==html.size() || space(html[after]) || html[after]=='>' || html[after]=='/')break;++end;
            }
            pos=end==std::string::npos?html.size():end;
            text_end=pos;
        }
        if(n=="plaintext")break;
        if(templates)continue;
        if(n=="input"&&in_form)supported_source(*tag,*page,report);
        if(n=="base" && !base_seen && tag->attrs.contains("href")) {
            base_seen=true;if(auto b=resolve(attr(*tag,"href"),page->full))base=b->full;continue;
        }
        if(n=="meta" && lower(attr(*tag,"http-equiv"))=="content-security-policy") {
            policies(attr(*tag,"content"),"CSP meta policy",*page,report);continue;
        }
        std::string kind,value,detail;
        if(n=="script") {
            auto type=lower(trim(attr(*tag,"type")));
            if(!type.empty() && type!="module" && type!="text/javascript" && type!="application/javascript" && type!="text/ecmascript" && type!="application/ecmascript")continue;
            if(!tag->attrs.contains("src"))inline_urls(std::string_view(html).substr(text_start,text_end-text_start),*page,base,report);
            kind="Script";value=attr(*tag,"src");detail="script[src]";
            if(tag->attrs.contains("integrity"))detail+="; integrity attribute present (not validated)";
        }else if(n=="iframe") {
            if(tag->attrs.contains("srcdoc"))continue;
            kind="Embedded frame";value=attr(*tag,"src");detail="iframe[src]";
            detail+=tag->attrs.contains("sandbox")?"; sandbox attribute present":"; no sandbox attribute";
        }else if(n=="form"){kind="Form destination";value=attr(*tag,"action");detail="form[action]";}
        else if((n=="button"||n=="input") && tag->attrs.contains("formaction")){kind="Form destination";value=attr(*tag,"formaction");detail=n+"[formaction]";}
        else if(n=="img"||n=="audio"||n=="video"||n=="source"){kind="Image / media";value=attr(*tag,"src");detail=n+"[src]";}
        else if(n=="link") {
            auto rel=attr(*tag,"rel");value=attr(*tag,"href");detail="link[href]";
            if(token(rel,"stylesheet"))kind="Stylesheet";
            else if(token(rel,"preconnect")||token(rel,"dns-prefetch"))kind="Connection hint";
            else if(token(rel,"preload")||token(rel,"modulepreload"))kind="Resource preload";
        }
        if(!kind.empty())reference(report,*page,base,value,kind,"Page reference",detail);
    }
    if(h.body_truncated || h.body.size()>kVendorMarkupBytes)report.vendors_truncated=true;
}
}
