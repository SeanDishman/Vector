#include "vector/fingerprint.hpp"
#include "vector/model.hpp"
#include "markup.hpp"
#include "web_signatures.hpp"
#include "bundle_signatures.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <string_view>

namespace vectorapp {
void add_finding(std::vector<Finding>& all, Finding f) {
    auto it = std::find_if(all.begin(), all.end(), [&](const Finding& old) {
        return old.name == f.name && old.category == f.category &&
            (old.version == f.version || old.version.empty() || f.version.empty());
    });
    if (it == all.end()) { all.push_back(std::move(f)); return; }
    if (it->version.empty()) it->version = f.version;
    if (f.confidence == "High") it->confidence = "High";
    for (auto& e : f.evidence) {
        if (std::none_of(it->evidence.begin(), it->evidence.end(), [&](const Evidence& old) { return old.source == e.source && old.value == e.value; }))
            it->evidence.push_back(std::move(e));
    }
}
namespace {
struct Rule {
    const char *name, *category, *source, *pattern, *confidence;
    int version_group = 0;
    const char* required_literal = nullptr;
};
struct CompiledRule {
    Rule rule;
    std::regex regex;
    explicit CompiledRule(Rule r, bool case_sensitive=false) : rule(r), regex(r.pattern, std::regex::optimize | (case_sensitive ? std::regex::ECMAScript : std::regex::icase)) {}
};
const std::vector<CompiledRule>& rules() {
    static const std::vector<CompiledRule> r = [] {
        const Rule list[] = {
            {"nginx","Server","server",R"(\bnginx(?:/([0-9][0-9.a-z-]*))?)","High",1},
            {"Apache Traffic Server","Server","server",R"(^ATS/([0-9]{1,3}\.[0-9]{1,3}(?:\.[0-9]{1,3})?)(?:\s|$))","High",1},
            {"Apache HTTP Server","Server","server",R"(\bApache(?:/([0-9][0-9.]*))?)","High",1},
            {"Microsoft IIS","Server","server",R"(Microsoft-IIS(?:/([0-9.]+))?)","High",1},
            {"Caddy","Server","server",R"(\bCaddy\b)","High"},
            {"LiteSpeed","Server","server",R"(\b(?:OpenLiteSpeed|LiteSpeed)\b)","High"},
            {"lighttpd","Server","server",R"(lighttpd(?:/([0-9.]+))?)","High",1},
            {"Jetty","Server","server",R"(Jetty(?:[/ (]([0-9][0-9.a-z_-]*))?)","High",1},
            {"Apache Tomcat","Server","server",R"((?:Apache-Coyote|Apache Tomcat)(?:/([0-9.]+))?)","High",1},
            {"Gunicorn","Server","server",R"(gunicorn(?:/([0-9.]+))?)","High",1},
            {"Werkzeug","Server","server",R"(Werkzeug(?:/([0-9.]+))?)","High",1},
            {"Uvicorn","Server","server",R"(\buvicorn\b)","High"},
            {"PHP","Runtime","x-powered-by",R"(PHP(?:/([0-9][0-9.a-z-]*))?)","High",1},
            {"Express","Framework","x-powered-by",R"(^Express\b)","High"},
            {"ASP.NET","Framework","x-powered-by",R"(ASP\.NET)","High"},
            {"ASP.NET","Framework","x-aspnet-version",R"(^([0-9.]+))","High",1},
            {"Next.js","Framework","x-powered-by",R"(^Next\.js\b)","High"},
            {"Next.js","Framework","scripts",R"(/_next/static/)","High"},
            {"Nuxt","Framework","scripts",R"(/_nuxt/)","High"},
            {"Astro","Framework","scripts",R"(^(?:https?://[^/?#]+)?/_astro/[^/?#]+\.js(?:[?#]|$))","Medium"},
            {"Astro","Framework","assets",R"(^(?:https?://[^/?#]+)?/_astro/[^/?#]+\.css(?:[?#]|$))","Medium"},
            {"Web Font Loader","Library","scripts",R"(/ajax/libs/webfont/([0-9]+\.[0-9]+\.[0-9]+)/webfont\.js(?:[?#]|$))","High",1},
            {"Angular","Framework","html",R"(ng-version\s*=\s*["']([0-9.]+))","High",1},
            {"AngularJS","Library","scripts",R"(angular(?:[.-]([0-9]+\.[0-9.]+))?(?:\.min)?\.js)","Medium",1},
            {"React","Library","scripts",R"((?:^|[/@])react(?:[.-]([0-9]+\.[0-9.]+))?(?:\.production|\.development)?(?:\.min)?\.js)","Medium",1},
            {"Vue.js","Library","scripts",R"((?:^|[/@])vue(?:[.@-]([0-9]+\.[0-9.]+))?(?:\.runtime|\.global|\.esm-browser|\.prod)*(?:\.min)?\.js)","Medium",1},
            {"jQuery","Library","scripts",R"(jquery(?:[.-]([0-9]+\.[0-9]+(?:\.[0-9]+)?))?(?:\.min)?\.js)","High",1},
            {"jQuery","Library","scripts",R"(/jquery/([0-9]+\.[0-9]+(?:\.[0-9]+)?)/)","High",1},
            {"Bootstrap","Library","assets",R"(bootstrap(?:[.-]([0-9]+\.[0-9.]+))?(?:\.bundle)?(?:\.min)?\.(?:js|css))","High",1},
            {"Bootstrap","Library","assets",R"(bootstrap[@/]([0-9]+\.[0-9.]+)[/])","High",1},
            {"Alpine.js","Library","scripts",R"(alpinejs(?:@([0-9.]+))?/)","High",1},
            {"HTMX","Library","scripts",R"(htmx(?:\.org)?(?:@([0-9.]+))?(?:/|\.min\.js|\.js))","High",1},
            {"Polymer","Framework","scripts",R"((?:/|^)polymer(?:-element)?(?:\.min)?\.(?:js|html)$)","Medium"},
            {"Polymer","Framework","scripts",R"(/@polymer/polymer(?:@([0-9]+\.[0-9]+\.[0-9]+))?/)","High",1},
            {"lit-html","Library","scripts",R"(/lit-html(?:@([0-9]+\.[0-9]+\.[0-9]+))?/)","High",1},
            {"LitElement","Framework","scripts",R"(/lit-element(?:@([0-9]+\.[0-9]+\.[0-9]+))?/)","High",1},
            {"Web Components polyfills","Library","scripts",R"(/webcomponents-(?:bundle|loader|lite|all(?:-noPatch)?)(?:\.min)?\.js$)","Medium"},
            {"Web Animations polyfill","Library","scripts",R"(/web-animations(?:-next(?:-lite)?)?(?:\.min)?\.js$)","Medium"},
            {"Fetch polyfill","Library","scripts",R"(/fetch-polyfill(?:\.min)?\.js$)","Medium"},
            {"Intersection Observer polyfill","Library","scripts",R"(/intersection-observer(?:\.min)?\.js$)","Medium"},
            {"SPF","Framework","scripts",R"(/spf(?:\.vflset|/[0-9]+\.[0-9]+\.[0-9]+)/spf(?:\.min)?\.js$)","Medium"},
            {"Google Cast","Library","scripts",R"(^https://www\.gstatic\.com/(?:cv/js/sender/v[0-9]+|eureka/clank/[0-9]+)/cast_sender\.js$)","High"},
            {"ShadyDOM","Library","scripts",R"(/shadydom(?:\.min)?\.js$)","Medium"},
            {"ShadyCSS","Library","scripts",R"(/shadycss(?:\.min)?\.js$)","Medium"},
            {"YouTube web client","Platform","scripts",R"(/s/desktop/[A-Za-z0-9_-]+/jsbin/desktop_polymer(?:_[A-Za-z0-9_-]+)?\.vflset/(?:[A-Za-z_-]+/)?desktop_polymer(?:_[A-Za-z0-9_-]+)?\.js$)","High"},
            {"YouTube web client","Platform","scripts",R"(/s/_/ytmainappweb/_/js/k=ytmainappweb\.[^/?#]+/[^?#]*/m=[A-Za-z0-9_,.-]+$)","High"},
            {"YouTube player","Library","scripts",R"(/s/player/[A-Za-z0-9_-]+/player_[A-Za-z0-9_-]+\.vflset/(?:[A-Za-z_-]+/)?base\.js$)","High"},
            {"YouTube player","Library","scripts",R"(^https?://(?:www\.)?youtube(?:-nocookie)?\.com(?::[0-9]+)?/(?:iframe_api|player_api)$)","High"},
            {"ESF","Server","server",R"(^ESF$)","High"},
            {"gws","Server","server",R"(^gws$)","High"},
            {"Google Frontend","Server","server",R"(^Google Frontend$)","High"},
            {"WordPress","CMS","generator",R"(WordPress(?:\s+([0-9.]+))?)","High",1},
            {"WordPress","CMS","assets",R"(/wp-(?:content|includes)/)","High"},
            {"Drupal","CMS","generator",R"(Drupal(?:\s+([0-9.]+))?)","High",1},
            {"Drupal","CMS","x-generator",R"(Drupal(?:\s+([0-9.]+))?)","High",1},
            {"Joomla","CMS","generator",R"(Joomla!?(?:\s+([0-9.]+))?)","High",1},
            {"Ghost","CMS","generator",R"(Ghost(?:\s+([0-9.]+))?)","High",1},
            {"Hugo","Generator","generator",R"(Hugo(?:\s+([0-9.]+))?)","High",1},
            {"Jekyll","Generator","generator",R"(Jekyll(?:\s+v?([0-9.]+))?)","High",1},
            {"Docusaurus","Generator","generator",R"(^Docusaurus(?:\s+v?([0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?))?$)","High",1},
            {"Gatsby","Framework","generator",R"(^Gatsby(?:\s+([0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?))?$)","High",1},
            {"Mintlify","Documentation","assets",R"(/_mintlify/(?:assets|custom|static)/[^?#]{1,300}\.(?:js|css)(?:[?#]|$))","High"},
            {"Mintlify","Documentation","generator",R"(^Mintlify$)","High"},
            {"Clerk","Authentication","scripts",R"(/npm/@clerk/clerk-js@(?:([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(?:-[A-Za-z0-9.-]{1,40})?)|[0-9A-Za-z.^~-]{1,40})/dist/[A-Za-z0-9_.-]{1,120}\.js$)","High",1},
            {"Shopify","Platform","assets",R"((?:cdn\.shopify\.com|/cdn/shop/))","High"},
            {"Wix","Platform","assets",R"((?:static\.wixstatic\.com|static\.parastorage\.com))","Medium"},
            {"Squarespace","Platform","generator",R"(Squarespace)","High"},
            {"Cloudflare","Edge","server",R"(^cloudflare$)","High"},
            {"Cloudflare","Edge","cf-ray",R"(^[a-z0-9-]+$)","High"},
            {"Vercel","Edge","server",R"(^Vercel$)","High"},
            {"Vercel","Edge","x-vercel-id",R"(\S+)","High"},
            {"Netlify","Edge","server",R"(^Netlify$)","High"},
            {"Amazon CloudFront","Edge","via",R"(cloudfront)","High"},
            {"Google Cloud Load Balancing","Load balancer","via",R"((?:^|,)\s{0,8}1\.1 google\s{0,8}(?:,|$))","High"},
            {"Amazon S3","Storage","server",R"(^AmazonS3$)","High"},
            {"Varnish","Cache","via",R"(varnish)","High"},
            {"Envoy","Proxy","server",R"(^envoy$)","High"},
            {"Envoy","Proxy","x-envoy-upstream-service-time",R"(^[0-9]{1,9}$)","Medium"},
            {"OpenResty","Server","server",R"(^openresty(?:/([0-9][0-9.]*))?(?:\s|$))","High",1},
            {"Akamai","Edge","server",R"(^Akamai(?:GHost|NetStorage)(?:\s|$))","High"},
            {"Akamai","Edge","x-akamai-transformed",R"(^[0-9]{1,4}\s)","High"},
            {"Amazon S3","Storage","x-amz-id-2",R"(^[A-Za-z0-9+/=]{24,200}$)","Medium"},
            {"Fastly","Edge","x-served-by",R"(^cache-[a-z0-9-]*[0-9]+-[a-z]{3}(?:,\s*cache-[a-z0-9-]*[0-9]+-[a-z]{3})*$)","Medium"},
            {"MikroTik RouterOS","Device hint","server",R"((?:MikroTik|RouterOS))","Medium"},
            {"MikroTik RouterOS","Device hint","title",R"((?:RouterOS|MikroTik|WebFig))","Medium"},
            {"Synology NAS","Device hint","title",R"((?:Synology|DiskStation|Synology DiskStation Manager))","Medium"},
            {"QNAP NAS","Device hint","title",R"((?:QNAP|QTS Desktop))","Medium"},
            {"Ubiquiti UniFi","Device hint","title",R"((?:UniFi|Ubiquiti))","Medium"},
            {"pfSense firewall","Device hint","title",R"(pfSense)","Medium"},
            {"OPNsense firewall","Device hint","title",R"(OPNsense)","Medium"},
            {"Fortinet FortiGate","Device hint","title",R"(FortiGate)","Medium"},
            {"Hikvision camera","Device hint","server",R"(Hikvision)","Medium"},
            {"Hikvision camera","Device hint","www-authenticate",R"(Hikvision)","Medium"},
            {"Dahua camera","Device hint","www-authenticate",R"(Dahua)","Medium"},
            {"HP printer","Device hint","server",R"((?:HP HTTP Server|HP-ChaiSOE|HP Jetdirect))","Medium"},
            {"Brother printer","Device hint","server",R"((?:Brother|debut))","Low"},
            {"Windows","OS hint","server",R"((?:Microsoft-IIS|Win32|Win64))","Medium"},
            {"Ubuntu Linux","OS hint","server",R"(Ubuntu)","Medium"},
            {"Debian Linux","OS hint","server",R"(Debian)","Medium"},
            {"FreeBSD","OS hint","server",R"(FreeBSD)","Medium"},
            {"OpenWrt","Device hint","title",R"((?:OpenWrt|LuCI))","Medium"}
        };
        std::vector<CompiledRule> out;
        for (const auto& rule : list) out.emplace_back(rule);
        return out;
    }();
    return r;
}

}
void identify_script_reference(const std::string& url,const std::string& source,std::vector<Finding>& all) {
    Target target;
    try {target=parse_target(url);}catch(const std::exception&){return;}
    target.path=target.path.substr(0,target.path.find_first_of("?#"));
    const bool default_port=(target.scheme=="https"&&target.port==443)||(target.scheme=="http"&&target.port==80);
    const auto clean=target.scheme+"://"+target.authority()+(default_port?"":":"+std::to_string(target.port))+target.path;
    for(const auto& r:rules()) {
        if(std::string_view(r.rule.source)!="scripts"&&std::string_view(r.rule.source)!="assets")continue;
        std::smatch match;if(!std::regex_search(clean,match,r.regex))continue;
        const auto version=r.rule.version_group?match[r.rule.version_group].str():"";
        add_finding(all,{r.rule.name,r.rule.category,version,r.rule.confidence,{{source,printable(url,300)}}});
    }
    identify_os_libraries(clean,source,all);
}

void identify_javascript(const std::string& url,const std::string& body,std::vector<Finding>& all) {
    const auto sample=body.substr(0,2*1024*1024);
    if(sample.empty())return;
    std::vector<unsigned char> region(sample.size(),0);
    for(size_t i=0;i<sample.size();) {
        const auto start=i;const char c=sample[i];
        unsigned char kind=0;
        if(c=='\''||c=='"'||c=='`') {
            kind=1;++i;
            while(i<sample.size()) {if(sample[i]=='\\'){i=std::min(sample.size(),i+2);continue;}if(sample[i++]==c)break;}
        } else if(c=='/'&&i+1<sample.size()&&sample[i+1]=='*') {
            kind=2;const auto end=sample.find("*/",i+2);i=end==std::string::npos?sample.size():end+2;
        } else if(c=='/'&&i+1<sample.size()&&sample[i+1]=='/') {
            kind=2;const auto end=sample.find('\n',i+2);i=end==std::string::npos?sample.size():end;
        } else if(c=='/') {
            size_t previous=i;while(previous&&std::isspace(static_cast<unsigned char>(sample[previous-1])))--previous;
            const bool regex_literal=previous==0||std::string_view("=([{,:;!?&|+*-~%^<>").find(sample[previous-1])!=std::string_view::npos;
            ++i;
            if(regex_literal) {
                kind=1;bool character_class=false;
                while(i<sample.size()&&sample[i]!='\n') {
                    const char next=sample[i++];
                    if(next=='\\'){if(i<sample.size())++i;continue;}
                    if(next=='[')character_class=true;else if(next==']')character_class=false;
                    else if(next=='/'&&!character_class)break;
                }
            }
        } else {++i;}
        if(kind)std::fill(region.begin()+start,region.begin()+i,kind);
    }
    web_signatures().javascript(url.empty()?"inline script":url,sample,region,all);
    bundle_signatures::identify(url.empty()?"inline script":url,sample,region,all);
    static const std::vector<CompiledRule> js=[] {
        const Rule list[]={
            {"jQuery","Library","banner",R"(\bjQuery v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"jquery v"},
            {"jQuery","Library","banner",R"(\bjQuery JavaScript Library v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"jquery javascript library v"},
            {"Bootstrap","Library","banner",R"(\bBootstrap v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"bootstrap v"},
            {"Vue.js","Library","banner",R"((?:Vue\.js v|@vue/[a-z-]+ v)([0-9]+\.[0-9]+\.[0-9]+))","High",1," v"},
            {"React","Library","banner",R"(\bReact v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"react v"},
            {"Alpine.js","Library","banner",R"(\bAlpine(?:\.js)? v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"alpine"},
            {"HTMX","Library","banner",R"(\bhtmx (?:v|version )?([0-9]+\.[0-9]+\.[0-9]+))","High",1,"htmx"},
            {"lodash","Library","banner",R"(\blodash v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"lodash v"},
            {"Moment.js","Library","banner",R"(moment\.js\s{0,20}([0-9]+\.[0-9]+\.[0-9]+))","High",1,"moment.js"},
            {"axios","Library","banner",R"(\baxios v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"axios v"},
            {"Socket.IO","Library","banner",R"(Socket\.IO v([0-9]+\.[0-9]+\.[0-9]+))","High",1,"socket.io v"},
            {"three.js","Library","banner",R"(three\.js r([0-9]+))","High",1,"three.js r"},
            {"GSAP","Library","banner",R"(\bGSAP ([0-9]+\.[0-9]+\.[0-9]+))","High",1,"gsap"},
            {"core-js","Library","banner",R"(core-js@([0-9]+\.[0-9]+\.[0-9]+))","High",1,"core-js@"},
            {"Font Awesome","Library","banner",R"(Font Awesome (?:Free )?([0-9]+\.[0-9]+\.[0-9]+))","High",1,"font awesome"},
            {"Google Closure Library","Library","banner",R"(Copyright[^\n]{0,35}The Closure Library Authors)","High",0,"the closure library authors"},
            {"Polymer","Framework","runtime",R"rx(\bPolymer\.version\s{0,20}=\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`])rx","High",1,"Polymer"},
            {"Polymer","Framework","runtime",R"rx(\bPolymer\s{0,20}\(\s{0,20}\{\s{0,20}is\s{0,20}:\s{0,20}["'`][a-z][a-z0-9-]{0,70}-[a-z0-9-]{1,70}["'`])rx","Medium",0,"Polymer"},
            {"lit-html","Library","runtime",R"rx(\blitHtmlVersions[^;\n"']{0,100}?\.push\(\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`]\s{0,20}\))rx","High",1,"litHtmlVersions"},
            {"LitElement","Framework","runtime",R"rx(\blitElementVersions[^;\n"']{0,100}?\.push\(\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`]\s{0,20}\))rx","High",1,"litElementVersions"},
            {"Lit ReactiveElement","Library","runtime",R"rx(\breactiveElementVersions[^;\n"']{0,100}?\.push\(\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`]\s{0,20}\))rx","High",1,"reactiveElementVersions"},
            {"React","Library","runtime",R"rx(\bReact\.version\s{0,20}=\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`])rx","High",1,"React.version"},
            {"React","Library","runtime",R"rx(\bversion\s{0,20}:\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`]\s{0,20},\s{0,20}rendererPackageName\s{0,20}:\s{0,20}["'`]react-dom["'`])rx","High",1,"rendererPackageName"},
            {"jQuery","Library","runtime",R"rx(\bjQuery\.fn\.jquery\s{0,20}=\s{0,20}["'`]([0-9]+\.[0-9]+\.[0-9]+)["'`])rx","High",1,"jQuery.fn.jquery"},
            {"ShadyCSS","Library","runtime",R"(\bShadyCSS\.(?:prepareTemplate|styleElement|styleDocument)\s{0,20}\()","Medium",0,"ShadyCSS."},
            {"Google Closure Library","Library","runtime",R"(\bgoog\.(?:provide|require|exportSymbol)\s{0,20}=\s{0,20}function\s{0,20}\()","Medium",0,"goog."},
            {"webpack","Build tool","runtime",R"rx(\b(?:self|window)\s{0,10}(?:\.webpackChunk[A-Za-z0-9_$]{0,80}|\[\s{0,10}["'`]webpackChunk[A-Za-z0-9_$]{0,80}["'`]\s{0,10}\])\s{0,20}=)rx","Medium",0,"webpackChunk"}
        };
        std::vector<CompiledRule> out;for(const auto& rule:list)out.emplace_back(rule,std::string_view(rule.source)=="runtime");return out;
    }();
    const auto source=url.empty()?"inline script":url;
    const auto folded=lower(sample);
    for(const auto& r:js) {
        const bool banner=std::string_view(r.rule.source)=="banner";
        if(r.rule.required_literal && (banner?folded:sample).find(r.rule.required_literal)==std::string::npos)continue;
        for(auto it=std::sregex_iterator(sample.begin(),sample.end(),r.regex);it!=std::sregex_iterator();++it) {
            const auto pos=static_cast<size_t>(it->position());
            if(region[pos]!=(std::string_view(r.rule.source)=="banner"?2:0))continue;
            add_finding(all,{r.rule.name,r.rule.category,r.rule.version_group?(*it)[r.rule.version_group].str():"",r.rule.confidence,{{source,printable(it->str(),300)}}});
        }
    }
    const auto marker=[&](const std::regex& pattern,std::string_view literal="") {
        if(!literal.empty()&&sample.find(literal)==std::string::npos)return std::string{};
        for(auto it=std::sregex_iterator(sample.begin(),sample.end(),pattern);it!=std::sregex_iterator();++it)
            if(region[static_cast<size_t>(it->position())]==0)return it->str();
        return std::string{};
    };
    static const std::regex element(R"rx(\bSymbol\.for\(\s{0,20}["'`]react\.(?:transitional\.)?element["'`]\s{0,20}\))rx");
    static const std::regex fragment(R"rx(\bSymbol\.for\(\s{0,20}["'`]react\.fragment["'`]\s{0,20}\))rx");
    const auto e=marker(element,"react."),f=marker(fragment,"react.fragment");
    if(!e.empty()&&!f.empty())add_finding(all,{"React","Library",{},"Medium",{{source,e},{source,f}}});
    static const std::regex tsr_bootstrap(R"(\bself\.\$_TSR\s{0,20}=\s{0,20}\{)");
    static const std::regex tsr_router(R"((?:^|[^A-Za-z0-9_$])\$_TSR\.router\s{0,20}=)");
    const auto bootstrap=marker(tsr_bootstrap,"$_TSR"),router=marker(tsr_router,"$_TSR.router");
    if(!bootstrap.empty()&&!router.empty())add_finding(all,{"TanStack Router","Framework",{},"High",{{source,bootstrap},{source,router}}});
    const auto paired=[&](const char* name,const char* category,std::initializer_list<const std::regex*> patterns) {
        Finding finding{name,category,{},"High",{}};
        for(const auto* pattern:patterns) {
            const auto value=marker(*pattern);if(value.empty())return;
            finding.evidence.push_back({source,printable(value,300)});
        }
        add_finding(all,std::move(finding));
    };
    static const std::regex tealium_config(R"rx(\butag\.cfg\s{0,20}=\s{0,20}\{\s{0,20}template\s{0,20}:\s{0,20}["'`]ut[0-9][0-9.]{1,16}["'`])rx");
    static const std::regex tealium_library(R"rx(\btealium_library_name\s{0,20}=\s{0,20}["'`]utag\.js["'`])rx");
    static const std::regex tealium_loader(R"(\butag\.loader\.[A-Za-z_$][A-Za-z0-9_$]{0,40})");
    if(sample.find("utag.cfg")!=std::string::npos&&sample.find("tealium_library_name")!=std::string::npos&&sample.find("utag.loader.")!=std::string::npos)
        paired("Tealium","Tag management",{&tealium_config,&tealium_library,&tealium_loader});
    static const std::regex swiper_wrapper(R"rx(\bwrapperClass\s{0,20}:\s{0,20}["'`]swiper-wrapper["'`])rx");
    static const std::regex swiper_slide(R"rx(\bslideClass\s{0,20}:\s{0,20}["'`]swiper-slide["'`])rx");
    static const std::regex swiper_layout(R"(\.slidesPerViewDynamic\s{0,20}\()");
    if(sample.find("swiper-wrapper")!=std::string::npos&&sample.find("swiper-slide")!=std::string::npos&&sample.find(".slidesPerViewDynamic")!=std::string::npos)
        paired("Swiper","Library",{&swiper_wrapper,&swiper_slide,&swiper_layout});
    static const std::regex optimizely_init(R"(\bwindow\.optimizely\.initialized\s{0,20}=)");
    static const std::regex optimizely_experiments(R"(\bgetActiveExperimentIds\s{0,20}:\s{0,20}function\s{0,20}\()");
    static const std::regex optimizely_variations(R"(\bgetVariationMap\s{0,20}:\s{0,20}function\s{0,20}\()");
    if(sample.find("window.optimizely.initialized")!=std::string::npos&&sample.find("getActiveExperimentIds")!=std::string::npos&&sample.find("getVariationMap")!=std::string::npos)
        paired("Optimizely","Experimentation",{&optimizely_init,&optimizely_experiments,&optimizely_variations});
}

void identify_http(const HttpObservation& h, std::vector<Finding>& all) {
    web_signatures().cookies(h,all);
    std::vector<std::pair<std::string,std::string>> sources = h.headers;
    sources.emplace_back("title", h.title);
    for(const auto& tag:markup::tags(h.body)) {
        if(tag.name=="link") {
            const auto href=tag.attr("href");if(!href.empty())sources.emplace_back("assets",href);
        } else if(tag.name=="meta"&&lower(tag.attr("name"))=="generator")sources.emplace_back("generator",tag.attr("content"));
        if(!tag.attr("ng-version").empty())sources.emplace_back("html",tag.opening);
    }
    for (const auto& [source, value] : sources) {
        for (const auto& r : rules()) {
            if (source != r.rule.source) continue;
            std::smatch match;
            if (!std::regex_search(value, match, r.regex)) continue;
            std::string version;
            if (r.rule.version_group && match.size() > static_cast<size_t>(r.rule.version_group)) version = match[r.rule.version_group].str();
            auto evidence = source == "html" ? match.str() : value;
            add_finding(all, {r.rule.name,r.rule.category,version,r.rule.confidence,{{h.url + " | " + source, printable(evidence,300)}}});
        }
        if (source != "html" && source != "title") identify_os_libraries(value, h.url + " | " + source, all);
        if (source != "html" && source != "title") identify_hardware(value, h.url + " | " + source, all);
    }
    identify_wordpress_plugins(h.body, h.url + " | html", all);
    absorb_page_markup(h, all);
    identify_ntlm(h, all);
    note_http_signals(h, all);
}
void identify_upnp_type(const std::string& type, const std::string& source, std::vector<Finding>& all) {
    static const CompiledRule gateway({"Router / gateway","Device hint","ssdp-st",
        R"(^urn:schemas-upnp-org:device:InternetGatewayDevice:[12]$)","Medium"},true);
    if(std::regex_match(type,gateway.regex))
        add_finding(all,{gateway.rule.name,gateway.rule.category,{},gateway.rule.confidence,{{source,type}}});
}
void identify_banner(Service& s, std::vector<Finding>& all) {
    const auto& raw = s.banner;
    auto source = "TCP " + std::to_string(s.port) + (s.active_probe ? " probe" : " banner");
    auto add = [&](std::string name, std::string category, std::string version = {}, std::string confidence = "High") {
        add_finding(all,{std::move(name),std::move(category),std::move(version),std::move(confidence),{{source,printable(version.empty() ? raw : raw, 300)}}});
    };
    auto add_text = [&](std::string name, std::string category, std::string version, std::string confidence, const std::string& evidence) {
        add_finding(all,{std::move(name),std::move(category),std::move(version),std::move(confidence),{{source,printable(evidence,300)}}});
    };
    if (raw.size() >= 4 && static_cast<unsigned char>(raw[0]) == 0xfe && raw[1] == 'S' && raw[2] == 'M' && raw[3] == 'B') {
        s.name = "SMB";
        std::string version;
        if (raw.size() >= 70 && static_cast<unsigned char>(raw[64]) == 65 && raw[65] == 0) {
            const unsigned dialect = static_cast<unsigned char>(raw[68]) | (static_cast<unsigned char>(raw[69]) << 8);
            if (dialect == 0x0202) version = "2.0.2";
            else if (dialect == 0x0210) version = "2.1";
            else if (dialect == 0x0300) version = "3.0";
            else if (dialect == 0x0302) version = "3.0.2";
            else if (dialect == 0x0311) version = "3.1.1";
        }
        add("SMB2","Protocol",version);
        return;
    }
    if (raw.size() >= 4 && static_cast<unsigned char>(raw[0]) == 0xff && raw[1] == 'S' && raw[2] == 'M' && raw[3] == 'B') {
        s.name = "SMB"; add("SMB1","Protocol"); return;
    }
    if (raw.size() >= 11 && static_cast<unsigned char>(raw[0]) == 0x03 && static_cast<unsigned char>(raw[1]) == 0x00 && static_cast<unsigned char>(raw[5]) == 0xd0) {
        s.name = "RDP";
        std::string selected;
        if (raw.size() >= 19 && static_cast<unsigned char>(raw[11]) == 0x02) {
            const unsigned proto = static_cast<unsigned char>(raw[15]) | (static_cast<unsigned char>(raw[16]) << 8) |
                (static_cast<unsigned char>(raw[17]) << 16) | (static_cast<unsigned char>(raw[18]) << 24);
            if (proto == 0) selected = "standard";
            else if (proto == 1) selected = "TLS";
            else if (proto == 2) selected = "CredSSP";
        }
        add("RDP","Protocol",selected);
        return;
    }
    if (raw.size() >= 3 && (static_cast<unsigned char>(raw[0]) == 0x16 || static_cast<unsigned char>(raw[0]) == 0x15) && static_cast<unsigned char>(raw[1]) == 0x03) {
        s.name = "TLS";
        const auto minor = static_cast<unsigned char>(raw[2]);
        std::string version = minor == 1 ? "1.0" : minor == 2 ? "1.1" : minor == 3 ? "1.2" : minor == 4 ? "1.3" : "";
        add("TLS","Protocol",version);
        if (auto fp = ja4s_from_tls(raw); !fp.empty()) add_text("JA4S","Fingerprint",fp,"High",fp);
        return;
    }
    if (raw.size() >= 6 && raw[3] == 0 && static_cast<unsigned char>(raw[4]) == 10) {
        const auto end = raw.find('\0', 5);
        const auto version = raw.substr(5, (end == std::string::npos ? raw.size() : end) - 5);
        const bool textual = !version.empty() && version.size() < 32 &&
            std::all_of(version.begin(), version.end(), [](unsigned char c) { return c >= 32 && c < 127; }) &&
            std::isdigit(static_cast<unsigned char>(version[0]));
        if (textual) { s.name = "MySQL"; add("MySQL","Service",version); return; }
    }
    if (raw.starts_with("AMQP")) { s.name = "AMQP"; add("AMQP","Protocol"); return; }
    if (raw.starts_with("PostgreSQL SSLRequest ")) { s.name = "PostgreSQL"; add("PostgreSQL","Service"); return; }
    if (raw.starts_with("MQTT CONNACK")) { s.name = "MQTT"; add("MQTT","Protocol"); return; }
    if (raw.starts_with("DNS TCP response")) { s.name = "DNS"; add("DNS","Protocol"); return; }
    const auto text = printable(raw, 1024);
    if (text.empty()) return;
    std::smatch m;
    if (raw.starts_with("HTTP/")) {
        s.name = "HTTP";
        add("HTTP","Protocol");
        if (std::regex_search(raw, m, std::regex(R"((?:^|\r?\n)Server:\s*([^\r\n]+))", std::regex::icase))) {
            const auto server = m[1].str();
            if (std::regex_search(server, m, std::regex(R"(nginx(?:/([0-9][0-9.a-z-]*))?)", std::regex::icase))) add_text("nginx","Server",m[1].matched ? m[1].str() : "","High",server);
            else if (std::regex_search(server, m, std::regex(R"(Apache(?:/([0-9][0-9.]*))?)", std::regex::icase))) add_text("Apache HTTP Server","Server",m[1].matched ? m[1].str() : "","High",server);
            else if (std::regex_search(server, m, std::regex(R"(Microsoft-IIS(?:/([0-9.]+))?)", std::regex::icase))) add_text("Microsoft IIS","Server",m[1].matched ? m[1].str() : "","High",server);
            identify_os_libraries(server, source, all);
        }
        return;
    }
    if (text.starts_with("+PONG") || text.starts_with("-NOAUTH")) { s.name = "Redis"; add("Redis","Service"); return; }
    if (text.starts_with("VERSION ")) {
        s.name = "Memcached";
        std::string version;
        if (std::regex_search(text, m, std::regex(R"(VERSION\s+([0-9][0-9.]*))"))) version = m[1].str();
        add("Memcached","Service",version);
        return;
    }
    if (std::regex_search(text, m, std::regex(R"(^SSH-[0-9.]+-([^ ]+))"))) {
        s.name = "SSH";
        const auto product = m[1].str();
        if (std::regex_search(product,m,std::regex(R"(^OpenSSH_([0-9][a-zA-Z0-9.p-]*))"))) add("OpenSSH","Service",m[1].str());
        else if (std::regex_search(product,m,std::regex(R"(^dropbear_([0-9.]+))",std::regex::icase))) add("Dropbear","Service",m[1].str());
        else add(product,"Service");
    } else if (std::regex_search(text,m,std::regex(R"(^RFB ([0-9.]+))"))) {
        s.name = "VNC"; add("VNC / RFB","Protocol",m[1].str());
    } else if (lower(text).starts_with("* ok") && lower(text).find("imap") != std::string::npos) {
        s.name = "IMAP"; add("IMAP","Protocol");
    } else if (text.starts_with("+OK")) {
        s.name = "POP3"; add("POP3","Protocol",{},"Medium");
    } else if (text.starts_with("220") && lower(text).find("ftp") != std::string::npos) {
        s.name = "FTP"; add("FTP","Protocol");
    } else if (text.starts_with("220") && (lower(text).find("smtp") != std::string::npos || lower(text).find("postfix") != std::string::npos)) {
        s.name = "SMTP"; add("SMTP","Protocol");
    } else if (text.starts_with("RTSP/")) { s.name = "RTSP"; add("RTSP","Protocol"); }
    const Rule list[] = {
        {"vsftpd","Service","",R"(vsftpd\s+([0-9.]+))","High",1},
        {"ProFTPD","Service","",R"(ProFTPD\s+([0-9.]+))","High",1},
        {"Postfix","Service","",R"(Postfix)","High"},
        {"Exim","Service","",R"(Exim\s+([0-9.]+))","High",1},
        {"Dovecot","Service","",R"(Dovecot)","High"},
        {"Ubuntu Linux","OS hint","",R"(Ubuntu)","Medium"},
        {"Debian Linux","OS hint","",R"(Debian)","Medium"},
        {"Windows","OS hint","",R"((?:OpenSSH_for_Windows|Microsoft))","Medium"},
        {"FreeBSD","OS hint","",R"(FreeBSD)","Medium"},
        {"MikroTik RouterOS","Device hint","",R"((?:MikroTik|RouterOS))","Medium"},
        {"Cisco device","Device hint","",R"((?:Cisco IOS|Cisco Systems))","Medium"}
    };
    for (const auto& r : list) if (std::regex_search(text,m,std::regex(r.pattern,std::regex::icase)))
        add(r.name,r.category,r.version_group ? m[r.version_group].str() : "",r.confidence);
    identify_os_libraries(text, source, all);
    identify_hardware(text, source, all);
}
}
