
void site_comparison_checks() {
    struct Library {const char* name;const char* body;const char* version;};
    const Library libraries[]={
        {"Turbopack","(globalThis.TURBOPACK||(globalThis.TURBOPACK=[])).push([\"object\"==typeof document?document.currentScript:void 0,7001,(e,t,r)=>{r.value=1}]);",""},
        {"Turbopack","(globalThis[\"TURBOPACK_remote_chunk_loading_global_fixture-app\"]||(globalThis[\"TURBOPACK_remote_chunk_loading_global_fixture-app\"]=[])).push([\"object\"==typeof document?document.currentScript:void 0,7001,e=>{}]);",""},
        {"Lucide","const Icon=forwardRef(({size:s=24,strokeWidth:w=2,absoluteStrokeWidth:a,className:c=\"\",iconNode:n,...rest},ref)=>createElement(\"svg\",{ref,width:s,height:s,strokeWidth:a?24*Number(w)/Number(s):w,className:merge(\"lucide\",c),...rest}));",""},
        {"Base UI","function message(code,...args){const url=new URL(\"https://base-ui.com/production-error\");url.searchParams.set(\"code\",code.toString());return url}const focusable=\"data-base-ui-focusable\";",""},
        {"Motion (Framer Motion)","function mount(node,props){node.projection=new Projection(node.latestValues,props[\"data-framer-portal-id\"]?void 0:closest(node.parent));}",""},
        {"web-vitals","const metric=(name,value)=>({name,value,rating:\"good\",delta:0,entries:[],id:\"v3-\".concat(Date.now(),\"-\").concat(Math.floor(8999999999999*Math.random())+1e12),navigationType:type});",""},
        {"web-vitals","const metric=(name,value)=>({name,value,rating:\"good\",delta:0,entries:[],id:`v5-${Date.now()}-${Math.floor(8999999999999*Math.random())+1e12}`,navigationType:type});",""},
        {"Statsig","api._getStatsigGlobal=()=>{try{return typeof __STATSIG__!==\"undefined\"?__STATSIG__:fallback}catch(e){return fallback}};const config=\"statsig::config_exposure\",gate=\"statsig::gate_exposure\";",""},
        {"Kasada","function configure(routes){window.KPSDK.configure(routes)}document.addEventListener(\"kpsdk-load\",configure,{once:!0});",""},
        {"styled-components","const attr=typeof process!==\"undefined\"&&(process.env.REACT_APP_SC_ATTR||process.env.SC_ATTR)||\"data-styled\",active=\"active\",versionAttr=\"data-styled-version\",release=\"6.99.2\";","6.99.2"},
        {"Emotion","class Sheet{create(){const tag=document.createElement(\"style\");tag.setAttribute(\"data-emotion\",this.key);return tag}}function adopt(node,key,sheet){node.setAttribute(\"data-emotion\",key),sheet.hydrate([node])}",""},
        {"Sanity","const defaults={apiHost:\"https://api.sanity.io\",apiVersion:\"1\",useProjectHostname:!0};function urls(c,host){const cdn=c.isDefaultApi?\"apicdn.sanity.io\":host;return cdn}",""},
        {"three.js","typeof __THREE_DEVTOOLS__<\"u\"&&__THREE_DEVTOOLS__.dispatchEvent(new CustomEvent(\"register\",{detail:{revision:\"199\"}})),typeof window<\"u\"&&(window.__THREE__?console.warn(\"WARNING: Multiple instances of Three.js being imported.\"):window.__THREE__=\"199\");","199"},
        {"PostHog","class Client{constructor(){this.__request_queue=[],this.__loaded=!1;this.config={}}name(c,token){return c.consent_persistence_name||(c.prefix?c.prefix+token:\"__ph_opt_in_out_\"+token)}}",""},
        {"Apollo Client","class Client{constructor(o){o.connect&&typeof window===\"object\"&&(window.__APOLLO_CLIENT__=this);if(!warned&&!window.__APOLLO_DEVTOOLS_GLOBAL_HOOK__)hint()}}",""},
        {"RudderStack","const appName=\"RudderLabs JavaScript SDK\",appVersion=\"3.99.1\",moduleType=\"RudderJS-Initiated\";","3.99.1"},
        {"Sift","async function user(load){const e=await load();if(e,window._sift){window._sift.push([\"_setUserId\",e.id])}return e}",""},
        {"Vercel Speed Insights","function config(n){const dataset={sdkn:\"@vercel/speed-insights\"+(n.framework?\"/\"+n.framework:\"\"),sdkv:\"2.99.0\"};return{src:n.basePath?join(n.basePath):\"/_vercel/speed-insights/script.js\",dataset}}","2.99.0"},
        {"Vercel Web Analytics","function config(n){const dataset={sdkn:\"@vercel/analytics\"+(n.framework?\"/\"+n.framework:\"\"),sdkv:\"2.99.0-canary.5\"};return{src:n.basePath?join(n.basePath):\"/_vercel/insights/script.js\",dataset}}","2.99.0-canary.5"},
        {"OneTrust","var OneTrustStub=(t=>{var state=new function(){this.optanonCookieName=\"OptanonConsent\",this.stubFileName=\"otSDKStub\"};return t})(window);",""}
    };
    for(const auto& [name,body,version]:libraries) {
        const auto found=script(body);check(has(found,name),std::string(name)+" paired code markers");
        for(const auto& f:found)if(f.name==name) {
            check(f.evidence.size()>=2,std::string(name)+" retains both matched values");
            check(f.version==version,std::string(name)+" version comes only from tied metadata");
            for(const auto& e:f.evidence)check(e.source=="https://unseen.example/opaque","no target or filename dependence");
        }
        check(!has(script("/*"+std::string(body)+"*/"),name),std::string(name)+" commented example excluded");
        std::string quoted="const documentation=\"";
        for(const char ch:std::string(body)){if(ch=='\\'||ch=='\"')quoted+='\\';quoted+=ch;}
        quoted+="\";";
        check(!has(script(quoted),name),std::string(name)+" quoted code excluded");
        check(!has(script(std::string(2*1024*1024,' ')+body),name),std::string(name)+" markers beyond cap excluded");
    }
    const auto svelte=script("typeof window<\"u\"&&((window.__svelte?" "?={}).v?" "?=new Set).add(\"5\");");
    check(has(svelte,"Svelte"),"Svelte version registration");
    for(const auto& f:svelte)if(f.name=="Svelte")check(f.version=="5"&&f.category=="Framework","Svelte registers its own major version");
    const auto svelte_four=script("typeof window<\"u\"&&(window.__svelte||(window.__svelte={v:new Set})).v.add(\"4\");");
    check(has(svelte_four,"Svelte"),"earlier Svelte registration form");
    for(const auto& f:svelte_four)if(f.name=="Svelte")check(f.version=="4","earlier registration supplies its own version");
    check(!has(script("const note=\"((window.__svelte?" "?={}).v?" "?=new Set).add('5')\";"),"Svelte"),"quoted Svelte registration excluded");
    check(!has(script("if(window.__svelte)console.log(window.__svelte.v);const cls='svelte-abc123';"),"Svelte"),"reading the registry and a class name are not Svelte");
    check(!has(script("const name='TURBOPACK';globalThis.TURBOPACK_note=1;items.push([\"object\"==typeof document?document.currentScript:void 0]);"),"Turbopack"),"a name and an unrelated push are not Turbopack chunks");
    check(!has(script("(globalThis.TURBOPACK||(globalThis.TURBOPACK=[])).push([1,2,3]);"),"Turbopack"),"chunk registry without the chunk script argument excluded");
    check(!has(script("const cls=cx(\"lucide\",extra);const width=absolute?24*Number(stroke)/Number(size):stroke;"),"Lucide"),"Lucide needs the icon component properties");
    check(!has(script("const docs=\"https://base-ui.com/production-error\";const attr=\"data-base-ui-focusable\";fetch(docs);"),"Base UI"),"a documentation link is not the Base UI error formatter");
    check(!has(script("function message(){return new URL(\"https://base-ui.com/production-error\")}if(node.matches(\"[data-base-ui-focusable]\"))skip();"),"Base UI"),"application selectors do not pair with the formatter");
    check(!has(script("node.projection=new Projection(node.latestValues,parent);"),"Motion (Framer Motion)"),"projection without the portal attribute excluded");
    check(!has(script("const id=Math.floor(8999999999999*Math.random())+1e12;"),"web-vitals"),"a random identifier alone is not web-vitals");
    check(!has(script("const metric={rating:\"good\",delta:0,entries:[],id:makeId()};"),"web-vitals"),"a metric-shaped object without the identifier scheme excluded");
    check(!has(script("const event=\"statsig::gate_exposure\";log(\"_getStatsigGlobal\");"),"Statsig"),"an exposure name and a logged word are not the Statsig client");
    check(!has(script("const hosts=[\"featuregates.org\",\"statsigapi.net\"];const key=\"statsig.cached.evaluations\";"),"Statsig"),"Statsig hostnames and storage keys are not the client");
    check(!has(script("if(window.KPSDK)ready();const headers=[\"x-kpsdk-ct\",\"x-kpsdk-cd\"];"),"Kasada"),"Kasada header names and a presence check are not the integration");
    check(!has(script("document.addEventListener(\"kpsdk-load\",ready);"),"Kasada"),"one Kasada marker insufficient");
    check(!has(script("const selector=\"style[data-styled]\";const attr=\"data-styled-version\";"),"styled-components"),"selectors for styled-components attributes are not the library");
    check(!has(script("const attr=process.env.SC_ATTR||\"data-styled\",versionAttr=\"data-styled-version\",release=load();"),"styled-components"),"styled-components requires its adjacent version constant");
    check(!has(script("const tags=document.querySelectorAll(\"style[data-emotion]\");tag.setAttribute(\"data-emotion\",this.key);"),"Emotion"),"one Emotion marker and a selector insufficient");
    check(!has(script("const hosts=[\"api.sanity.io\",\"apicdn.sanity.io\",\"cdn.sanity.io\"];const image=\"https://cdn.sanity.io/images/x/y/z.png\";"),"Sanity"),"Sanity hostnames and image URLs are not the client");
    check(!has(script("if(window.__THREE__)console.log(\"three present\");const revision=\"199\";"),"three.js"),"reading the global is not three.js");
    check(!has(script("new CustomEvent(\"register\",{detail:{revision:\"199\"}});"),"three.js"),"one three.js marker insufficient");
    check(!has(script("posthog.capture(\"$pageview\");const cookie=\"__ph_opt_in_out_\";window.posthog.__loaded=!0;"),"PostHog"),"calling a PostHog API is not the library");
    check(!has(script("if(window.__APOLLO_CLIENT__)inspect(window.__APOLLO_CLIENT__);const state=window.__APOLLO_STATE__;"),"Apollo Client"),"reading Apollo globals is not the client");
    check(!has(script("const label=\"RudderLabs JavaScript SDK\";window.rudderanalytics.track(\"opened\");"),"RudderStack"),"a label and an API call are not the SDK");
    check(!has(script("const queue=\"_sift\";const script=\"https://cdn.sift.com/s.js\";"),"Sift"),"Sift names in strings are not the integration");
    check(!has(script("const pkg=\"@vercel/speed-insights\";const path=\"/_vercel/speed-insights/script.js\";"),"Vercel Speed Insights"),"package and path strings alone excluded");
    check(!has(script("if(window.OneTrustStub)wait();const cookie=\"OptanonConsent\";"),"OneTrust"),"reading the stub and a cookie name are not OneTrust");
    const auto unversioned_three=script("typeof __THREE_DEVTOOLS__<\"u\"&&__THREE_DEVTOOLS__.dispatchEvent(new CustomEvent(\"register\",{detail:{revision:REV}})),window.__THREE__?console.warn(\"WARNING: Multiple instances of Three.js being imported.\"):window.__THREE__=REV;");
    check(has(unversioned_three,"three.js"),"three.js with a revision constant");
    for(const auto& f:unversioned_three)if(f.name=="three.js")check(f.version.empty(),"an unresolved revision constant supplies no version");

    HttpObservation header;header.url="https://unseen.example/";header.status=200;header.headers={{"x-envoy-upstream-service-time","12"}};
    std::vector<Finding> headers;identify_http(header,headers);check(has(headers,"Envoy"),"Envoy upstream timing header");
    header.headers={{"server","envoy"}};headers.clear();identify_http(header,headers);check(has(headers,"Envoy"),"Envoy Server token");
    header.headers={{"server","envoy-like"},{"x-envoy-upstream-service-time","fast"},{"x-note","x-envoy-upstream-service-time: 12"}};header.body="Powered by envoy";
    headers.clear();identify_http(header,headers);check(!has(headers,"Envoy"),"lookalike token, non-numeric value and prose excluded");

    const std::pair<const char*,const char*> advertised[]={{"server","openresty"},{"server","Openresty"},{"server","openresty/1.25.3.1"},{"server","AkamaiGHost"},
        {"x-akamai-transformed","9 12345 0 pmb=mRUM,1"},{"x-akamai-transformed","0 - 0 -"},{"x-amz-id-2","Uuag1LuByRx9e6j5Onimru9pO4ZVKnJ2Qz7/C1NPcfTWAtRPfTaOFg=="}};
    const char* advertised_names[]={"OpenResty","OpenResty","OpenResty","Akamai","Akamai","Akamai","Amazon S3"};
    for(size_t i=0;i<std::size(advertised);++i) {
        header.headers={{advertised[i].first,advertised[i].second}};header.body.clear();headers.clear();identify_http(header,headers);
        check(has(headers,advertised_names[i]),std::string(advertised_names[i])+" advertised by "+advertised[i].first);
    }
    header.headers={{"server","openresty/1.25.3.1"}};headers.clear();identify_http(header,headers);
    for(const auto& f:headers)if(f.name=="OpenResty")check(f.version=="1.25.3.1","OpenResty version comes from its Server token");
    check(!has(headers,"nginx"),"an OpenResty token does not claim nginx");
    header.headers={{"server","not-openresty"},{"server","openrestyish"},{"server","Akamai-like"},{"x-akamai-transformed","yes"},{"x-amz-id-2","short"},
        {"x-amz-request-id","0A49CE4060975EAC"},{"x-note","x-akamai-transformed: 9 1 0"}};header.body="Served by openresty behind AkamaiGHost with x-amz-id-2";
    headers.clear();identify_http(header,headers);
    check(!has(headers,"OpenResty")&&!has(headers,"Akamai")&&!has(headers,"Amazon S3"),"lookalike tokens, malformed values, a lone request ID and prose excluded");

    HttpObservation page;page.url="https://unseen.example/";page.status=200;
    page.body="<style data-styled=\"active\" data-styled-version=\"6.99.2\">.a{color:red}</style><style data-emotion=\"css 14r34si 9x8y7z\">.css-14r34si{color:red}</style>"
              "<link rel=\"stylesheet\" href=\"/_astro/layout.Ab12Cd34.css\"><script type=\"module\" src=\"/_astro/Router.astro_astro_type_script_index_0_lang.Ab12Cd34.js\"></script>"
              "<script src=\"https://ajax.googleapis.com/ajax/libs/webfont/1.6.99/webfont.js\"></script>";
    std::vector<Finding> markup;absorb_page_markup(page,markup);identify_http(page,markup);
    check(has(markup,"styled-components")&&has(markup,"Emotion")&&has(markup,"Astro")&&has(markup,"Web Font Loader"),"style elements and asset paths identify their producers");
    for(const auto& f:markup) {
        if(f.name=="styled-components")check(f.version=="6.99.2","styled-components version comes from its style element");
        if(f.name=="Web Font Loader")check(f.version=="1.6.99","Web Font Loader version comes from the hosted path");
        if(f.name=="Emotion")check(f.version.empty(),"Emotion style element supplies no version");
    }
    page.body="<p>We use data-styled-version=\"6.99.2\" and data-emotion and /_astro/ assets.</p><div data-styled=\"\" data-styled-version=\"6.99.2\" data-emotion=\"css abc\"></div>"
              "<style data-styled-version=\"latest\" data-styled=\"\"></style><style data-emotion=\"CSS Rules!\"></style><style data-emotion=\"css\"></style>"
              "<a href=\"/_astro/readme.js\">file</a><script src=\"/static/_astro_copy/app.js\"></script><script src=\"/_astro/\"></script><script src=\"/libs/webfont/webfont.js\"></script>";
    markup.clear();absorb_page_markup(page,markup);identify_http(page,markup);
    check(!has(markup,"styled-components")&&!has(markup,"Emotion")&&!has(markup,"Astro")&&!has(markup,"Web Font Loader"),"prose, other elements, malformed values and lookalike paths excluded");
    page.body="<style data-emotion=\"css\" data-s=\"\"></style>";markup.clear();absorb_page_markup(page,markup);check(has(markup,"Emotion"),"client-inserted Emotion style element");

    const std::string katex="@font-face{font-family:KaTeX_Main;font-style:normal;font-weight:400;src:url(a.woff2) format(\"woff2\")}.katex{font:normal 1.21em KaTeX_Main,Times New Roman,serif}.katex .katex-version:after{content:\"0.99.3\"}";
    std::vector<Finding> css;identify_stylesheet("https://unseen.example/opaque",katex,css);check(has(css,"KaTeX"),"KaTeX font face and version rule");
    for(const auto& f:css)if(f.name=="KaTeX")check(f.version=="0.99.3"&&f.evidence.size()==2,"KaTeX version comes from its own stylesheet rule");
    css.clear();identify_stylesheet("https://unseen.example/opaque","/*"+katex+"*/",css);check(!has(css,"KaTeX"),"commented KaTeX rules excluded");
    css.clear();identify_stylesheet("https://unseen.example/opaque",".katex{color:red}.math{font-family:KaTeX_Main}",css);check(!has(css,"KaTeX"),"KaTeX class and font names alone excluded");
    css.clear();identify_stylesheet("https://unseen.example/opaque",".katex .katex-version:after{content:\"0.99.3\"}",css);check(!has(css,"KaTeX"),"version rule without the font face excluded");

    const std::string splide=".splide__list{display:flex}.splide.is-initialized:not(.is-active) .splide__list{display:block}.splide__track--draggable{user-select:none}.splide__pagination{display:flex}";
    css.clear();identify_stylesheet("https://unseen.example/opaque",splide,css);check(has(css,"Splide"),"Splide state and track rules");
    for(const auto& f:css)if(f.name=="Splide")check(f.version.empty()&&f.evidence.size()==2,"Splide stylesheet supplies no version");
    css.clear();identify_stylesheet("https://unseen.example/opaque","/*"+splide+"*/",css);check(!has(css,"Splide"),"commented Splide rules excluded");
    css.clear();identify_stylesheet("https://unseen.example/opaque",".splide{color:red}.splide__list{display:flex}.note:after{content:\".splide.is-initialized:not(.is-active) .splide__list{ .splide__track--fade{\"}",css);
    check(!has(css,"Splide"),"Splide class names and quoted selectors alone excluded");
    css.clear();identify_stylesheet("https://unseen.example/opaque",".splide.is-initialized:not(.is-active) .splide__list{display:block}",css);check(!has(css,"Splide"),"one Splide rule insufficient");

    const std::pair<const char*,const char*> hosts[]={
        {"https://cdn.sanity.io/images/project/dataset/abc-10x10.png","Sanity"},
        {"https://cdn.sanity.io/files/project/dataset/abc.riv","Sanity"},
        {"https://ajax.googleapis.com/ajax/libs/webfont/1.6.26/webfont.js","Google Hosted Libraries"},
        {"https://geolocation.onetrust.com/cookieconsentpub/v1/geo/location","OneTrust"},
        {"https://js.hcaptcha.com/1/api.js","hCaptcha"},
        {"https://websdk.appsflyersdk.com/manifestLoader.v1.js","AppsFlyer"},
        {"https://websdk.appsflyersdk.com/","AppsFlyer"}};
    for(const auto& [url,name]:hosts) {
        auto r=request(std::string(url)+"?key=PRIVATE");check(vendor(r,name),std::string(name)+" documented resource attributed");
        check(report_json(r).find("PRIVATE")==std::string::npos,"request query value omitted");
        check(!vendor(request(url,0),name),"failed request never becomes observed provider");
        const auto slash=std::string(url).find('/',8);auto misleading=std::string(url);misleading.insert(slash,".unseen.example");
        check(!vendor(request(misleading),name),std::string(name)+" lookalike suffix rejected");
        misleading=std::string(url);misleading.insert(8,"unseen.");check(!vendor(request(misleading),name),std::string(name)+" unrelated subdomain rejected");
        check(!vendor(request(std::string(url).substr(0,slash)+"/unrelated"),name),std::string(name)+" unrelated path rejected");
    }
    for(const auto* url:{"https://bucket.s3.us-east-2.amazonaws.com/public/app.js","https://bucket.s3.amazonaws.com/app.js","https://s3.eu-west-1.amazonaws.com/bucket/app.js",
                         "https://s3.amazonaws.com/bucket/app.js","https://my.bucket.s3.dualstack.ap-southeast-1.amazonaws.com/app.js","https://bucket.s3-us-west-2.amazonaws.com/app.js",
                         "https://bucket.s3-accelerate.amazonaws.com/app.js"})
        check(vendor(request(url),"Amazon S3"),std::string("Amazon S3 endpoint form: ")+url);
    for(const auto* url:{"https://s3.amazonaws.com.unseen.example/app.js","https://bucket.s3.us-east-2.amazonaws.com.unseen.example/app.js","https://execute-api.us-east-2.amazonaws.com/app.js",
                         "https://s3bucket.us-east-2.amazonaws.com/app.js","https://bucket.s3.notaregion.amazonaws.com/app.js","https://s3.example.com/app.js",
                         "https://d111.cloudfront.net/s3/app.js","https://lambda.s3x.amazonaws.com/app.js"})
        check(!vendor(request(url),"Amazon S3"),std::string("not an Amazon S3 endpoint: ")+url);
    check(!vendor(request("https://bucket.s3.us-east-2.amazonaws.com/app.js",0),"Amazon S3"),"no response is not observed Amazon S3 contact");
}
