#pragma once
#include "vector/model.hpp"
#include <cctype>
#include <regex>

namespace vectorapp::embedded_libraries {
struct Marker {std::string value;std::vector<std::string> groups;size_t position=0;};


inline Marker marker(const std::string& body,const std::vector<unsigned char>& region,
                     const char* literal,const std::regex& pattern) {
    size_t from=0;
    for(int count=0;count<64;++count) {
        const auto hit=body.find(literal,from);if(hit==std::string::npos)break;
        from=hit+1;const auto begin=hit>100?hit-100:0;
        const auto window=body.substr(begin,std::min<size_t>(512,body.size()-begin));
        for(auto it=std::sregex_iterator(window.begin(),window.end(),pattern);it!=std::sregex_iterator();++it) {
            const auto start=begin+static_cast<size_t>(it->position()),end=start+static_cast<size_t>(it->length());
            if(start>hit||end<=hit||start>=region.size()||region[start]!=0)continue;
            Marker result{it->str(),{},start};for(size_t i=1;i<it->size();++i)result.groups.push_back((*it)[i].str());return result;
        }
    }return {};
}
inline void identify(const std::string& source,const std::string& body,const std::vector<unsigned char>& region,std::vector<Finding>& out) {
    const auto get=[&](const char* literal,const std::regex& pattern){return marker(body,region,literal,pattern);};
    const auto emit=[&](const char* name,const char* category,const Marker& a,const Marker& b,const std::string& version="") {
        if(!a.value.empty()&&!b.value.empty())add_finding(out,{name,category,version,"Medium",{{source,a.value},{source,b.value}}});
    };
    static const std::regex vnode(R"(\.__v_isVNode\s{0,16}={2,3}\s{0,16}(?:!0|true)\b)");
    static const std::regex vue_app(R"(\.__vue_app__\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\b)");
    emit("Vue.js","Library",get("__v_isVNode",vnode),get("__vue_app__",vue_app));
    static const std::regex i18n_symbol(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}["'`]global-vue-i18n["'`]\s{0,16}\))rx");
    static const std::regex i18n_install(R"(\.__VUE_I18N_SYMBOL__\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\b)");
    emit("Vue I18n","Library",get("global-vue-i18n",i18n_symbol),get("__VUE_I18N_SYMBOL__",i18n_install));
    static const std::regex lucide_class(R"rx(\bclass\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}["'`]lucide["'`]\s{0,16},)rx");
    static const std::regex lucide_props(R"(\biconNode\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}absoluteStrokeWidth\s{0,16}:)");
    emit("Lucide","Library",get("lucide",lucide_class),get("absoluteStrokeWidth",lucide_props));
    static const std::regex pako_global(R"(\.pako\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}\))");
    static const std::regex pako_api(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.inflateRaw\s{0,16}=\s{0,16}function\()");
    static const std::regex pako_ungzip(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.ungzip\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\b)");
    const auto inflate=get(".inflateRaw",pako_api),ungzip=get(".ungzip",pako_ungzip);
    if(!inflate.groups.empty()&&!ungzip.groups.empty()&&inflate.groups[0]==ungzip.groups[0])emit("Pako","Library",get(".pako",pako_global),inflate);
    static const std::regex rlottie_binary(R"rx(\bwasmBinaryFile\s{0,16}=\s{0,16}["'`]rlottie-wasm\.wasm["'`])rx");
    static const std::regex rlottie_export(R"rx(\bModule\[\s{0,16}["'`]_lottie_load_from_data["'`]\s{0,16}\]\s{0,16}=\s{0,16}function\()rx");
    emit("rlottie","Library",get("rlottie-wasm.wasm",rlottie_binary),get("_lottie_load_from_data",rlottie_export));
    static const std::regex dotlottie_export(R"(\bexport\s{0,16}\{[^{}]{0,160}\bas\s{1,16}DotLottieWC\b)");
    static const std::regex dotlottie_api(R"(\._createDotLottieInstance\s{0,16}\(\s{0,16}\{)");
    emit("dotLottie Web Component","Library",get("DotLottieWC",dotlottie_export),get("_createDotLottieInstance",dotlottie_api));
    static const std::regex sentry_sdk(R"rx(\.name\s{0,16}\|\|\s{0,16}\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.name\s{0,16}=\s{0,16}[`"']sentry\.javascript\.)rx");
    static const std::regex sentry_packages(R"rx(\.packages\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.map\([^{}]{0,80}\{\s{0,16}name\s{0,16}:\s{0,16}`\$\{[A-Za-z_$][A-Za-z0-9_$]{0,80}\}:@sentry/)rx");
    emit("Sentry","Library",get("sentry.javascript.",sentry_sdk),get("@sentry/",sentry_packages));
    static const std::regex gtm_bootstrap(R"rx(\.push\(\s{0,16}\{\s{0,16}["'`]gtm\.start["'`]\s{0,16}:[^{}]{0,100}\bevent\s{0,16}:\s{0,16}["'`]gtm\.js["'`]\s{0,16}\}\s{0,16}\))rx");
    static const std::regex gtm_url(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`](?:https?:)?//www\.googletagmanager\.com/gtm\.js\?)rx");
    emit("Google Tag Manager","Tag management",get("gtm.start",gtm_bootstrap),get("www.googletagmanager.com/gtm.js",gtm_url));
    static const std::regex headless_state(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\[\s{0,16}[`"']data-headlessui-state[`"']\s{0,16}\]\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.join\()rx");
    static const std::regex headless_focus(R"rx(\bdocument\.documentElement\.dataset\.headlessuiFocusVisible\s{0,16}=\s{0,16}([`"'])\1)rx");
    emit("Headless UI","Library",get("data-headlessui-state",headless_state),get("headlessuiFocusVisible",headless_focus));
    static const std::regex floating_rects(R"(\.getElementRects\(\s{0,16}\{\s{0,16}reference\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}floating\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}strategy\s{0,16}:)");
    static const std::regex floating_middleware(R"(\bmiddlewareData\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}rects\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}platform\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}elements\s{0,16}:\s{0,16}\{\s{0,16}reference\s{0,16}:)");
    emit("Floating UI","Library",get("getElementRects",floating_rects),get("middlewareData",floating_middleware));
    static const std::regex turbopack_flag(R"(\bwindow\.next\s{0,16}=\s{0,16}\{[^{};]{0,120}\bturbopack\s{0,16}:\s{0,16}(?:!0|true)\b)");
    static const std::regex turbopack_loader(R"(\bself\.__turbopack_load_page_chunks__\s{0,16}=\s{0,16}function\()");
    emit("Turbopack","Build tool",get("turbopack",turbopack_flag),get("__turbopack_load_page_chunks__",turbopack_loader));
    static const std::regex prebid_init(R"rx(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.libLoaded\s{0,16}=\s{0,16}(?:!0|true)\s{0,16}[;,]\s{0,16}\1\.version\s{0,16}=\s{0,16}["'`]v?([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16}[;,]\s{0,16}\1\.adUnits\s{0,16}=\s{0,16}\1\.adUnits\s{0,16}\|\|\s{0,16}\[\s{0,16}\])rx");
    static const std::regex prebid_api(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}["'`]getHighestCpmBids["'`]\s{0,16},\s{0,16}function\([^()]{0,80}\)\s{0,16}\{\s{0,16}return\s{1,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.targeting\.getWinningBids\()rx");
    const auto prebid=get(".libLoaded",prebid_init);
    if(prebid.groups.size()==2)emit("Prebid","Library",prebid,get("getHighestCpmBids",prebid_api),prebid.groups[1]);
    static const std::regex ttag_dedent(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.conf\.setDedent\()");
    static const std::regex ttag_locale(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.conf\.setCurrentLocale\()");
    static const std::regex ttag_context(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.copyWithNewContext\()");
    const auto dedent=get(".conf.setDedent",ttag_dedent),locale=get(".conf.setCurrentLocale",ttag_locale),context=get(".copyWithNewContext",ttag_context);
    if(dedent.groups.size()==1&&locale.groups.size()==1&&context.groups.size()==1&&dedent.groups[0]==locale.groups[0]&&dedent.groups[0]==context.groups[0]) {
        emit("ttag","Library",dedent,locale);
        add_finding(out,{"ttag","Library","","Medium",{{source,context.value}}});
    }
    static const std::regex motion_appear(R"rx([=(]\s{0,16}["'`]framerAppearId["'`]\s{0,16}[;,)])rx");
    static const std::regex motion_portal(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\[\s{0,16}["'`]data-framer-portal-id["'`]\s{0,16}\])rx");
    emit("Motion (Framer Motion)","Library",get("framerAppearId",motion_appear),get("data-framer-portal-id",motion_portal));
    static const std::regex baseui_guard(R"rx([{,]\s{0,16}["'`]data-base-ui-focus-guard["'`]\s{0,16}:\s{0,16}(["'`])\1)rx");
    static const std::regex baseui_inert(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]data-base-ui-inert["'`])rx");
    emit("Base UI","Library",get("data-base-ui-focus-guard",baseui_guard),get("data-base-ui-inert",baseui_inert));
    static const std::regex datadog_intake(R"rx(\[\s{0,16}["'`]ddsource["'`]\s{0,16},\s{0,16}["'`]dd-api-key["'`]\s{0,16},\s{0,16}["'`]dd-request-id["'`]\s{0,16}\])rx");
    static const std::regex datadog_api(R"rx(\bversion\s{0,16}:\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16},\s{0,16}onReady\s{0,16}[(:])rx");
    const auto datadog=get("onReady",datadog_api);
    if(datadog.groups.size()==1)emit("Datadog Browser SDK","Library",get("dd-api-key",datadog_intake),datadog,datadog.groups[0]);
    static const std::regex query_client(R"rx(\bError\(\s{0,16}["'`]No QueryClient set, use QueryClientProvider to set one["'`]\s{0,16}\))rx");
    static const std::regex query_fn(R"rx(\bError\(\s{0,16}(?:`Missing queryFn: '\$\{|["'`]Missing queryFn: '["'`]\s{0,16}\+))rx");
    emit("TanStack Query","Library",get("No QueryClient set",query_client),get("Missing queryFn",query_fn));
    static const std::regex router_state(R"(\b__TSR_key\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\b)");
    static const std::regex router_scroll(R"rx((?:\.getItem\(|[=:])\s{0,16}["'`]tsr-scroll-restoration-v[0-9][0-9_]{0,7}["'`])rx");
    emit("TanStack Router","Framework",get("__TSR_key",router_state),get("tsr-scroll-restoration-v",router_scroll));
    static const std::regex intercom_stub(R"rx(\(\s{0,16}["'`]reattach_activator["'`]\s{0,16}\))rx");
    static const std::regex intercom_widget(R"rx(\.src\s{0,16}=\s{0,16}["'`]https://widget\.intercom\.io/widget/["'`]\s{0,16}\+)rx");
    emit("Intercom Messenger","Support / chat",get("reattach_activator",intercom_stub),get("widget.intercom.io/widget/",intercom_widget));
    static const std::regex hljs_prefix(R"rx(\bclassPrefix\s{0,16}:\s{0,16}["'`]hljs-["'`])rx");
    static const std::regex hljs_version(R"rx(\.versionString\s{0,16}=\s{0,16}([A-Za-z_$][A-Za-z0-9_$]{0,80}|["'`][0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}["'`]))rx");
    const auto hljs=get("classPrefix",hljs_prefix),hljs_export=get(".versionString",hljs_version);
    if(!hljs.value.empty()&&hljs_export.groups.size()==1) {
        const auto& name=hljs_export.groups[0];
        std::string version,declared;
        if(name.front()=='"'||name.front()=='\'')version=name.substr(1,name.size()-2);
        else {
            const auto needle=name+"=";
            const auto floor=hljs_export.position>32768?hljs_export.position-32768:0;
            for(auto hit=body.rfind(needle,hljs_export.position);hit!=std::string::npos&&hit>=floor;hit=hit?body.rfind(needle,hit-1):std::string::npos) {
                const auto before=hit?static_cast<unsigned char>(body[hit-1]):' ';
                if(std::isalnum(before)||before=='_'||before=='$'||before=='.'||region[hit]!=0)continue;
                static const std::regex constant(R"rx(^["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`])rx");
                std::smatch match;const auto tail=body.substr(hit+needle.size(),16);
                if(std::regex_search(tail,match,constant)){version=match[1].str();declared=needle+match[0].str();}
                break;
            }
        }
        emit("Highlight.js","Library",hljs,hljs_export,version);
        if(!declared.empty())add_finding(out,{"Highlight.js","Library",version,"Medium",{{source,declared}}});
    }
    const auto any=[&](std::initializer_list<const char*> literals,const std::regex& pattern) {
        for(const auto* literal:literals){auto found=marker(body,region,literal,pattern);if(!found.value.empty())return found;}
        return Marker{};
    };
    static const std::regex turbopack_chunks(R"rx(\(\s{0,16}globalThis(?:\.TURBOPACK|\[\s{0,16}["'`]TURBOPACK[A-Za-z0-9_$-]{0,120}["'`]\s{0,16}\])\s{0,16}\|\|\s{0,16}\(\s{0,16}globalThis(?:\.TURBOPACK|\[\s{0,16}["'`]TURBOPACK[A-Za-z0-9_$-]{0,120}["'`]\s{0,16}\])\s{0,16}=\s{0,16}\[\s{0,16}\]\s{0,16}\)\s{0,16}\)\s{0,16}\.push\()rx");
    static const std::regex turbopack_script(R"rx(\.push\(\s{0,16}\[\s{0,16}(?:["'`]object["'`]\s{0,16}={2,3}\s{0,16}typeof\s{1,16}document|typeof\s{1,16}document\s{0,16}={2,3}\s{0,16}["'`]object["'`])\s{0,16}\?\s{0,16}document\.currentScript\s{0,16}:)rx");
    emit("Turbopack","Build tool",get("TURBOPACK",turbopack_chunks),get("document.currentScript",turbopack_script));
    static const std::regex lucide_stroke(R"rx(\bstrokeWidth\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}\?\s{0,16}24\s{0,16}\*\s{0,16}Number\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}\)\s{0,16}/\s{0,16}Number\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}\)\s{0,16}:)rx");
    static const std::regex lucide_component(R"rx(\bclassName\s{0,16}:\s{0,16}(?:[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}|\[\s{0,16})["'`]lucide["'`]\s{0,16},)rx");
    emit("Lucide","Library",any({"24*Number(","24 * Number("},lucide_stroke),any({"\"lucide\",","'lucide',","`lucide`,"},lucide_component));
    static const std::regex lucide_scaled(R"rx(\?\s{0,16}Number\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}(?:\s{0,16}\?\?\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80})?\s{0,16}\)\s{0,16}\*\s{0,16}24\s{0,16}/\s{0,16}Number\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}(?:\s{0,16}\?\?\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80})?\s{0,16}\)\s{0,16}:)rx");
    emit("Lucide","Library",any({")*24/Number(",") * 24 / Number("},lucide_scaled),any({"\"lucide\",","'lucide',","`lucide`,"},lucide_component));
    static const std::regex baseui_error(R"rx(\bnew\s{1,16}URL\(\s{0,16}["'`]https://base-ui\.com/production-error["'`]\s{0,16}\))rx");
    static const std::regex baseui_attribute(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]data-base-ui-[a-z-]{3,40}["'`])rx");
    emit("Base UI","Library",get("base-ui.com/production-error",baseui_error),get("data-base-ui-",baseui_attribute));
    static const std::regex baseui_helper(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}["'`]https://base-ui\.com/production-error["'`]\s{0,16},\s{0,16}["'`]Base UI["'`]\s{0,16}\))rx");
    if(const auto helper=get("base-ui.com/production-error",baseui_helper);!helper.value.empty()) {
        Finding found{"Base UI","Library",{},"Medium",{{source,helper.value}}};
        if(const auto attribute=get("data-base-ui-",baseui_attribute);!attribute.value.empty())found.evidence.push_back({source,attribute.value});
        add_finding(out,std::move(found));
    }
    static const std::regex motion_projection(R"rx(\.projection\s{0,16}=\s{0,16}new\s{1,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.latestValues\s{0,16},)rx");
    emit("Motion (Framer Motion)","Library",get("data-framer-portal-id",motion_portal),any({".projection=new ",".projection = new "},motion_projection));
    static const std::regex vitals_id(R"rx(\bMath\.floor\(\s{0,16}(?:8999999999999\s{0,16}\*\s{0,16}Math\.random\(\s{0,16}\)|Math\.random\(\s{0,16}\)\s{0,16}\*\s{0,16}\(\s{0,16}9e12\s{0,16}-\s{0,16}1\s{0,16}\))\s{0,16}\)\s{0,16}\+\s{0,16}(?:1e12|0xe8d4a51000|1000000000000)\b)rx");
    static const std::regex vitals_metric(R"rx(\bdelta\s{0,16}:\s{0,16}0\s{0,16},\s{0,16}entries\s{0,16}:\s{0,16}\[\s{0,16}\]\s{0,16},\s{0,16}id\s{0,16}:)rx");
    const auto vitals=any({"entries:[]","entries: []"},vitals_metric);
    if(!vitals.value.empty()) {
        const auto begin=vitals.position>1600?vitals.position-1600:0;
        const auto window=body.substr(begin,2400);
        for(auto id=std::sregex_iterator(window.begin(),window.end(),vitals_id);id!=std::sregex_iterator();++id) {
            if(region[begin+static_cast<size_t>(id->position())]==2)continue;
            add_finding(out,{"web-vitals","Library","","Medium",{{source,vitals.value},{source,id->str()}}});
            break;
        }
    }
    static const std::regex statsig_global(R"rx(\b_getStatsigGlobal\s{0,16}=\s{0,16}(?:\(\s{0,16}\)\s{0,16}=>|function\s{0,16}\())rx");
    static const std::regex statsig_exposure(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]statsig::gate_exposure["'`])rx");
    emit("Statsig","Feature management",get("_getStatsigGlobal",statsig_global),get("statsig::gate_exposure",statsig_exposure));
    static const std::regex kasada_configure(R"rx(\bwindow\.KPSDK\.configure\()rx");
    static const std::regex kasada_event(R"rx(\.addEventListener\(\s{0,16}["'`]kpsdk-(?:load|ready)["'`]\s{0,16},)rx");
    emit("Kasada","Bot protection",get("KPSDK.configure",kasada_configure),get("kpsdk-",kasada_event));
    static const std::regex hcaptcha_hook(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]hCaptchaOnLoad["'`])rx");
    static const std::regex hcaptcha_query(R"rx(\brecaptchacompat\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.recaptchacompat\b)rx");
    emit("hCaptcha","Bot protection",get("hCaptchaOnLoad",hcaptcha_hook),get("recaptchacompat",hcaptcha_query));
    static const std::regex recaptcha_source(R"rx(\.src\s{0,16}=\s{0,16}["'`]https://www\.(?:google\.com|recaptcha\.net)/recaptcha/(?:api|enterprise)\.js\b)rx");
    static const std::regex recaptcha_query(R"rx(\.src\s{0,16}=\s{0,16}["'`]https://www\.(?:google\.com|recaptcha\.net)/recaptcha/(?:api|enterprise)\.js\?(?:render|onload|hl)=)rx");
    emit("reCAPTCHA","Bot protection",get("/recaptcha/",recaptcha_source),get("/recaptcha/",recaptcha_query));
    static const std::regex styled_attr(R"rx(\.SC_ATTR\s{0,16}\)\s{0,16}\|\|\s{0,16}["'`]data-styled["'`])rx");
    static const std::regex styled_version(R"rx(=\s{0,16}["'`]data-styled-version["'`]\s{0,16}[,;]\s{0,16}(?:(?:var|let|const)\s{1,16})?[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`])rx");
    const auto styled=get("data-styled-version",styled_version);
    if(styled.groups.size()==1)emit("styled-components","Library",get("SC_ATTR",styled_attr),styled,styled.groups[0]);
    static const std::regex emotion_sheet(R"rx(\.setAttribute\(\s{0,16}["'`]data-emotion["'`]\s{0,16},\s{0,16}this\.key\s{0,16}\))rx");
    static const std::regex emotion_hydrate(R"rx(\.setAttribute\(\s{0,16}["'`]data-emotion["'`]\s{0,16},\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}\)\s{0,16}[,;]\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.hydrate\()rx");
    emit("Emotion","Library",get("data-emotion",emotion_sheet),get(".hydrate(",emotion_hydrate));
    static const std::regex sanity_defaults(R"rx(\bapiHost\s{0,16}:\s{0,16}["'`]https://api\.sanity\.io["'`]\s{0,16},\s{0,16}apiVersion\s{0,16}:)rx");
    static const std::regex sanity_cdn(R"rx((?:\.isDefaultApi\s{0,16}\?\s{0,16}["'`]apicdn\.sanity\.io["'`]\s{0,16}:|\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]apicdn\.sanity\.io["'`]))rx");
    emit("Sanity","CMS",get("api.sanity.io",sanity_defaults),get("apicdn.sanity.io",sanity_cdn));
    static const std::regex svelte_five(R"rx(\(\s{0,16}window\.__svelte\s{0,16}\?\?=\s{0,16}\{\s{0,16}\}\s{0,16}\)\.v\s{0,16}\?\?=\s{0,16}new\s{1,16}Set\s{0,16}(?:\(\s{0,16}\))?\s{0,16}\)\.add\(\s{0,16}["'`]([0-9]{1,2})["'`]\s{0,16}\))rx");
    static const std::regex svelte_four(R"rx(\(\s{0,16}window\.__svelte\s{0,16}\|\|\s{0,16}\(\s{0,16}window\.__svelte\s{0,16}=\s{0,16}\{\s{0,16}v\s{0,16}:\s{0,16}new\s{1,16}Set\s{0,16}(?:\(\s{0,16}\))?\s{0,16}\}\s{0,16}\)\s{0,16}\)\.v\.add\(\s{0,16}["'`]([0-9]{1,2})["'`]\s{0,16}\))rx");
    auto svelte=get("__svelte",svelte_five);if(svelte.value.empty())svelte=get("__svelte",svelte_four);
    if(svelte.groups.size()==1)add_finding(out,{"Svelte","Framework",svelte.groups[0],"High",{{source,svelte.value}}});
    static const std::regex three_instances(R"rx(\bwindow\.__THREE__\s{0,16}(?:\?|\)\s{0,16}\{?)\s{0,16}[A-Za-z_$][A-Za-z0-9_$.]{0,80}\(\s{0,16}["'`]WARNING: Multiple instances of Three\.js being imported)rx");
    static const std::regex three_register(R"rx(\bnew\s{1,16}CustomEvent\(\s{0,16}["'`]register["'`]\s{0,16},\s{0,16}\{\s{0,16}detail\s{0,16}:\s{0,16}\{\s{0,16}revision\s{0,16}:\s{0,16}(?:["'`]([0-9]{2,4})["'`]|[A-Za-z_$][A-Za-z0-9_$]{0,80})\s{0,16}\})rx");
    const auto three=any({"{detail:{revision:","detail: { revision:"},three_register);
    emit("three.js","Library",get("Multiple instances of Three.js",three_instances),three,three.groups.empty()?std::string{}:three.groups[0]);
    static const std::regex posthog_state(R"rx(\.__request_queue\s{0,16}=\s{0,16}\[\s{0,16}\]\s{0,16}[,;]\s{0,16}this\.__loaded\s{0,16}=\s{0,16}(?:!1|false)\b)rx");
    static const std::regex posthog_consent(R"rx((?::|\|\|)\s{0,16}["'`]__ph_opt_in_out_["'`]\s{0,16}\)?\s{0,16}\+)rx");
    emit("PostHog","Analytics",get("__request_queue",posthog_state),get("__ph_opt_in_out_",posthog_consent));
    static const std::regex apollo_client(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\.__APOLLO_CLIENT__\s{0,16}=\s{0,16}this\b)rx");
    static const std::regex apollo_devtools(R"rx((?:!\s{0,16}window\.__APOLLO_DEVTOOLS_GLOBAL_HOOK__|\bSymbol\.for\(\s{0,16}["'`]apollo\.devtools["'`]\s{0,16}\)))rx");
    emit("Apollo Client","Library",get("__APOLLO_CLIENT__",apollo_client),any({"__APOLLO_DEVTOOLS_GLOBAL_HOOK__","apollo.devtools"},apollo_devtools));
    static const std::regex rudder_name(R"rx(=\s{0,16}["'`]RudderLabs JavaScript SDK["'`]\s{0,16}[,;]\s{0,16}(?:(?:var|let|const)\s{1,16})?[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`])rx");
    static const std::regex rudder_module(R"rx(=\s{0,16}["'`]RudderJS-Initiated["'`])rx");
    const auto rudder=get("RudderLabs JavaScript SDK",rudder_name);
    if(rudder.groups.size()==1)emit("RudderStack","Customer data",rudder,get("RudderJS-Initiated",rudder_module),rudder.groups[0]);
    static const std::regex sift_push(R"rx(\b(?:window\.)?_sift\.push\(\s{0,16}\[\s{0,16}["'`]_set(?:Account|UserId|SessionId)["'`]\s{0,16},)rx");
    static const std::regex sift_queue(R"rx((?:\b_sift\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\._sift\s{0,16}\|\|\s{0,16}\[\s{0,16}\]|[(&|,;]\s{0,16}window\._sift\s{0,16}[)&|]))rx");
    emit("Sift","Fraud prevention",get("_sift.push",sift_push),any({"window._sift)","window._sift&&","._sift||","._sift ||"},sift_queue));
    static const std::regex vercel_speed(R"rx(\bsdkn\s{0,16}:\s{0,16}["'`]@vercel/speed-insights["'`][^;]{0,160}\bsdkv\s{0,16}:\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(?:-[A-Za-z0-9.]{1,20})?)["'`])rx");
    static const std::regex vercel_speed_script(R"rx([:?(,=]\s{0,16}["'`]/_vercel/speed-insights/script\.js["'`])rx");
    const auto speed=get("@vercel/speed-insights",vercel_speed);
    if(speed.groups.size()==1)emit("Vercel Speed Insights","Analytics",speed,get("/_vercel/speed-insights/script.js",vercel_speed_script),speed.groups[0]);
    static const std::regex vercel_analytics(R"rx(\bsdkn\s{0,16}:\s{0,16}["'`]@vercel/analytics["'`][^;]{0,160}\bsdkv\s{0,16}:\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(?:-[A-Za-z0-9.]{1,20})?)["'`])rx");
    static const std::regex vercel_analytics_script(R"rx([:?(,=]\s{0,16}["'`]/_vercel/insights/script\.js["'`])rx");
    const auto analytics=get("@vercel/analytics",vercel_analytics);
    if(analytics.groups.size()==1)emit("Vercel Web Analytics","Analytics",analytics,get("/_vercel/insights/script.js",vercel_analytics_script),analytics.groups[0]);
    static const std::regex onetrust_stub(R"rx(\bOneTrustStub\s{0,16}=\s{0,16}[A-Za-z_$(!])rx");
    static const std::regex onetrust_cookie(R"rx(\.optanonCookieName\s{0,16}=\s{0,16}["'`]OptanonConsent["'`])rx");
    emit("OneTrust","Consent management",get("OneTrustStub",onetrust_stub),get("optanonCookieName",onetrust_cookie));
    static const std::regex i18next_logger(R"rx(\.prefix\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.prefix\s{0,16}\|\|\s{0,16}["'`]i18next:["'`])rx");
    static const std::regex i18next_event(R"rx(\.emit\(\s{0,16}["'`]languageChanged["'`]\s{0,16},)rx");
    emit("i18next","Library",get("i18next:",i18next_logger),get("languageChanged",i18next_event));
    static const std::regex dayjs_format(R"rx((?:\|\||=)\s{0,16}["'`]YYYY-MM-DDTHH:mm:ssZ["'`])rx");
    static const std::regex dayjs_locale(R"rx(\bthis\.\$L\s{0,16}=\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\.locale\s{0,16},\s{0,16}null\s{0,16},\s{0,16}(?:!0|true)\s{0,16}\))rx");
    emit("Day.js","Library",get("YYYY-MM-DDTHH:mm:ssZ",dayjs_format),any({"this.$L=","this.$L ="},dayjs_locale));
    static const std::regex sensors_global(R"rx(\bwindow\.sensorsDataAnalytic201505\s{0,16}=\s{0,16}[A-Za-z_$])rx");
    static const std::regex sensors_version(R"rx(=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16}[,;]\s{0,16}(?:(?:var|let|const)\s{1,16})?[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]sensorsdata_domain_test["'`])rx");
    static const std::regex sensors_test(R"rx(=\s{0,16}["'`]sensorsdata_domain_test["'`])rx");
    const auto sensors=any({"window.sensorsDataAnalytic201505=","window.sensorsDataAnalytic201505 ="},sensors_global);
    auto sensors_key=get("sensorsdata_domain_test",sensors_version);
    const auto sensors_release=sensors_key.groups.size()==1?sensors_key.groups[0]:std::string{};
    if(sensors_key.value.empty())sensors_key=get("sensorsdata_domain_test",sensors_test);
    emit("Sensors Analytics","Analytics",sensors,sensors_key,sensors_release);
    static const std::regex next_release(R"rx(\bwindow\.next\s{0,16}=\s{0,16}\{\s{0,16}version\s{0,16}:\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}(?:-[A-Za-z0-9.]{1,40})?)["'`]\s{0,16},\s{0,16}appDir\s{0,16}:)rx");
    const auto next=any({"window.next={","window.next = {"},next_release);
    if(next.groups.size()==1)add_finding(out,{"Next.js","Framework",next.groups[0],"High",{{source,next.value}}});
    static const std::regex react_prerelease(R"rx(\bversion\s{0,16}:\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}-[A-Za-z0-9.-]{1,60})["'`]\s{0,16},\s{0,16}rendererPackageName\s{0,16}:\s{0,16}["'`]react-dom["'`])rx");
    const auto react=get("rendererPackageName",react_prerelease);
    if(react.groups.size()==1)add_finding(out,{"React","Library",react.groups[0],"High",{{source,react.value}}});
    static const std::regex radix_global(R"rx(\bwindow\[\s{0,16}Symbol\.for\(\s{0,16}["'`]radix-ui["'`]\s{0,16}\)\s{0,16}\]\s{0,16}=\s{0,16}(?:!0|true)\b)rx");
    static const std::regex radix_slot(R"rx(\bSymbol(?:\.for)?\(\s{0,16}["'`]radix\.slottable["'`]\s{0,16}\))rx");
    static const std::regex radix_identity(R"rx(\.__radixId\s{0,16}={2,3}\s{0,16}[A-Za-z_$])rx");
    static const std::regex radix_guard(R"rx(\.setAttribute\(\s{0,16}["'`]data-radix-focus-guard["'`]\s{0,16},)rx");
    const auto radix=any({"Symbol.for(\"radix-ui\")","Symbol.for('radix-ui')","Symbol.for(`radix-ui`)"},radix_global);
    const auto radix_a=get("radix.slottable",radix_slot),radix_b=get(".__radixId",radix_identity),radix_c=get("data-radix-focus-guard",radix_guard);
    if(!radix.value.empty()) {
        Finding found{"Radix UI","Library",{},"Medium",{{source,radix.value}}};
        for(const auto* extra:{&radix_a,&radix_c})if(!extra->value.empty()){found.evidence.push_back({source,extra->value});break;}
        add_finding(out,std::move(found));
    } else if(!radix_a.value.empty())emit("Radix UI","Library",radix_a,radix_b.value.empty()?radix_c:radix_b);
    static const std::regex mobx_error(R"rx(\bError\(\s{0,16}(?:(?:typeof\s{1,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}={2,3}\s{0,16}["'`]number["'`]|["'`]number["'`]\s{0,16}={2,3}\s{0,16}typeof\s{1,16}[A-Za-z_$][A-Za-z0-9_$]{0,80})\s{0,16}\?\s{0,16})?["'`]\[MobX\] minified error nr: )rx");
    static const std::regex mobx_symbol(R"rx(\bSymbol\(\s{0,16}["'`]mobx administration["'`]\s{0,16}\))rx");
    emit("MobX","Library",get("[MobX] minified error nr: ",mobx_error),get("mobx administration",mobx_symbol));
    static const std::regex rxjs_error(R"rx(\.name\s{0,16}=\s{0,16}["'`]UnsubscriptionError["'`]\s{0,16}[,;]\s{0,16}this\.errors\s{0,16}=)rx");
    static const std::regex rxjs_subscription(R"rx(\bthis\.(?:_parentage\s{0,16}=\s{0,16}null\s{0,16}[,;]\s{0,16}this\._finalizers|_parentOrParents\s{0,16}=\s{0,16}null\s{0,16}[,;]\s{0,16}this\._subscriptions)\s{0,16}=\s{0,16}null\b)rx");
    emit("RxJS","Library",get("UnsubscriptionError",rxjs_error),any({"this._parentage","this._parentOrParents"},rxjs_subscription));
    static const std::regex loadable_chunks(R"rx(\(\s{0,16}(self|window|globalThis)\.__LOADABLE_LOADED_CHUNKS__\s{0,16}=\s{0,16}\1\.__LOADABLE_LOADED_CHUNKS__\s{0,16}\|\|\s{0,16}\[\s{0,16}\]\s{0,16}\)\s{0,16}\.push\()rx");
    if(const auto loadable=get("__LOADABLE_LOADED_CHUNKS__",loadable_chunks);!loadable.value.empty())
        add_finding(out,{"Loadable Components","Library",{},"Medium",{{source,loadable.value}}});
    static const std::regex zaraz_guard(R"rx(\.zaraz\s{0,16}\)\s{0,16}console\.error\(\s{0,16}["'`]zaraz is loaded twice["'`]\s{0,16}\))rx");
    static const std::regex zaraz_release(R"rx(\.zaraz\._v\s{0,16}=\s{0,16}["'`][0-9]{1,4}["'`])rx");
    emit("Cloudflare Zaraz","Tag management",get("zaraz is loaded twice",zaraz_guard),get(".zaraz._v",zaraz_release));
    static const std::regex rive_package(R"rx(\bJSON\.parse\(\s{0,16}["'`]\{\s{0,8}"name"\s{0,8}:\s{0,8}"@rive-app/[a-z0-9-]{3,24}"\s{0,8},\s{0,8}"version"\s{0,8}:\s{0,8}"([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})")rx");
    if(const auto rive=get("@rive-app/",rive_package);rive.groups.size()==1)
        add_finding(out,{"Rive","Library",rive.groups[0],"Medium",{{source,rive.value}}});
    static const std::regex segment_host(R"rx(=\s{0,16}["'`]api\.segment\.io/v1["'`])rx");
    static const std::regex segment_cdn(R"rx(\|\|\s{0,16}["'`]https://cdn\.segment\.com["'`])rx");
    static const std::regex segment_release(R"rx(=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16};\s{0,16}function\s{1,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\(\s{0,16}\)\s{0,16}\{\s{0,16}return\s{0,16}["'`](?:npm|web)["'`])rx");
    const auto segment_version=any({"return\"npm\"","return\"web\"","return \"npm\"","return \"web\""},segment_release);
    emit("Twilio Segment","Analytics",get("api.segment.io/v1",segment_host),get("https://cdn.segment.com",segment_cdn),segment_version.groups.size()==1?segment_version.groups[0]:std::string{});
    static const std::regex lucide_icon(R"rx(\bstrokeLinejoin\s{0,16}:\s{0,16}["'`]round["'`]\s{0,16},\s{0,16}className\s{0,16}:\s{0,16}["'`]lucide lucide-[a-z0-9-]{2,48}["'`])rx");
    if(const auto drawn=get("lucide lucide-",lucide_icon);!drawn.value.empty())
        add_finding(out,{"Lucide","Library",{},"Medium",{{source,drawn.value}}});
    static const std::regex lodash_sentinel(R"rx(\b[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]__lodash_hash_undefined__["'`])rx");
    static const std::regex lodash_store(R"rx(\bthis\.__data__\s{0,16}=\s{0,16}[A-Za-z_$({\[])rx");
    emit("lodash","Library",get("__lodash_hash_undefined__",lodash_sentinel),any({"this.__data__=","this.__data__ ="},lodash_store));
    static const std::regex youtube_frame(R"rx(\(\s{0,16}["'`]iframe["'`]\s{0,16},\s{0,16}\{[^{}]{0,240}?\bsrc\s{0,16}:\s{0,16}["'`]https://www\.youtube(?:-nocookie)?\.com/embed/)rx");
    if(const auto frame=any({"youtube.com/embed/","youtube-nocookie.com/embed/"},youtube_frame);!frame.value.empty())
        add_finding(out,{"YouTube embed","Video",{},"Medium",{{source,frame.value.substr(0,220)}}});
    static const std::regex router_release(R"rx(\bwindow\.__reactRouterVersion\s{0,16}=\s{0,16}["'`]([0-9]{1,3}(?:\.[0-9]{1,3}){0,2}(?:-[A-Za-z0-9.]{1,40})?)["'`])rx");
    if(const auto router=any({"window.__reactRouterVersion=","window.__reactRouterVersion ="},router_release);router.groups.size()==1)
        add_finding(out,{"React Router","Framework",router.groups[0],"High",{{source,router.value}}});
    static const std::regex redux_init(R"rx(\bINIT\s{0,16}:\s{0,16}(?:["']@@redux/INIT["']\s{0,16}\+|`@@redux/INIT\$\{))rx");
    static const std::regex redux_replace(R"rx(\bREPLACE\s{0,16}:\s{0,16}(?:["']@@redux/REPLACE["']\s{0,16}\+|`@@redux/REPLACE\$\{))rx");
    emit("Redux","Library",get("@@redux/INIT",redux_init),get("@@redux/REPLACE",redux_replace));
    static const std::regex optimizely_logger(R"rx(\bthis\.prefix\s{0,16}=\s{0,16}[^;{}"'`]{0,60}["'`]\[OPTIMIZELY\]["'`])rx");
    static const std::regex optimizely_notice(R"rx(\bOPTIMIZELY_CONFIG_UPDATE\s{0,16}[:=]\s{0,16}["'`]OPTIMIZELY_CONFIG_UPDATE["'`])rx");
    static const std::regex optimizely_engine(R"rx(=\s{0,16}["'`]javascript-sdk["'`]\s{0,16},\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]node-sdk["'`]\s{0,16},\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`])rx");
    static const std::regex optimizely_control(R"rx(=\s{0,16}["'`]\$opt_bot_filtering["'`]\s{0,16},\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]\$opt_bucketing_id["'`]\s{0,16},\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`])rx");
    auto optimizely_release=get("javascript-sdk",optimizely_engine);
    if(optimizely_release.value.empty())optimizely_release=get("$opt_bucketing_id",optimizely_control);
    emit("Optimizely","Experimentation",get("[OPTIMIZELY]",optimizely_logger),get("OPTIMIZELY_CONFIG_UPDATE",optimizely_notice),
         optimizely_release.groups.size()==1?optimizely_release.groups[0]:std::string{});
    static const std::regex mparticle_store(R"rx(\blocalStorageName\s{0,16}:\s{0,16}["'`]mprtcl-api["'`])rx");
    static const std::regex mparticle_service(R"rx(\bv[0-9]SecureServiceUrl\s{0,16}:\s{0,16}["'`]jssdks\.mparticle\.com/v[0-9]/JS/["'`])rx");
    static const std::regex mparticle_vendor(R"rx((?:\bsdkVersion\s{0,16}:|=)\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16},\s{0,16}(?:sdkVendor\s{0,16}:|[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=)\s{0,16}["'`]mparticle["'`])rx");
    static const std::regex mparticle_named(R"rx(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16},\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}=\s{0,16}\{\s{0,16}sdkVersion\s{0,16}:\s{0,16}\1\s{0,16},\s{0,16}sdkVendor\s{0,16}:\s{0,16}["'`]mparticle["'`])rx");
    const auto mparticle_release=any({"\"mparticle\"","'mparticle'","`mparticle`"},mparticle_vendor);
    const auto mparticle_constant=any({"\"mparticle\"","'mparticle'","`mparticle`"},mparticle_named);
    emit("mParticle","Customer data",get("mprtcl-api",mparticle_store),get("jssdks.mparticle.com/v",mparticle_service),
         mparticle_release.groups.size()==1?mparticle_release.groups[0]:mparticle_constant.groups.size()==2?mparticle_constant.groups[1]:std::string{});
    static const std::regex require_names(R"rx(\bvar\s{1,16}requirejs\s{0,16},\s{0,16}require\s{0,16},\s{0,16}define\s{0,16};)rx");
    static const std::regex require_node(R"rx(\.setAttribute\(\s{0,16}["'`]data-requiremodule["'`]\s{0,16},)rx");
    static const std::regex require_release(R"rx(\bversion\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16},\s{0,16}commentRegExp\s{0,16}=)rx");
    const auto require_version=get("commentRegExp",require_release);
    emit("RequireJS","Library",get("requirejs",require_names),get("data-requiremodule",require_node),
         require_version.groups.size()==1?require_version.groups[0]:std::string{});
    static const std::regex algolia_agent(R"rx(\bvalue\s{0,16}:\s{0,16}(?:["']Algolia for JavaScript \(["']\s{0,16}(?:\.concat\(|\+)|`Algolia for JavaScript \(\$\{))rx");
    static const std::regex algolia_headers(R"rx(\{\s{0,16}["'`]x-algolia-api-key["'`]\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16},\s{0,16}["'`]x-algolia-application-id["'`]\s{0,16}:\s{0,16}[A-Za-z_$][A-Za-z0-9_$]{0,80}\s{0,16}\})rx");
    emit("Algolia","Search",get("Algolia for JavaScript (",algolia_agent),get("x-algolia-application-id",algolia_headers));
    if(body.find("dompurify")!=std::string::npos) {
        static const std::regex purify_version(R"rx(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.version\s{0,16}=\s{0,16}["'`]([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3})["'`]\s{0,16},\s{0,16}\1\.removed\s{0,16}=\s{0,16}\[\s{0,16}\])rx");
        static const std::regex purify_sanitize(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.sanitize\s{0,16}=\s{0,16}function\()");
        static const std::regex purify_hooks(R"(\b([A-Za-z_$][A-Za-z0-9_$]{0,80})\.removeAllHooks\s{0,16}=\s{0,16}function\()");
        const auto v=get(".version",purify_version),s=get(".sanitize",purify_sanitize),h=get(".removeAllHooks",purify_hooks);
        if(v.groups.size()==2&&s.groups.size()==1&&h.groups.size()==1&&v.groups[0]==s.groups[0]&&v.groups[0]==h.groups[0]) {
            emit("DOMPurify","Library",v,s,v.groups[1]);
            add_finding(out,{"DOMPurify","Library",v.groups[1],"Medium",{{source,h.value}}});
        }
    }
}
}
