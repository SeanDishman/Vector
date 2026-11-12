
void expanded_checks() {
    HttpObservation header;header.url="https://unseen.example/";header.status=200;header.headers={{"server","ATS/9.2.16"}};
    std::vector<Finding> headers;identify_http(header,headers);check(has(headers,"Apache Traffic Server"),"ATS advertised Server token");
    for(const auto& f:headers)if(f.name=="Apache Traffic Server")check(f.version=="9.2.16","ATS exact advertised version");
    header.headers={{"server","cats/9.2.16"}};header.body="The server was ATS/9.2.16";headers.clear();identify_http(header,headers);
    check(!has(headers,"Apache Traffic Server"),"prose and unrelated Server token excluded");
    const std::string bulma="/*! bulma.io v1.0.4 | MIT License | github.com/jgthms/bulma */:root{--bulma-control-radius:1px;}";
    std::vector<Finding> css;identify_stylesheet("https://unseen.example/opaque",bulma,css);check(has(css,"Bulma"),"Bulma complete license banner");
    css.clear();identify_stylesheet("https://unseen.example/opaque",".example{content:'"+bulma+"'}",css);check(!has(css,"Bulma"),"CSS string banner excluded");
    css.clear();identify_stylesheet("https://unseen.example/opaque","/*! bulma.io v1.0.4 */",css);check(!has(css,"Bulma"),"Bulma product text without provenance excluded");
    const std::pair<const char*,const char*> libraries[]={
        {"Vue.js","function node(v){return v.__v_isVNode===!0} el.__vue_app__=application;"},
        {"Vue I18n","const key=Symbol('global-vue-i18n');app.__VUE_I18N_SYMBOL__=key;"},
        {"Lucide","const component=({iconNode:n,absoluteStrokeWidth:a})=>({class:merge('lucide',n)});"},
        {"Pako","global.pako=factory();lib.inflateRaw=function(input,options){return input},lib.ungzip=inflate;"},
        {"rlottie","var wasmBinaryFile='rlottie-wasm.wasm';Module['_lottie_load_from_data']=function(){};"},
        {"dotLottie Web Component","this._createDotLottieInstance({});export{Component as DotLottieWC};"},
        {"Sentry","sdk.name||(sdk.name=`sentry.javascript.${type}`,sdk.packages=names.map(name=>({name:`${source}:@sentry/${name}`,version:release})));"},
        {"Google Tag Manager","data.push({'gtm.start':Date.now(),event:'gtm.js'});url='//www.googletagmanager.com/gtm.js?id=PRIVATE';"},
        {"DOMPurify","const policy='dompurify';p.version='3.4.0',p.removed=[];p.sanitize=function(){};p.removeAllHooks=function(){};"},
        {"Headless UI","attrs[`data-headlessui-state`]=states.join(` `);document.documentElement.dataset.headlessuiFocusVisible=``;"},
        {"Floating UI","platform.getElementRects({reference:ref,floating:node,strategy:mode});const result=fn({middlewareData:data,rects:rect,platform:engine,elements:{reference:ref,floating:node}});"},
        {"ttag","this.conf.setDedent(!!value);this.conf.setCurrentLocale(locale);const copy=this.copyWithNewContext(context);"},
        {"Turbopack","window.next={version:build.version,turbopack:!0};self.__turbopack_load_page_chunks__=function(page,chunks){};"},
        {"Prebid","engine.libLoaded=!0;engine.version='v10.23.0';engine.adUnits=engine.adUnits||[];register('getHighestCpmBids',function(code){return bids.targeting.getWinningBids(code)});"},
        {"Motion (Framer Motion)","const appearAttribute='data-'+dash('framerAppearId');function project(node,props){node.projection=new Projection(node.latestValues,props['data-framer-portal-id']?void 0:closest(node.parent));}"},
        {"Base UI","const inertMarker='data-base-ui-inert';const guard=create('span',{...rest,'data-base-ui-focus-guard':''});"},
        {"Datadog Browser SDK","const intake=['ddsource','dd-api-key','dd-request-id'];function api(stub){return{version:'7.99.1',onReady(callback){callback()},...stub}}"},
        {"TanStack Query","function client(c){if(!c)throw new Error('No QueryClient set, use QueryClientProvider to set one');return c}const fn=o=>()=>Promise.reject(new Error(`Missing queryFn: '${o.queryHash}'`));"},
        {"Intercom Messenger","function boot(id){var w=window,ic=w.Intercom;if(typeof ic==='function'){ic('reattach_activator');ic('update',w.intercomSettings)}else{var s=document.createElement('script');s.async=!0;s.src='https://widget.intercom.io/widget/'+id;document.head.appendChild(s)}}"},
        {"Highlight.js","var release='11.99.2';const options={classPrefix:'hljs-',cssSelector:'pre code'};Object.assign(api,{highlightAll:run});api.versionString=release;"},
        {"TanStack Router","function entry(win,id){win.history.replaceState({key:id,__TSR_key:id},'')}const saved=JSON.parse(store.getItem('tsr-scroll-restoration-v1_3')||'{}');"}
    };
    const auto tied_version=[](const std::string& name) {
        return name=="DOMPurify"?"3.4.0":name=="Prebid"?"10.23.0":name=="Datadog Browser SDK"?"7.99.1":name=="Highlight.js"?"11.99.2":"";
    };
    for(const auto& [name,body]:libraries) {
        const auto found=script(body);check(has(found,name),std::string(name)+" paired code markers");
        for(const auto& f:found)if(f.name==name) {
            check(f.evidence.size()>=2,std::string(name)+" retains both matched values");
            check(f.version==tied_version(name),"only tied product metadata supplies a version");
            for(const auto& e:f.evidence)check(e.source=="https://unseen.example/opaque","no target or filename dependence");
        }
        check(!has(script("/*"+std::string(body)+"*/"),name),"commented example excluded");
        std::string quoted="const documentation=\"";
        for(const char ch:std::string(body)){if(ch=='\\'||ch=='\"')quoted+='\\';quoted+=ch;}
        quoted+="\";";
        check(!has(script(quoted),name),"quoted code excluded");
        check(!has(script(std::string(2*1024*1024,' ')+body),name),"markers beyond cap excluded");
    }
    check(!has(script("const node={__v_isVNode:true};"),"Vue.js"),"one Vue marker insufficient");
    check(!has(script("const cls='lucide';const text='iconNode:absoluteStrokeWidth';"),"Lucide"),"icon mentions insufficient");
    check(!has(script("global.pako=factory();lib.inflateRaw=function(){};"),"Pako"),"Pako needs paired export methods");
    check(!has(script("var wasmBinaryFile='rlottie-wasm.wasm';"),"rlottie"),"filename alone insufficient");
    check(!has(script("let url='https://www.googletagmanager.com/gtm.js?id=PRIVATE';"),"Google Tag Manager"),"GTM URL alone insufficient");
    check(!has(script("sdk.name='sentry.javascript.browser';"),"Sentry"),"Sentry label alone insufficient");
    check(!has(script("const policy='dompurify';a.version='3.4.0',a.removed=[];b.sanitize=function(){};b.removeAllHooks=function(){};"),"DOMPurify"),"DOMPurify APIs belong to same object");
    check(!has(script("attrs['data-headlessui-state']=states.join(' ');"),"Headless UI"),"one Headless UI marker insufficient");
    check(!has(script("platform.getElementRects({reference:ref,floating:node,strategy:mode});"),"Floating UI"),"one Floating UI API insufficient");
    check(!has(script("this.conf.setDedent(!!value);this.conf.setCurrentLocale(locale);"),"ttag"),"ttag also requires context API");
    check(!has(script("a.conf.setDedent(!!value);b.conf.setCurrentLocale(locale);c.copyWithNewContext(context);"),"ttag"),"ttag API calls belong to same object");
    check(!has(script("window.next={version:build.version,turbopack:!0};"),"Turbopack"),"Turbopack flag requires paired page loader");
    check(has(script("window.next = {version: build.version, turbopack : true};self.__turbopack_load_page_chunks__ = function(page,chunks){};"),"Turbopack"),"Turbopack flag whitespace accepted");
    check(!has(script("window.next={version:build.version,turbopack:false};self.__turbopack_load_page_chunks__=function(page,chunks){};"),"Turbopack"),"false Turbopack flag excluded");
    check(!has(script("engine.libLoaded=!0;engine.version='v10.23.0';engine.adUnits=engine.adUnits||[];"),"Prebid"),"Prebid initialization requires paired implementation API");
    check(!has(script("engine.libLoaded=!0;other.version='v10.23.0';engine.adUnits=engine.adUnits||[];register('getHighestCpmBids',function(code){return bids.targeting.getWinningBids(code)});"),"Prebid"),"Prebid version belongs to initialized object");
    check(!has(script("engine.libLoaded=!0;engine.version='v10.23.0';engine.adUnits=engine.adUnits||[];pbjs.getHighestCpmBids();"),"Prebid"),"an API call alone is not an implementation");
    const auto other_prebid=script("r.libLoaded = true; r.version = '17.8.9'; r.adUnits = r.adUnits || [];expose('getHighestCpmBids', function(code) { return auctions.targeting.getWinningBids(code) });");
    check(has(other_prebid,"Prebid"),"Prebid renamed variables and different build version");
    for(const auto& f:other_prebid)if(f.name=="Prebid")check(f.version=="17.8.9","Prebid version is extracted, not catalogued");
    check(!has(script("const allowed=new Set(['react','framer-motion','lucide-react']);const hint='data-framer-portal-id framerAppearId';"),"Motion (Framer Motion)"),"package allow-list and prose do not bundle Motion");
    check(!has(script("const appearAttribute='data-'+dash('framerAppearId');"),"Motion (Framer Motion)"),"one Motion marker insufficient");
    check(!has(script("const guards='[data-base-ui-focus-guard], [data-toast-guard]';if(node.hasAttribute('data-base-ui-focus-guard')||node.matches('[data-base-ui-inert]'))skip();"),"Base UI"),"application selectors for Base UI attributes are not the library");
    check(!has(script("const inertMarker='data-base-ui-inert';"),"Base UI"),"one Base UI marker insufficient");
    check(!has(script("const site=config.site||'datadoghq.com';const hosts=['browser-intake-datadoghq.com'];const meta={version:'7.99.1',onReady(callback){callback()}};"),"Datadog Browser SDK"),"Datadog hostnames and a versioned object are not the SDK");
    check(!has(script("const intake=['ddsource','dd-api-key','dd-request-id'];const meta={version:'7.99.1',ready:true};"),"Datadog Browser SDK"),"intake parameters need the versioned public API");
    check(!has(script("console.warn('No QueryClient set, use QueryClientProvider to set one');const key='tanstack-query-cache';"),"TanStack Query"),"a logged message and a cache key are not TanStack Query");
    check(!has(script("const widget='https://widget.intercom.io/widget/';const allowed=['*.intercom.io','*.intercomcdn.com'];track('reattach_activator');"),"Intercom Messenger"),"Intercom hostnames in an allow-list are not the loader");
    check(!has(script("const cls='hljs-keyword';const options={classPrefix:'token-'};api.versionString='11.99.2';"),"Highlight.js"),"another highlighter's prefix is not Highlight.js");
    check(!has(script("const note='__TSR_key: id';if(history.state&&history.state.__TSR_key)restore();const name='tsr-scroll-restoration-notes';"),"TanStack Router"),"reading the state key and a similar storage name are not the router");
    check(!has(script("function entry(win,id){win.history.replaceState({key:id,__TSR_key:id},'')}"),"TanStack Router"),"one TanStack Router marker insufficient");
    const auto inline_hljs=script("const options={classPrefix:\"hljs-\"};api.versionString=\"11.99.2\";");
    check(has(inline_hljs,"Highlight.js"),"Highlight.js literal version export");
    for(const auto& f:inline_hljs)if(f.name=="Highlight.js")check(f.version=="11.99.2","literal Highlight.js version retained");
    const auto far_hljs=script("var release='9.9.9';function other(){var release=load();return release}const options={classPrefix:'hljs-'};api.versionString=release;");
    check(has(far_hljs,"Highlight.js"),"Highlight.js without a resolvable version constant");
    for(const auto& f:far_hljs)if(f.name=="Highlight.js")check(f.version.empty(),"only the nearest declaration may supply the Highlight.js version");
    const auto react_dom=script("const internals={bundleType:0,version:'19.99.3',rendererPackageName:'react-dom',currentDispatcherRef:d,reconcilerVersion:'19.99.3'};");
    check(has(react_dom,"React"),"React DOM renderer registration");
    for(const auto& f:react_dom)if(f.name=="React")check(f.version=="19.99.3"&&f.confidence=="High","renderer registration supplies the React version");
    check(!has(script("const internals={version:'4.5.6',rendererPackageName:'react-native-renderer'};"),"React"),"another renderer's version is not React DOM's");
    check(!has(script("const doc=\"version:'19.99.3',rendererPackageName:'react-dom'\";"),"React"),"quoted renderer registration excluded");
    const std::pair<const char*,const char*> responses[]={
        {"https://ib.adnxs.com/getuid","AppNexus"},
        {"https://cm.g.doubleclick.net/pixel","Google cookie matching"},
        {"https://gum.criteo.com/sid/json","Criteo"},
        {"https://match.adsrvr.org/track/rid","The Trade Desk"},
        {"https://ssum-sec.casalemedia.com/usermatch","Index Exchange"},
        {"https://u.openx.net/w/1.0/cm","OpenX"},
        {"https://ce.lijit.com/beacon/prebid-server/","Sovrn"},
        {"https://s.ad.smaato.net/i/","Smaato"},
        {"https://s.seedtag.com/cs/cookiesync/prebid","Seedtag"},
        {"https://onetag-sys.com/usync/","OneTag"},
        {"https://pbs-cs.minutemedia-prebid.com/pbs-iframe","Minute Media"},
        {"https://rtb.gumgum.com/usync/98765","GumGum"},
        {"https://eus.rubiconproject.com/usync.html","Rubicon Project"},
        {"https://prebid.a-mo.net/isyn","AMX"},
        {"https://cs.startappnetwork.com/sync","Start.io"},
        {"https://beacon.bidder.taboola.com/timeout","Taboola"},
        {"https://ads.pubmatic.com/AdServer/js/user_sync.html","PubMatic"},
        {"https://eb2.3lift.com/sync","TripleLift"},
        {"https://eb2.3lift.com/getuid","TripleLift"},
        {"https://sync.go.sonobi.com/us","Sonobi"},
        {"https://sync.go.sonobi.com/us.gif","Sonobi"},
        {"https://sync.go.sonobi.com/uc.html","Sonobi"},
        {"https://ups.analytics.yahoo.com/ups/98765/cms","Yahoo cookie matching"},
        {"https://match.sharethrough.com/FGMrCMMc/v1","Sharethrough"},
        {"https://jadserve.postrelease.com/suid/98765","Nativo"},
        {"https://cs.media.net/cksync.php","Media.net"},
        {"https://c1.adform.net/cookie","Adform"},
        {"https://cdn.confiant-integrations.net/fresh-property/gpt_and_prebid/config.js","Confiant"},
        {"https://pixel.adsafeprotected.com/services/pub","Integral Ad Science"},
        {"https://client.aps.amazon-adsystem.com/publisher.js","Amazon Publisher Services"},
        {"https://exbd.ex.co/sdk.js","EX.CO"},
        {"https://phx.bmtrcs.com/api/v2/fresh-site/lib/site.js","Brand Metrics"},
        {"https://secure.cdn.fastclick.net/js/pubcid/fresh-version/pubcid.min.js","SharedID"},
        {"https://donorbox.org/widget.js","Donorbox"},
        {"https://match.adsrvr.org/track/cmf/generic","The Trade Desk"},
        {"https://ssum.casalemedia.com/usermatchredir","Index Exchange"},
        {"https://rtb.openx.net/sync/prebid","OpenX"},
        {"https://s.ad.smaato.net/c/","Smaato"},
        {"https://eus.rubiconproject.com/usync.js","Rubicon Project"},
        {"https://prebid.a-mo.net/cchain/0","AMX"},
        {"https://beacon.bidder.taboola.com/bidError","Taboola"},
        {"https://trc.taboola.com/sg/ps/1/cm","Taboola"},
        {"https://image8.pubmatic.com/AdServer/ImgSync","PubMatic"},
        {"https://hbx.media.net/checksync.php","Media.net"},
        {"https://cdn.confiant-integrations.net/gptprebidnativevideo/fresh/wrap.js","Confiant"},
        {"https://static.adsafeprotected.com/publisher/fresh/sdk.js","Integral Ad Science"},
        {"https://config.aps.amazon-adsystem.com/configs/fresh","Amazon Publisher Services"},
        {"https://web-banner.ads.aps.amazon-adsystem.com/e/dtb/bid","Amazon Publisher Services"},
        {"https://c.amazon-adsystem.com/bao-csm/aps-comm/aps_csm.js","Amazon Publisher Services"},
        {"https://collector.brandmetrics.com/states/json","Brand Metrics"},
        {"https://a.teads.tv/hb/bid-request","Teads"},
        {"https://display.bidder.taboola.com/OpenRTB/TaboolaHB/auction","Taboola"},
        {"https://native.bidder.taboola.com/OpenRTB/TaboolaHB/auction","Taboola"},
        {"https://98765.fls.doubleclick.net/activityi;src=98765;type=example;cat=test;ord=1","DoubleClick Floodlight"},
        {"https://ssbsync-global.smartadserver.com/api/sync","Smart Ad Server"}
    };
    for(const auto& [url,name]:responses) {
        auto r=request(std::string(url)+"?identifier=PRIVATE");check(vendor(r,name),std::string(name)+" response attributed");
        check(report_json(r).find("PRIVATE")==std::string::npos,"query values remain private");
        check(!vendor(request(url,0),name),"no response is not observed contact");
        const auto redirect=request(url,302);
        check(vendor(redirect,name),"redirect establishes attributed contact");
        for(const auto& v:redirect.vendors)check(v.capability.find("received this script")==std::string::npos,"redirect does not establish a received script body");
        auto wrong=std::string(url);const auto slash=wrong.find('/',8);wrong.insert(slash,".unrelated.example");
        check(!vendor(request(wrong),name),"suffix lookalike excluded");
        wrong=std::string(url);const auto hostEnd=wrong.find('/',8),lastDot=wrong.rfind('.',hostEnd),domainDot=wrong.rfind('.',lastDot-1);
        wrong.insert(domainDot==std::string::npos?8:domainDot+1,"not-");
        check(!vendor(request(wrong),name),"registrable-domain lookalike excluded");
        wrong=std::string(url).substr(0,slash)+"/unrelated-endpoint";
        check(!vendor(request(wrong),name),std::string(name)+" unrelated path excluded");
        check(report_json(r).find("\"account_access_verified\":false")!=std::string::npos||report_json(r).find("\"account_access_verified\": false")!=std::string::npos,"contact does not verify account access");
    }
    HttpObservation page;page.url="https://unseen.example/";page.status=200;
    page.body="<a href='https://ib.adnxs.com/getuid'>Vendor docs</a><script>const example='https://gum.criteo.com/sid/json';</script>";
    ScanReport report;report.target=parse_target(page.url);identify_vendors(page,report);
    check(infrastructure_entries(report).empty(),"links and code strings do not imply integrations");
}
