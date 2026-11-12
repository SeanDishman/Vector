#include "vector/api_endpoints.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
namespace {
int checks=0;
void check(bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);}
ScanReport sample(const std::string& url="https://site.example/"){ScanReport r;r.target=parse_target(url);r.web_coverage.started=true;return r;}
void request(ScanReport& r,const std::string& type,const std::string& method,const std::string& url,long status=200,
             const std::string& mime="application/json",const std::string& state="",const std::string& page="") {
    BrowserRequest q;q.url=url;q.page=page.empty()?r.target.url():page;q.type=type;q.method=method;q.status=status;q.mime=mime;
    q.state=!state.empty()?state:status>0?"complete":"failed";if(status<=0)q.error="net::ERR_FAILED";
    r.browser.requests.push_back(q);
}
HttpObservation page(const std::string& url,const std::string& body,const std::string& type="text/html",long status=200) {
    HttpObservation h;h.url=url;h.status=status;h.body=body;h.headers={{"content-type",type}};return h;
}
const ApiEndpoint* find(const std::vector<ApiEndpoint>& rows,const std::string& method,const std::string& url) {
    for(const auto& row:rows)if(row.method==method&&row.url==url)return &row;
    return nullptr;
}
ApiEndpoint get(const std::vector<ApiEndpoint>& rows,const std::string& method,const std::string& url) {
    const auto* row=find(rows,method,url);
    if(!row) {
        std::string seen;for(const auto& r:rows)seen+="\n  ["+r.method+"] "+r.url+" ("+r.basis+")";
        throw std::runtime_error("missing endpoint ["+method+"] "+url+seen);
    }
    return *row;
}
bool absent(const std::vector<ApiEndpoint>& rows,const std::string& part) {
    return std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.url.find(part)!=std::string::npos;});
}
bool proof(const ApiEndpoint& row,const std::string& part) {
    return std::any_of(row.evidence.begin(),row.evidence.end(),[&](const auto& e){return (e.source+" | "+e.value).find(part)!=std::string::npos;});
}
std::vector<ApiEndpoint> code(const std::string& text,const std::string& script="https://site.example/static/app.js") {
    auto r=sample();collect_api_references(script,text,r);return api_endpoints(r);
}
}
int main(){try {
    {
        const auto rows=code(R"JS(
            fetch("/api/v1/items?token=PRIVATE");
            fetch("/api/v1/session",{credentials:"include",headers:{"x-a":"1"},method:"POST"});
            fetch("/search");
            axios.post("/orders",{});
            cache.get("/pricing");
            map.delete("/account");
            var x=new XMLHttpRequest();x.open("PUT","/api/profile");
            const u=`${base}/v2/users/${user.id}/orders?limit=${n}`;
            navigator.sendBeacon("/collect",d);
            const s=new WebSocket("wss://stream.site.example/ws/v5");
            $.getJSON("/status/summary");
        )JS");
        check(get(rows,"","/api/v1/items").basis=="Code reference","fetch argument is a code reference");
        check(get(rows,"","/api/v1/items").scope=="Host not stated","a relative literal does not state a host");
        check(proof(get(rows,"","/api/v1/items"),"fetch() argument")&&proof(get(rows,"","/api/v1/items"),"(query omitted)"),"call context and query omission recorded");
        check(get(rows,"POST","/api/v1/session").basis=="Code reference","method read from the options object");
        check(find(rows,"","/search")!=nullptr,"fetch argument without an API-shaped path is still a request URL");
        check(find(rows,"POST","/orders")!=nullptr,"write verb on any client names a request");
        check(absent(rows,"/pricing")&&absent(rows,"/account"),"ambiguous .get and .delete calls need an API-shaped path");
        check(find(rows,"PUT","/api/profile")!=nullptr,"XMLHttpRequest.open method and URL");
        check(find(rows,"","/v2/users/{}/orders")!=nullptr,"template placeholders collapse and the base is left unstated");
        check(find(rows,"POST","/collect")!=nullptr,"sendBeacon posts");
        check(get(rows,"GET","wss://stream.site.example/ws/v5").scope=="Subdomain","WebSocket constructor URL");
        check(find(rows,"GET","/status/summary")!=nullptr,"getJSON reads");
        for(const auto& row:rows)for(const auto& e:row.evidence)check((row.url+e.source+e.value).find("PRIVATE")==std::string::npos,"query values never kept");
    }
    {
        const auto rows=code(R"JS(
            api.get("/products");
            this.http.get("/billing/summary");
            httpClient.delete("/cart/item");
            o.Z.post("/orders",d);o.Z.get("/orders/history");
            $.ajax({url:"/cart/add",type:"POST",data:d});
            $.ajax({dataType:"json",url:"/cart/view"});
            useSWR("/user/me",fetcher);
            $fetch("/session/refresh",{method:"POST"});
            const req=new Request("/inbox/unread");
            const userEndpoint="/users/me";
            const ENDPOINTS={LOGIN:"/auth/login",LOGOUT:"/auth/logout"};
            const settings={apiPath:"/settings/profile"};
            cache.get("/pricing");map.delete("/account");n.post("/n-write");n.get("/n-read");
            router.get("/about");
            const og={url:"/welcome",title:"Welcome"};
            const routes={home:"/home",help:"/help/center"};
            const redirectUrl="/dashboard";
        )JS");
        check(find(rows,"GET","/products")!=nullptr,"get on a named API client");
        check(proof(get(rows,"GET","/products"),"on an HTTP client"),"client calls say why they count");
        check(find(rows,"GET","/billing/summary")!=nullptr,"get on this.http");
        check(find(rows,"DELETE","/cart/item")!=nullptr,"delete on an HTTP client");
        check(proof(get(rows,"GET","/orders/history"),"also sends writes"),"get on an object that also posts in the same script");
        check(find(rows,"POST","/cart/add")!=nullptr,"ajax options with a type");
        check(find(rows,"","/cart/view")!=nullptr,"ajax options with request keys before the url");
        check(find(rows,"","/user/me")!=nullptr,"useSWR key");
        check(find(rows,"POST","/session/refresh")!=nullptr,"$fetch with a method");
        check(find(rows,"","/inbox/unread")!=nullptr,"new Request");
        check(proof(get(rows,"","/users/me"),"variable userEndpoint"),"endpoint-named variable");
        check(proof(get(rows,"","/auth/login"),"of endpoints")&&find(rows,"","/auth/logout")!=nullptr,"endpoint table");
        check(find(rows,"","/settings/profile")!=nullptr,"apiPath key");
        check(absent(rows,"/pricing")&&absent(rows,"/account")&&absent(rows,"/n-read")&&absent(rows,"/about"),"maps, caches, one-letter objects and routers stay out");
        check(absent(rows,"/welcome")&&absent(rows,"/home")&&absent(rows,"/help/center")&&absent(rows,"/dashboard"),"page links stay out");
    }
    {
        const auto rows=code(R"JS(
            /* fetch("/api/block-comment") */
            // fetch("/api/line-comment")
            const pattern=/\/api\/regex-literal/;
            const prose="See the /api/prose docs for details";
            const docs="/docs/api/reference",locale="/en/api/overview",guide="/guides/v2/setup/start";
            const image="/api/logo.png",bundle="/static/app.js",sheet="https://cdn.site.example/v1/theme.css";
            const sample="https://api.example.com/v1/sample",local="https://api.myapp.test/v1/x",bare="https://localhost/api/x";
            const wild="https://assets.partner.io/api/v1/*",traversal="/api/../secret",spaced="/api/ v1";
            const svg="http://www.w3.org/2000/svg",share="https://twitter.com/intent/tweet";
            const dsn=`${protocol}//${host}${port}/api/`;
            const manual="https://docs.partner.io/api/",guide="https://developers.partner.io/v1/reference";
            const og={url:"https://www.site.example/about",title:"About"};
            const half="//api2.unify-test-1.";
            fetch("/static/data/fallback");
        )JS");
        check(rows.empty(),"misleading literals produce no endpoint: "+(rows.empty()?std::string{}:rows[0].url));
    }
    {
        const auto rows=code(R"JS(
            const a="https://api.partner.io/v1/track",b="https:\/\/gateway.site.example\/graphql";
            const c={baseURL:"https://backend.site.example"},d={url:"https://www.site.example/x-api/v3/user",type:"POST"};
            const e="//cdn.site.example/api/config",f="/x-api/",g="/x-api/v3/asset/coin",h="/fht/sirius-api/live/get-activity";
            const i="https://api.partner.io:443/v1/track",j="/api/markdown/:path*",k="/wp-admin/admin-ajax.php";
        )JS");
        check(get(rows,"","https://api.partner.io/v1/track").scope=="Other host","absolute API host literal");
        check(get(rows,"","https://api.partner.io/v1/track").evidence.size()==2,"a default port written out is the same endpoint, with both literals kept as evidence");
        check(get(rows,"","https://gateway.site.example/graphql").scope=="Subdomain","escaped slashes decoded");
        check(proof(get(rows,"","https://backend.site.example/"),"property baseURL"),"named base URL key");
        check(get(rows,"POST","https://www.site.example/x-api/v3/user").scope=="Same host","url key with a type beside it; www alias is the same site");
        check(proof(get(rows,"","https://cdn.site.example/api/config"),"scheme not stated"),"protocol-relative literal");
        check(absent(rows,"/x-api/")==false&&find(rows,"","/x-api/")==nullptr,"bare prefix dropped when a longer path exists");
        check(find(rows,"","/x-api/v3/asset/coin")&&find(rows,"","/fht/sirius-api/live/get-activity"),"prefixed api segments");
        check(find(rows,"","/api/markdown/:path*")&&find(rows,"","/wp-admin/admin-ajax.php"),"route pattern and server script");
        check(rows.front().scope!="Other host"&&rows.back().scope=="Other host","the inspected site's rows come first");
    }
    {
        auto r=sample();
        collect_api_references("https://js.stripe.com/v3/","const a=\"/api/v1/internal\",b=\"https://api.stripe.com/v1/tokens\";",r);
        collect_api_references("https://cdn.jsdelivr.net/gh/site/app/main.js","const c=\"/api/v1/hosted\";",r);
        const auto rows=api_endpoints(r);
        check(absent(rows,"/api/v1/internal"),"a relative path inside a named provider's script is not listed against the site");
        check(get(rows,"","https://api.stripe.com/v1/tokens").provider=="Stripe","absolute provider endpoint keeps its provider");
        check(find(rows,"","/api/v1/hosted")!=nullptr,"a script on a CDN is not treated as a provider's own code");
        auto own=sample("https://js.stripe.com/");
        collect_api_references("https://js.stripe.com/v3/","const a=\"/api/v1/internal\";",own);
        check(find(api_endpoints(own),"","/api/v1/internal")!=nullptr,"when the provider's own site is inspected its scripts are the site's code");
        const auto after=code("function q(s){return /[\"']/.test(s)}fetch(\"/api/after-regex\");const t=typeof /'/;fetch('/api/after-keyword');");
        check(find(after,"","/api/after-regex")&&find(after,"","/api/after-keyword"),"quotes inside a regular expression after a keyword do not hide later literals");
    }
    {
        auto r=sample();
        request(r,"XHR","GET","https://site.example/api/items");
        request(r,"Fetch","GET","https://site.example/pricing",200,"text/x-component");
        request(r,"Fetch","GET","https://site.example/pricing",0,"","interrupted");
        request(r,"XHR","GET","https://site.example/page-data/index/page-data.json");
        request(r,"Fetch","POST","https://px.vendor.example/collect",0,"");
        request(r,"Fetch","GET","https://x.site.example/probe",0,"");
        request(r,"Fetch","GET","https://x.site.example/api/probe",0,"");
        request(r,"Other","OPTIONS","https://site.example/api/items",204,"");
        request(r,"Other","GET","https://iapi.site.example/api/internal/meta");
        request(r,"Other","GET","https://site.example/config.json");
        request(r,"Script","GET","https://site.example/api/widget",200,"text/javascript");
        request(r,"Image","GET","https://site.example/api/pixel",200,"image/gif");
        request(r,"Fetch","GET","https://site.example/api/logo",200,"image/png");
        request(r,"Fetch","GET","https://site.example/assets/home.lottie",200,"application/octet-stream");
        request(r,"EventSource","GET","https://cdn.flags.example/sub/stream",200,"text/event-stream","interrupted");
        request(r,"Ping","POST","https://site.example/analytics/page");
        request(r,"WebSocket","GET","wss://site.example/socket",101,"","open");
        request(r,"WebSocket","GET","wss://feed.other.example/live",0,"","failed");
        request(r,"Fetch","POST","https://o1.ingest.sentry.io/api/4504531942572032/envelope/");
        request(r,"Fetch","GET","https://site.example/api/users/8f14e45f-ceea-467f-a0e6-5c1d9e2b7a11/orders/1234567");
        request(r,"Fetch","GET","https://site.example/api/users/11111111-2222-3333-4444-555555555555/orders/7654321");
        request(r,"Fetch","GET","https://1-ggnpe76sjw2uhb5emcdticn4ui-p0.client.protect.site.example/ins_1lyWDZiobr600AKUeQDoSlrEmoM/probe",200,"text/plain");
        request(r,"Fetch","GET","https://site.example/homepage-card-expenses-mobile-0413/otFloatingRoundedCorner",200,"text/plain");
        request(r,"Fetch","POST","https://api.ffbbbdc6d3c353211fe2ba39c9f744cd.com/p/front",200,"application/octet-stream");
        BrowserPage trust;trust.url="https://trust.vendor.example/";trust.purpose="disclosure";r.browser.pages.push_back(trust);
        request(r,"Fetch","POST","https://trust.vendor.example/graphql",200,"application/json","",trust.url);
        const auto rows=api_endpoints(r);
        const auto& items=get(rows,"GET","https://site.example/api/items");
        check(items.basis=="Observed request"&&items.scope=="Same host"&&items.status==200,"answered XHR is an observed request");
        check(items.label=="GET    /api/items","the inspected origin shows the path only: "+items.label);
        check(items.evidence.size()==1,"preflight is not a second observation");
        check(absent(rows,"/pricing"),"page prefetches are not API endpoints, including an interrupted repeat");
        check(absent(rows,"page-data")&&absent(rows,"config.json")&&absent(rows,".lottie"),"static files fetched by code are not API endpoints");
        check(get(rows,"POST","https://px.vendor.example/collect").basis=="Request attempt","failed write is an attempt, not an observation");
        check(proof(get(rows,"POST","https://px.vendor.example/collect"),"no response"),"attempt says no response arrived");
        check(absent(rows,"x.site.example/probe"),"an unanswered plain GET is not listed");
        check(get(rows,"GET","https://x.site.example/api/probe").basis=="Request attempt","an unanswered API-shaped GET is an attempt");
        check(get(rows,"GET","https://iapi.site.example/api/internal/meta").scope=="Subdomain","preloaded API response");
        check(absent(rows,"/api/widget")&&absent(rows,"/api/pixel")&&absent(rows,"/api/logo"),"scripts and images are not API endpoints");
        check(get(rows,"GET","https://cdn.flags.example/sub/stream").basis=="Observed request","event stream");
        check(find(rows,"POST","https://site.example/analytics/page")!=nullptr,"beacon");
        check(get(rows,"GET","wss://site.example/socket").label=="GET    wss://site.example/socket","WebSocket keeps its scheme: "+get(rows,"GET","wss://site.example/socket").label);
        check(get(rows,"GET","wss://feed.other.example/live").basis=="Request attempt","WebSocket without a handshake response is an attempt");
        const auto& sentry=get(rows,"POST","https://o1.ingest.sentry.io/api/{id}/envelope/");
        check(sentry.provider=="Sentry"&&sentry.scope=="Other host","known provider named on its endpoint");
        const auto& orders=get(rows,"GET","https://site.example/api/users/{id}/orders/{id}");
        check(orders.evidence.size()==2&&proof(orders,"8f14e45f-ceea-467f-a0e6-5c1d9e2b7a11"),"identifiers collapse into one row and the evidence keeps the URL");
        check(find(rows,"GET","https://{id}.client.protect.site.example/{id}/probe")!=nullptr,"token-like host label and path segment collapse");
        check(find(rows,"GET","https://site.example/homepage-card-expenses-mobile-0413/otFloatingRoundedCorner")!=nullptr,"words with hyphens and camel case are not identifiers");
        check(find(rows,"POST","https://api.ffbbbdc6d3c353211fe2ba39c9f744cd.com/p/front")!=nullptr,"a registered domain is never shortened, however random it looks");
        check(proof(items,"Browser request / https://site.example/"),"evidence names the page without its default port");
        check(absent(rows,"trust.vendor.example"),"requests made by a separately rendered disclosure page are excluded");
        check(rows.front().scope=="Same host"&&rows.front().basis=="Observed request"&&rows.front().path.starts_with("/api/"),"observed same-site API-shaped rows lead: "+rows.front().url);
        check(rows.back().provider=="Sentry","named providers sort last: "+rows.back().url);
        collect_api_references("https://site.example/app.js","fetch(\"/api/items\");const p=\"/api/users/\"+id+\"/orders/\"+n,q=\"/api/reports/export\";",r);
        const auto merged=api_endpoints(r);
        check(merged.size()==rows.size()+1&&find(merged,"","/api/reports/export"),"only the unseen code path adds a row");
        check(proof(get(merged,"GET","https://site.example/api/items"),"fetch() argument")&&get(merged,"GET","https://site.example/api/items").basis=="Observed request","code evidence joins the observed row");
        check(find(merged,"","/api/users/")==nullptr,"a bare prefix of an observed path is not a separate row");
    }
    {
        auto r=sample();
        for(int i=0;i<12;++i)request(r,"Fetch","GET","https://site.example/api/poll/"+std::to_string(1000000+i));
        const auto rows=api_endpoints(r);
        check(rows.size()==1&&rows[0].evidence.size()==ApiEndpointList::evidence_limit+1,"evidence per row is capped");
        check(rows[0].evidence.back().source=="More evidence"&&rows[0].evidence.back().value.starts_with("4 further"),"the cap states how many were left out");
    }
    {
        auto r=sample();
        r.web.push_back(page("https://site.example:443/",R"HTML(<html><head>
            <link rel="https://api.w.org/" href="/wp-json/"><link rel="alternate" type="application/json" href="https://site.example/wp-json/wp/v2/pages/2">
            <link rel="EditURI" type="application/rsd+xml" href="https://site.example/xmlrpc.php?rsd">
            <link rel="stylesheet" href="/api/theme.css"><link rel="alternate" type="application/rss+xml" href="/feed/"></head><body>
            <form action="/api/subscribe?list=PRIVATE" method="post"></form><form action="/search"></form><form action="https://forms.vendor.example/contact"></form>
            <button hx-post="/items/add">Add</button><div data-hx-get="/fragments/cart"></div>
            <template><form action="/api/inert"></form></template><!-- <form action="/api/commented"> -->
            <script>fetch("/api/inline",{method:"DELETE"});</script>
            <script type="application/json">{"props":{"apiUrl":"https:\/\/api.site.example\/v2","page":"/docs/api/intro"}}</script>
            <script type="text/template">fetch("/api/template")</script>
            </body></html>)HTML"));
        r.web.back().headers.emplace_back("link","<https://site.example/wp-json/>; rel=\"https://api.w.org/\", <https://site.example/?p=2>; rel=shortlink");
        r.web.push_back(page("https://site.example:443/robots.txt","User-agent: *\nDisallow: /api/private/ # internal\nDisallow: /images/\nAllow: /graphql$\nDisallow: /*.json$\nDisallow: /docs/api/\nDisallow: /api/export?token=PRIVATE\n","text/plain"));
        const auto rows=api_endpoints(r);
        const auto& root=get(rows,"","https://site.example/wp-json/");
        check(root.basis=="Page declaration"&&root.scope=="Same host","WordPress REST root declared by the page");
        check(proof(root,"Link response header"),"the response header names the same root");
        check(find(rows,"","https://site.example/wp-json/wp/v2/pages/2")&&find(rows,"","https://site.example/xmlrpc.php"),"JSON alternate and XML-RPC discovery link");
        check(absent(rows,"theme.css")&&absent(rows,"/feed/"),"stylesheets and feeds are not API endpoints");
        check(get(rows,"POST","https://site.example/api/subscribe").basis=="Page declaration","API-shaped form action with its method");
        check(absent(rows,"/search")&&absent(rows,"forms.vendor.example"),"ordinary form destinations are not API endpoints");
        check(find(rows,"POST","https://site.example/items/add")&&find(rows,"GET","https://site.example/fragments/cart"),"htmx request attributes");
        check(absent(rows,"inert")&&absent(rows,"commented")&&absent(rows,"/api/template"),"inert markup, comments and non-script templates are ignored");
        check(get(rows,"DELETE","/api/inline").basis=="Code reference","inline script literal");
        check(get(rows,"","https://api.site.example/v2").scope=="Subdomain"&&absent(rows,"/docs/api"),"JSON data block: API base kept, documentation path ignored");
        check(get(rows,"","https://site.example/api/private/").basis=="robots.txt","API-shaped robots.txt rule");
        check(find(rows,"","https://site.example/graphql")&&absent(rows,"/images/")&&absent(rows,".json"),"robots.txt: anchors trimmed, ordinary and wildcard rules ignored");
        check(proof(get(rows,"","https://site.example/api/export"),"Disallow: /api/export")&&!proof(get(rows,"","https://site.example/api/export"),"PRIVATE"),"robots.txt rule kept without its query");
        for(const auto& row:rows)for(const auto& e:row.evidence)check((row.url+e.value).find("PRIVATE")==std::string::npos,"form query values never kept");
        auto rendered=sample();
        collect_api_references(page("https://site.example/","<script>fetch('/api/rendered')</script><div hx-get='/api/fragment'></div>"),rendered,"Rendered DOM / ");
        const auto drawn=api_endpoints(rendered);
        check(proof(get(drawn,"","/api/rendered"),"Rendered DOM / Inline script https://site.example/"),"rendered document keeps its origin label");
        check(get(drawn,"GET","https://site.example/api/fragment").basis=="Page declaration","rendered markup declaration");
        collect_api_references(page("https://site.example/","<script>fetch('/api/failed')</script>","text/html",500),rendered);
        collect_api_references(page("https://site.example/x.json","{\"a\":\"/api/json\"}","application/json"),rendered);
        check(absent(api_endpoints(rendered),"/api/failed")&&absent(api_endpoints(rendered),"/api/json"),"error pages and non-HTML bodies supply no page references");
    }
    {
        auto r=sample();
        r.web.push_back(page("https://site.example/openapi.json",R"JSON({"openapi":"3.0.1","servers":[{"url":"/store/v3"}],"paths":{
            "/pets":{"get":{"operationId":"listPets"},"post":{},"parameters":[]},"/pets/{petId}":{"delete":{"operationId":"deletePet"}},"bad path":{"get":{}}}})JSON","application/json"));
        r.web.push_back(page("https://site.example/v2/api-docs",R"JSON({"swagger":"2.0","host":"api.site.example","basePath":"/rest/","schemes":["https"],"paths":{"/orders":{"put":{}}}})JSON","application/json;charset=UTF-8"));
        r.web.push_back(page("https://site.example/swagger.json","{\"error\":\"Not found\"}","application/json",404));
        r.web.push_back(page("https://site.example/graphql","{\"errors\":[{\"message\":\"Must provide query string.\"}]}","application/json",400));
        r.web.push_back(page("https://site.example/v3/api-docs","<!doctype html><html></html>","text/html"));
        r.web.push_back(page("https://site.example/data","{\"paths\":{\"/x\":{\"get\":{}}}}","application/json"));
        request(r,"Fetch","DELETE","https://site.example/store/v3/pets/42");
        const auto rows=api_endpoints(r);
        check(get(rows,"GET","https://site.example/openapi.json").basis=="Collected response"&&proof(get(rows,"GET","https://site.example/openapi.json"),"API description document"),"description document answered Vector's own read");
        check(get(rows,"GET","https://site.example/store/v3/pets").basis=="API description","operation resolved against the relative server URL");
        check(proof(get(rows,"GET","https://site.example/store/v3/pets"),"operationId listPets"),"operation evidence");
        check(find(rows,"POST","https://site.example/store/v3/pets")!=nullptr&&absent(rows,"bad path")&&absent(rows,"PARAMETERS"),"only HTTP methods on valid paths");
        const auto& removed=get(rows,"DELETE","https://site.example/store/v3/pets/42");
        check(removed.basis=="Observed request"&&proof(removed,"operationId deletePet"),"a described operation joins the matching observed request");
        check(absent(rows,"{petId}"),"the template row is not repeated beside its observed request");
        check(get(rows,"PUT","https://api.site.example/rest/orders").scope=="Subdomain","Swagger 2 host and base path");
        check(absent(rows,"swagger.json"),"a JSON error is not an endpoint");
        check(proof(get(rows,"GET","https://site.example/graphql"),"GraphQL-style JSON error body"),"GraphQL error answer to the existing probe");
        check(absent(rows,"/v3/api-docs"),"an HTML answer to a probe is not an endpoint");
        check(get(rows,"GET","https://site.example/data").basis=="Collected response"&&absent(rows,"site.example/x"),"JSON without an OpenAPI marker lists no operations");
    }
    {
        auto r=sample("http://127.0.0.1:8080/app");
        request(r,"Fetch","GET","http://127.0.0.1:8080/api/a");request(r,"Fetch","GET","http://127.0.0.1:9000/api/b");
        request(r,"Fetch","GET","http://localhost:8080/api/c");request(r,"WebSocket","GET","ws://127.0.0.1:8080/ws",101,"","open");
        const auto rows=api_endpoints(r);
        check(get(rows,"GET","http://127.0.0.1:8080/api/a").label=="GET    /api/a","explicit port origin shows the path only");
        check(get(rows,"GET","http://127.0.0.1:9000/api/b").label=="GET    127.0.0.1:9000/api/b"&&get(rows,"GET","http://127.0.0.1:9000/api/b").scope=="Same host","another port of the same address is named");
        check(get(rows,"GET","http://localhost:8080/api/c").scope=="Other host","an IP target has no inferred aliases");
        check(get(rows,"GET","ws://127.0.0.1:8080/ws").label=="GET    ws://127.0.0.1:8080/ws","plain WebSocket keeps scheme and port");
    }
    {
        auto r=sample();std::string many;
        for(int i=0;i<90;++i)many+="fetch(\"/api/bulk/item"+std::to_string(i)+"\");";
        collect_api_references("https://site.example/big.js",many,r);
        check(r.api_references.size()==ApiEndpointList::references_per_source&&r.api_references_truncated,"references per script are capped and the cap is recorded");
        const auto list=api_endpoint_list(r);
        check(list.truncated&&list.rows.size()==ApiEndpointList::references_per_source,"the list reports the cap");
        auto full=sample();
        for(int i=0;i<300;++i)request(full,"Fetch","GET","https://site.example/api/row"+std::to_string(i));
        const auto capped=api_endpoint_list(full);
        check(capped.rows.size()==ApiEndpointList::row_limit&&capped.truncated,"row limit");
        const std::string huge(3*1024*1024,' ');auto late=sample();
        collect_api_references("https://site.example/late.js",huge+"fetch(\"/api/late\")",late);
        check(late.api_references.empty(),"only the first 2 MiB of a script is read");
        auto crawl=sample();std::string rules="User-agent: *\n";
        for(int i=0;i<60;++i)rules+="Disallow: /api/area"+std::to_string(i)+"/\n";
        crawl.web.push_back(page("https://site.example/robots.txt",rules,"text/plain"));
        const auto listed=api_endpoint_list(crawl);
        check(listed.rows.size()==48&&listed.truncated,"robots.txt rules are capped and the cap is reported: "+std::to_string(listed.rows.size()));
        auto groups=sample();rules.clear();
        for(int group=0;group<5;++group){rules+="User-agent: bot"+std::to_string(group)+"\n";for(int i=0;i<20;++i)rules+="Disallow: /api/area"+std::to_string(i)+"/\n";}
        groups.web.push_back(page("https://site.example/robots.txt",rules,"text/plain"));
        const auto repeated=api_endpoint_list(groups);
        check(repeated.rows.size()==20&&!repeated.truncated,"a rule repeated for several crawlers counts once: "+std::to_string(repeated.rows.size()));
        auto vendor=sample();std::string bulk;
        for(int i=0;i<90;++i)bulk+="fetch(\"/api/vendor/item"+std::to_string(i)+"\");";
        collect_api_references("https://js.stripe.com/v3/",bulk,vendor);
        check(vendor.api_references.empty()&&!vendor.api_references_truncated,"a provider's own relative paths use no reference budget");
        const auto plain=code("const a=\"/api/one\",b=\"/api/two\";");
        check(plain.size()==2&&plain[0].label=="/api/one","a list without any method has no empty method column: "+plain[0].label);
    }
    {
        auto r=sample();request(r,"Fetch","GET","https://site.example/api/items?token=PRIVATE");
        collect_api_references("https://site.example/app.js?v=PRIVATE","fetch(\"/api/other?key=PRIVATE\")",r);
        const auto exported=nlohmann::json::parse(report_json(r));
        check(exported.at("api_endpoints").size()==api_endpoints(r).size()&&exported.at("api_endpoints").size()==2,"projection exported with the report");
        check(exported.at("api_endpoints").dump().find("PRIVATE")==std::string::npos,"exported endpoints carry no query values");
        check(exported.at("api_endpoints")[0].at("meaning").get<std::string>().find("did not call")!=std::string::npos,"export states Vector did not call the endpoint");
        check(exported.at("api_endpoint_coverage").at("endpoints_requested_by_vector")==false&&exported.at("api_endpoint_coverage").at("truncated")==false,"coverage block");
        const auto demo=demo_report();const auto shown=api_endpoints(demo);
        check(shown.size()==7&&shown[0].scope=="Same host"&&shown[0].basis=="Page declaration","example report lists illustrative endpoints: "+std::to_string(shown.size()));
        check(nlohmann::json::parse(report_json(demo)).at("api_endpoints").size()==7,"example export");
        check(api_endpoints(ScanReport{}).empty()&&api_endpoints(sample()).empty(),"empty reports stay empty");
        check(api_endpoint_tag(shown[0])=="Declared"&&!api_endpoint_meaning(shown[0]).empty(),"row tag and meaning");
    }
    std::cout<<checks<<" API endpoint checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
