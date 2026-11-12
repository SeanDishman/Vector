
void bybit_checks() {
    struct Library {const char* name;const char* body;const char* version;};
    const Library libraries[]={
        {"i18next","class Logger{init(e,t={}){this.prefix=t.prefix||\"i18next:\",this.logger=e||fallback,this.options=t}}class I18n{change(l){this.translator.changeLanguage(l),this.emit(\"languageChanged\",l)}}",""},
        {"Day.js","var Dayjs=function(){function d(t){this.$L=parse(t.locale,null,!0),this.parse(t)}var p=d.prototype;p.format=function(t){var n=t||\"YYYY-MM-DDTHH:mm:ssZ\";return n};return d}();",""},
        {"Day.js","var FORMAT_DEFAULT = 'YYYY-MM-DDTHH:mm:ssZ';class Dayjs{constructor(cfg){this.$L = parseLocale(cfg.locale, null, true);this.parse(cfg)}}",""},
        {"Sensors Analytics","var channels=\"utm_source utm_medium\",release=\"1.99.2\",probe=\"sensorsdata_domain_test\";function boot(t){void 0===window.sensorsDataAnalytic201505?window.sensorsDataAnalytic201505=t:t.init()}","1.99.2"},
        {"Sensors Analytics","function probe(d){var key = 'sensorsdata_domain_test';return d[key]}function boot(sd){window.sensorsDataAnalytic201505 = sd;sd.init()}",""}
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
    check(!has(script("const prefix=options.prefix||\"i18next:\";"),"i18next"),"i18next logger prefix alone is insufficient");
    check(!has(script("bus.emit(\"languageChanged\",lang);const label=\"i18next:\";"),"i18next"),"i18next event with an unrelated literal is insufficient");
    check(!has(script("hooks.defaultFormat=\"YYYY-MM-DDTHH:mm:ssZ\";function Moment(c){this._locale=c.locale}"),"Day.js"),"another date library's default format is not Day.js");
    check(!has(script("function d(t){this.$L=parse(t.locale,null,!0)}"),"Day.js"),"locale assignment alone is insufficient");
    check(!has(script("var probe=\"sensorsdata_domain_test\";"),"Sensors Analytics"),"cookie probe name alone is insufficient");
    check(!has(script("window[\"sensorsDataAnalytic201505\"]=\"sensors\";var probe=\"sensorsdata_domain_test\";"),"Sensors Analytics"),"loader name string is not the SDK global assignment");
    const auto next=script("Object.defineProperty(t,\"appBootstrap\",{enumerable:!0,get:function(){return n}}),window.next={version:\"14.99.3\",appDir:!0};");
    check(next.size()==1&&next[0].name=="Next.js"&&next[0].category=="Framework"&&next[0].version=="14.99.3"&&next[0].confidence=="High","Next.js states its release in its own global");
    const auto canary=script("window.next = { version: \"15.99.0-canary.12\", appDir: true };");
    check(has(canary,"Next.js")&&canary[0].version=="15.99.0-canary.12","Next.js prerelease kept exactly as stated");
    check(script("window.next={version:release,appDir:!0};").empty(),"Next.js version variable is not a stated release");
    check(script("window.nextStep={version:\"14.99.3\",appDir:!0};").empty(),"lookalike global is not Next.js");
    check(script("const note=\"window.next={version:\\\"14.99.3\\\",appDir:!0}\";").empty(),"quoted Next.js bootstrap excluded");
    check(script("/*window.next={version:\"14.99.3\",appDir:!0}*/").empty(),"commented Next.js bootstrap excluded");
    check(script("window.next={version:\"14.99.3\",router:r};").empty(),"Next.js release needs the paired appDir field");
    const auto react=script("var internals={findFiberByHostInstance:f,bundleType:0,version:\"18.99.0-canary-0a1b2c3d4e-20990101\", rendererPackageName:\"react-dom\"};");
    check(react.size()==1&&react[0].name=="React"&&react[0].category=="Library"&&react[0].version=="18.99.0-canary-0a1b2c3d4e-20990101","React renderer prerelease kept exactly as stated");
    check(script("var internals={version:\"18.99.0-canary-0a1b2c3d4e-20990101\",rendererPackageName:\"react-native-renderer\"};").empty(),"another renderer is not react-dom");
    check(script("const note=\"version:\\\"18.99.0-canary-1\\\",rendererPackageName:\\\"react-dom\\\"\";").empty(),"quoted renderer registration excluded");
    const auto stable=script("var internals={bundleType:0,version:\"18.99.1\",rendererPackageName:\"react-dom\"};");
    check(stable.size()==1&&stable[0].version=="18.99.1","stable React release still reported once");
}

void ten_site_checks() {
    struct Library {const char* name;const char* body;const char* version;};
    const Library paired[]={
        {"React","var r=n((e=>{var t=Symbol.for(`react.transitional.element`),n=Symbol.for(`react.portal`),r=Symbol.for(`react.fragment`);e.x=t}));",""},
        {"MobX","function die(e,...t){throw Error(`[MobX] minified error nr: ${e}${t.length?` `+t.map(String).join(`,`):``}. See the documentation`)}var y=Symbol(`mobx administration`);",""},
        {"MobX","function die(e){throw new Error(\"[MobX] minified error nr: \"+e)}var $mobx=Symbol(\"mobx administration\");",""},
        {"RxJS","var E=make(function(e){return function(t){e(this),this.message=t?t.length+` errors`:``,this.name=`UnsubscriptionError`,this.errors=t}});var S=function(){function e(e){this.initialTeardown=e,this.closed=!1,this._parentage=null,this._finalizers=null}return e}();",""},
        {"RxJS","function UnsubscriptionErrorImpl(errors){Error.call(this);this.name=\"UnsubscriptionError\";this.errors=errors;return this}function Subscription(u){this.closed=false;this._parentOrParents=null;this._subscriptions=null;if(u){this._unsubscribe=u}}",""},
        {"Radix UI","var Slot=make(\"Slot\"),mark=Symbol(\"radix.slottable\");function isSlottable(e){return h.isValidElement(e)&&\"function\"==typeof e.type&&\"__radixId\"in e.type&&e.type.__radixId===mark}",""},
        {"Radix UI","var mark=Symbol.for('radix.slottable');function guard(){let e=document.createElement(\"span\");return e.setAttribute(\"data-radix-focus-guard\",\"\"),e.tabIndex=0,e}",""},
        {"Cloudflare Zaraz","!function(bb,bc,bd){if(bb.zaraz)console.error(\"zaraz is loaded twice\");else{bb[bd]=bb[bd]||{};bb.zaraz={deferred:[],listeners:[]};bb.zaraz._v=\"99\";bb.zaraz.q=[]}}(window,document,\"zarazData\");",""},
        {"Twilio Segment","var p=\"1.99.2\";function l(){return\"npm\"}var f=\"api.segment.io/v1\";var u=function(){var e=o();return e||\"https://cdn.segment.com\"};","1.99.2"},
        {"Twilio Segment","const host = 'api.segment.io/v1';const cdn = () => override() || 'https://cdn.segment.com';",""},
        {"Sanity","var Ar=`apicdn.sanity.io`,U={apiHost:`https://api.sanity.io`,apiVersion:`1`,useProjectHostname:!0};",""},
        {"PostHog","class C{constructor(){this.__request_queue=[],this.__loaded=!1}get name(){var{token:e,opt_out_capturing_cookie_prefix:t}=this.config;return(t||`__ph_opt_in_out_`)+e}}",""},
        {"Apollo Client","C.prototype.connectToDevTools=function(){if(!(typeof window>\"u\")){var e=window,t=Symbol.for(\"apollo.devtools\");(e[t]=e[t]||[]).push(this),e.__APOLLO_CLIENT__=this}};",""},
        {"web-vitals","var make=(e,t)=>({name:e,value:t,rating:`good`,delta:0,entries:[],id:`v5-${Date.now()}-${Math.floor(8999999999999*Math.random())+0xe8d4a51000}`,navigationType:r});",""},
        {"web-vitals","var G_=()=>`v5-${Date.now()}-${Math.floor(Math.random()*(9e12-1))+0xe8d4a51000}`,K_=(e,t=-1)=>{let r=`navigate`;return{name:e,value:t,rating:`good`,delta:0,entries:[],id:G_(),navigationType:r}};",""},
        {"web-vitals","var id=function(){return\"v4-\".concat(Date.now(),\"-\").concat(Math.floor(Math.random()*(9e12-1))+1e12)},init=function(e,t){return{name:e,value:t,rating:\"good\",delta:0,entries:[],id:id(),navigationType:n}};",""}
    };
    for(const auto& [name,body,version]:paired) {
        const auto found=script(body);check(has(found,name),std::string(name)+" paired markers in a real build form");
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
    const Library single[]={
        {"Radix UI","var P=n.forwardRef((e,n)=>{let{asChild:r,...o}=e;return\"u\">typeof window&&(window[Symbol.for(\"radix-ui\")]=!0),(0,i.jsx)(r?d:t,{...o,ref:n})});",""},
        {"Radix UI","var P=(0,y.forwardRef)((e,n)=>{(0,y.useEffect)(()=>{window[Symbol.for(`radix-ui`)]=true},[]);return make(e,n)});",""},
        {"Base UI","function message(t,n,i){return`${t} error #${n}; visit ${i} for the full message.`}var k=O(`https://base-ui.com/production-error`,`Base UI`);",""},
        {"Loadable Components","\"use strict\";(self.__LOADABLE_LOADED_CHUNKS__=self.__LOADABLE_LOADED_CHUNKS__||[]).push([[\"20466\"],{18341:function(e,t,n){n.d(t,{a:()=>i})}}]);",""},
        {"Rive","e=>{e.exports=JSON.parse('{\"name\":\"@rive-app/canvas\",\"version\":\"2.99.1\",\"description\":\"Canvas based web api.\",\"main\":\"rive.js\"}')}","2.99.1"},
        {"YouTube embed","function Player({id:C,open:p}){return p&&(0,e.jsx)(\"iframe\",{src:`https://www.youtube.com/embed/${C}?autoplay=1&rel=0`,allow:\"autoplay; encrypted-media\"})}",""},
        {"YouTube embed","function Player(p){return React.createElement('iframe',{title:p.title,src:'https://www.youtube-nocookie.com/embed/'+p.id,allowFullScreen:true})}",""},
        {"React","var qh={bundleType:0,version:`19.99.1`,rendererPackageName:`react-dom`,currentDispatcherRef:E};","19.99.1"}
    };
    for(const auto& [name,body,version]:single) {
        const auto found=script(body);check(has(found,name),std::string(name)+" library's own registration or metadata");
        for(const auto& f:found)if(f.name==name)check(f.version==version&&!f.evidence.empty(),std::string(name)+" keeps its matched value and stated version");
        check(!has(script("/*"+std::string(body)+"*/"),name),std::string(name)+" commented example excluded");
        std::string quoted="const documentation=\"";
        for(const char ch:std::string(body)){if(ch=='\\'||ch=='\"')quoted+='\\';quoted+=ch;}
        quoted+="\";";
        check(!has(script(quoted),name),std::string(name)+" quoted code excluded");
    }
    const auto lodash=script("var n=200,r=`__lodash_hash_undefined__`,i=800;function Hash(e){this.clear()}function clear(){this.__data__=nativeCreate?nativeCreate(null):{},this.size=0}");
    check(has(lodash,"lodash")&&lodash[0].evidence.size()>=2,"lodash internals: its sentinel constant with its cache store");
    check(!has(script("var r=`__lodash_hash_undefined__`;"),"lodash"),"the sentinel alone is insufficient");
    check(!has(script("function Store(){this.__data__={}}console.log(\"__lodash_hash_undefined__\");"),"lodash"),"a store field with the sentinel only mentioned in a call is insufficient");
    const auto lucide=script("var ArrowUpDown=e=>(0,i.jsx)(`svg`,{viewBox:`0 0 24 24`,fill:`none`,stroke:`currentColor`,strokeWidth:2,strokeLinecap:`round`,strokeLinejoin:`round`,className:`lucide lucide-arrow-up-down`,...e});");
    check(has(lucide,"Lucide"),"a compiled Lucide icon keeps its stroke attributes and class pair");
    check(!has(script("var cls=\"lucide lucide-arrow-up-down\";var o={strokeLinejoin:\"round\"};"),"Lucide"),"a class string apart from the icon element is insufficient");
    check(!has(script("var icon={strokeLinejoin:`round`,className:`icon icon-arrow`};"),"Lucide"),"another icon set's class is not Lucide");
    check(!has(script("function id(e,t){return e||(t?`radix-${t}`:\"\")}var selector=`[role=\"menu\"], [data-radix-popper-content-wrapper]`;"),"Radix UI"),"an id prefix and a selector string are not Radix UI code");
    check(!has(script("var mark=Symbol(\"radix.slottable\");"),"Radix UI"),"one unpaired symbol is insufficient");
    check(!has(script("Object.setPrototypeOf(this,this[S[0]][S[1]]),this.name=\"ObjectUnsubscribedError\";var q={closed:!1};"),"RxJS"),"an error name inside another vendor's bundle is not RxJS");
    check(!has(script("this.name=`UnsubscriptionError`,this.errors=t;"),"RxJS"),"one unpaired RxJS marker is insufficient");
    check(!has(script("function die(e){throw Error(`[MobX] minified error nr: ${e}`)}"),"MobX"),"one unpaired MobX marker is insufficient");
    check(!has(script("var ignored=[\"https://events.example.test/bulk/\",\"https://api.segment.io/v1/b\"];t.src=`${n||`\"https://cdn.segment.com\"`}/analytics.js/v1/`;"),"Twilio Segment"),"an ignore list and a loader template are not the bundled SDK");
    check(!has(script("function heap({heap:e}){if(!e?.appId)return null;return load(e.appId)}function chat(){return{src:`https://chat-assets.frontapp.com/v1/chat.bundle.js`}}"),"Twilio Segment"),"optional integration code names no analytics SDK");
    check(!has(script("var shared=U[\"__core-js_shared__\"],masked=/[^.]+$/.exec(shared&&shared.keys&&shared.keys.IE_PROTO||\"\");"),"core-js"),"another library probing the shared store is not core-js");
    check(!has(script("var links=[{name:\"Youtube\",link:\"https://www.youtube.com/c/\".concat(r)}];var thumb=`https://i.ytimg.com/vi/${e}/hqdefault.jpg`;"),"YouTube embed"),"a channel link and a thumbnail template are not an embed");
    check(!has(script("var doc=help(\"iframe\",{note:1});var url=\"https://www.youtube.com/embed/\"+id;"),"YouTube embed"),"an embed URL outside the frame constructor is not an embed component");
    check(!has(script("var k=O(`https://base-ui.com/production-error`,`Other`);"),"Base UI"),"the error address without the library name is insufficient");
    check(!has(script("var o={url:\"https://example.test/production-error\",name:\"Base UI\"};"),"Base UI"),"a name in data is not the library helper");
    check(!has(script("(self.__LOADABLE_LOADED_CHUNKS__=window.__OTHER__||[]).push([]);var s=\"__LOADABLE_LOADED_CHUNKS__\";"),"Loadable Components"),"a different chunk expression is not the loadable chunk registry");
    check(!has(script("var pkg=JSON.parse('{\"name\":\"@rive-app/canvas\"}');"),"Rive"),"package name without its stated version is insufficient");
    check(!has(script("return{name:e,value:t,rating:`good`,delta:0,entries:[],id:make(),navigationType:r}"),"web-vitals"),"a metric object without the identifier generator is insufficient");
    check(!has(script("var id=Math.floor(Math.random()*(9e12-1))+1e12;"),"web-vitals"),"an identifier generator without the metric object is insufficient");
}

void twenty_site_checks() {
    struct Library {const char* name;const char* version;const char* body;};
    const Library stated[]={
        {"React Router","6","var names=[\"onSubmit\",\"relative\",\"viewTransition\"];try{window.__reactRouterVersion=\"6\"}catch(e){}function C(e,t){return make(e,t)}"},
        {"React Router","7.99.2","try { window.__reactRouterVersion = \"7.99.2\"; } catch (e) {}"},
        {"Redux","","var o=function(){return Math.random().toString(36).substring(7).split(\"\").join(\".\")},u={INIT:\"@@redux/INIT\"+o(),REPLACE:\"@@redux/REPLACE\"+o(),PROBE_UNKNOWN_ACTION:function(){return\"@@redux/PROBE_UNKNOWN_ACTION\"+o()}};"},
        {"Redux","","const types={INIT:`@@redux/INIT${rand()}`,REPLACE:`@@redux/REPLACE${rand()}`,PROBE_UNKNOWN_ACTION:()=>`@@redux/PROBE_UNKNOWN_ACTION${rand()}`};"},
        {"MobX","","function die(n){throw new Error(typeof n==\"number\"?\"[MobX] minified error nr: \"+n+\". Find the full error at the project page\":\"[MobX] \"+n)}var ue=Symbol(\"mobx administration\");"},
        {"Lucide","","const w=n\?\?f?Number(r\?\?m)*24/Number(t\?\?p):r\?\?m;return u.createElement(\"svg\",{ref:E,width:t\?\?p,stroke:e\?\?v,strokeWidth:w,className:le(\"lucide\",h,c),...a},kids)"},
        {"Optimizely","5.99.1","var ne=\"javascript-sdk\",oe=\"node-sdk\",se=\"5.99.1\";function L(e){this.logToConsole=void 0===e.logToConsole||!!e.logToConsole,this.prefix=void 0!==e.prefix?e.prefix:\"[OPTIMIZELY]\"}!function(e){e.LOG_EVENT=\"LOG_EVENT:logEvent\",e.OPTIMIZELY_CONFIG_UPDATE=\"OPTIMIZELY_CONFIG_UPDATE\"}(W||(W={}));"},
        {"Optimizely","6.99.0","let G=\"$opt_bot_filtering\",W=\"$opt_bucketing_id\",Q=\"6.99.0\",Y=\"feature-test\";class _{constructor(e){this.prefix=e||\"[OPTIMIZELY]\"}}let eY={ACTIVATE:\"ACTIVATE\",OPTIMIZELY_CONFIG_UPDATE:\"OPTIMIZELY_CONFIG_UPDATE\",TRACK:\"TRACK\"};"},
        {"Optimizely","","class _{constructor(e){this.prefix=e||\"[OPTIMIZELY]\"}}let eY={OPTIMIZELY_CONFIG_UPDATE:\"OPTIMIZELY_CONFIG_UPDATE\"},engine=d||\"javascript-sdk\";"},
        {"mParticle","2.99.1","var w=\"2.99.1\",A=\"mparticle\",E={DeprecationMessages:{MethodHasBeenDeprecated:\"has been deprecated.\"}},H={localStorageName:\"mprtcl-api\",localStorageNameV3:\"mprtcl-v3\"},j={v1SecureServiceUrl:\"jssdks.mparticle.com/v1/JS/\",v2SecureServiceUrl:\"jssdks.mparticle.com/v2/JS/\"};"},
        {"mParticle","","var H={localStorageName:\"mprtcl-api\",cookieName:\"mprtcl-api\"},j={v3SecureServiceUrl:\"jssdks.mparticle.com/v3/JS/\"},list=[{name:\"mparticle\"}];"},
        {"RequireJS","2.99.5","var requirejs,require,define;(function(global){var req,s,head,subPath,version=\"2.99.5\",commentRegExp=/x/mg,op=Object.prototype;function load(node,context,moduleName){node.setAttribute(\"data-requirecontext\",context.contextName);node.setAttribute(\"data-requiremodule\",moduleName)}})(this);"},
        {"RequireJS","","var requirejs,require,define;(function(ba){var K=\"2.99.6\";function load(e,b,c){e.setAttribute(\"data-requirecontext\",b.contextName);e.setAttribute(\"data-requiremodule\",c)}})(this);"},
        {"Algolia","","function agent(a){var i={value:\"Algolia for JavaScript (\".concat(a,\")\"),add:function(e){return i}};return i}function auth(e,t,r){var n={\"x-algolia-api-key\":t,\"x-algolia-application-id\":e};return{headers:function(){return\"WithinHeaders\"===r?n:{}}}}"},
        {"Algolia","","function Zb(e){let t={value:`Algolia for JavaScript (${e})`,add(e){return t}};return t}function Qb(e,t,n=`WithinHeaders`){let r={\"x-algolia-api-key\":t,\"x-algolia-application-id\":e};return{headers(){return r}}}"}};
    for(const auto& [name,version,body]:stated) {
        const auto f=script(body);bool seen=false;
        for(const auto& x:f)if(x.name==name){seen=true;check(x.version==version,std::string(name)+" version comes only from the stated release");check(!x.evidence.empty(),std::string(name)+" keeps its evidence");}
        check(seen,std::string(name)+" implementation markers named");
        check(!has(script("/*"+std::string(body)+"*/"),name),std::string(name)+" commented code excluded");
    }
    struct Misleading {const char* name;const char* body;const char* reason;};
    const Misleading rejected[]={
        {"React Router","var note=\"window.__reactRouterVersion=\\\"6\\\"\";","a quoted router global is not the router"},
        {"React Router","window.__reactRouterVersion=release;","a router release held in a variable is not a stated release"},
        {"React Router","if(window.__reactRouterVersion===\"6\")warn();","reading the router global is not the router"},
        {"React Router","window.__reactRouterVersions=\"6\";","a lookalike global is not the router"},
        {"Redux","var a={INIT:\"@@redux/INIT\"+o()};","one unpaired Redux action type is insufficient"},
        {"Redux","if(action.type.startsWith(\"@@redux/INIT\")||action.type.startsWith(\"@@redux/REPLACE\"))return;","middleware that only tests for Redux action types is not Redux"},
        {"Redux","var doc=\"{INIT:\\\"@@redux/INIT\\\"+o(),REPLACE:\\\"@@redux/REPLACE\\\"+o()}\";","quoted Redux action types excluded"},
        {"MobX","var ue=Symbol(\"mobx administration\");var note=\"[MobX] minified error nr: \";","a MobX message held as data with one symbol is insufficient"},
        {"Lucide","const w=n?Number(r)*24/Number(t):r;var icon={strokeWidth:w,className:\"icon\"};","stroke scaling without the Lucide class is insufficient"},
        {"Lucide","var cls=join(\"lucide\",h);var size=24;","a class helper outside the element props is insufficient"},
        {"Optimizely","class Log{constructor(e){this.prefix=e||\"[OPTIMIZELY]\"}}","one unpaired Optimizely marker is insufficient"},
        {"Optimizely","var cfg={vendors:[{name:\"optimizely\"}]};window.optimizely=window.optimizely||[];var label=\"OPTIMIZELY_CONFIG_UPDATE\";","a vendor list and a queue are not the Optimizely SDK"},
        {"Optimizely","var text=\"this.prefix=e||\\\"[OPTIMIZELY]\\\"; OPTIMIZELY_CONFIG_UPDATE=\\\"OPTIMIZELY_CONFIG_UPDATE\\\"\";","quoted Optimizely code excluded"},
        {"mParticle","var vendors=[{id:\"62470f577e1e3605d5bc0b8a\",name:\"mparticle\"}];if(window.mParticle&&window.mParticle.addForwarder)window.mParticle.addForwarder({name:a});","a consent vendor list and a forwarder registration are not the mParticle SDK"},
        {"mParticle","var H={localStorageName:\"mprtcl-api\"};","one unpaired mParticle marker is insufficient"},
        {"mParticle","var H={localStorageName:\"mprtcl-api\"},j={v1SecureServiceUrl:\"jssdks.example.test/v1/JS/\"};","another service address is not mParticle"},
        {"RequireJS","var requirejs,require,define;","one unpaired RequireJS marker is insufficient"},
        {"RequireJS","if(typeof define===\"function\"&&define.amd)define([\"exports\"],factory);var id=node.getAttribute(\"data-requiremodule\");","an AMD wrapper that reads a module attribute is not RequireJS"},
        {"Algolia","var hosts=[{url:\"\".concat(f,\"-1.algolianet.com\"),accept:\"readWrite\"}];var h=e.request.headers[\"x-algolia-api-key\"]?{\"x-algolia-api-key\":\"*****\"}:{};","host names and a masked header are not the paired client markers"},
        {"Algolia","var n={\"x-algolia-api-key\":t,\"x-algolia-application-id\":e};","one unpaired Algolia marker is insufficient"},
        {"Algolia","var note=\"value:\\\"Algolia for JavaScript (\\\".concat(a)\";var n={\"x-algolia-api-key\":t,\"x-algolia-application-id\":e};","a quoted agent string is not the client"}};
    for(const auto& [name,body,reason]:rejected)check(!has(script(body),name),reason);
    const auto named=script("var Ar=\"2.99.4\",m={sdkVersion:Ar,sdkVendor:\"mparticle\",platform:\"web\",StorageNames:{localStorageName:\"mprtcl-api\"}},u={v1SecureServiceUrl:\"jssdks.mparticle.com/v1/JS/\"};");
    check(has(named,"mParticle")&&named[0].version=="2.99.4","mParticle release read through the constant its own field names");
    const auto other=script("var Ar=\"2.99.4\",Zz=\"9.9.9\",m={sdkVersion:Zz,sdkVendor:\"mparticle\",StorageNames:{localStorageName:\"mprtcl-api\"}},u={v1SecureServiceUrl:\"jssdks.mparticle.com/v1/JS/\"};");
    check(has(other,"mParticle")&&other[0].version=="9.9.9","mParticle release follows the named constant, not a neighbour");
    const auto unnamed=script("var Ar=\"2.99.4\",m={sdkVersion:load(),sdkVendor:\"mparticle\",StorageNames:{localStorageName:\"mprtcl-api\"}},u={v1SecureServiceUrl:\"jssdks.mparticle.com/v1/JS/\"};");
    check(has(unnamed,"mParticle")&&unnamed[0].version.empty(),"mParticle release left blank when no constant states it");
    const std::pair<const char*,const char*> endpoints[]={
        {"https://cdn.tinypass.com/api/tinypass.min.js","Piano"},
        {"https://static.elfsight.com/platform/platform.js","Elfsight"},
        {"https://elfsightcdn.com/platform.js","Elfsight"},
        {"https://dev.visualwebsiteoptimizer.com/j.php","VWO"}};
    for(const auto& [url,name]:endpoints) {
        check(vendor(request(std::string(url)+"?a=PRIVATE"),name),std::string(name)+" documented script endpoint");
        check(report_json(request(std::string(url)+"?a=PRIVATE")).find("PRIVATE")==std::string::npos,"provider request query value omitted");
        check(!vendor(request(url,0),name),std::string(name)+" failed request is not an observed provider");
        const auto slash=std::string(url).find('/',8);auto misleading=std::string(url);misleading.insert(slash,".unseen.example");
        check(!vendor(request(misleading),name),std::string(name)+" lookalike host suffix rejected");
        misleading=std::string(url);misleading.insert(8,"unseen.");check(!vendor(request(misleading),name),std::string(name)+" unrelated subdomain rejected");
        check(!vendor(request(std::string(url)+".map"),name),std::string(name)+" unrelated path rejected");
    }
    const auto router=script("try{window.__reactRouterVersion=\"6\"}catch(e){}");
    check(router.size()==1&&router[0].category=="Framework"&&router[0].confidence=="High","React Router states its major release in its own global");
}
