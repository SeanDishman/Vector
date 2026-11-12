
std::vector<Finding> stylesheet(const std::string& body) {
    std::vector<Finding> f;identify_stylesheet("https://unseen.example/opaque",body,f);return f;
}
std::vector<Finding> page_markup(const std::string& body) {
    HttpObservation h;h.url="https://unseen.example/";h.status=200;h.body=body;std::vector<Finding> f;identify_http(h,f);return f;
}
std::vector<Finding> script_url(const std::string& url) {
    std::vector<Finding> f;identify_script_reference(url,"https://unseen.example/ | script reference",f);return f;
}
const Finding* named(const std::vector<Finding>& f,const std::string& name) {
    for(const auto& x:f)if(x.name==name)return &x;
    return nullptr;
}

void page_rule_checks() {
    const auto gatsby=page_markup("<html><head><meta name=\"generator\" content=\"Gatsby 4.25.9\"/></head><body></body></html>");
    check(named(gatsby,"Gatsby")&&named(gatsby,"Gatsby")->version=="4.25.9"&&named(gatsby,"Gatsby")->category=="Framework","Gatsby generator with its version");
    check(named(page_markup("<meta name=\"generator\" content=\"Gatsby\">"),"Gatsby")!=nullptr,"Gatsby generator without a version");
    for(const auto* body:{"<meta name=\"description\" content=\"Gatsby 4.25.9\">","<meta name=\"generator\" content=\"Not Gatsby 4.25.9\">",
                          "<meta name=\"generator\" content=\"Gatsby-like builder\">","<p>Built with Gatsby 4.25.9</p>",
                          "<template><meta name=\"generator\" content=\"Gatsby 4.25.9\"></template>"})
        check(!named(page_markup(body),"Gatsby"),"Gatsby needs the page's own generator element");

    check(named(script_url("https://cal.com/docs/_mintlify/assets/index-DnmxF9Sy.js?dpl=private"),"Mintlify")!=nullptr,"Mintlify script asset path");
    check(named(script_url("https://unseen.example/docs/_mintlify/custom/site/34a103d44dce6f34/site.js"),"Mintlify")!=nullptr,"Mintlify custom script path");
    check(named(page_markup("<link rel=\"stylesheet\" href=\"/docs/_mintlify/assets/index-BSsJ1SRK.css?dpl=dpl_x\">"),"Mintlify")!=nullptr,"Mintlify stylesheet link");
    for(const auto* url:{"https://unseen.example/_mintlify.js","https://unseen.example/docs/mintlify/assets/index.js","https://unseen.example/_mintlify/assets/",
                         "https://unseen.example/_mintlify/assets/logo.png","https://unseen.example/static/x.js?ref=/_mintlify/assets/a.js"})
        check(!named(script_url(url),"Mintlify"),"Mintlify lookalike paths rejected");

    const auto fingerprint=script("/**\n * Fingerprint v4.1.6 - Copyright (c) FingerprintJS, Inc, 2026 (https://fingerprint.com)\n */\nconst y$1=\"client_timeout\";");
    check(named(fingerprint,"Fingerprint")&&named(fingerprint,"Fingerprint")->version=="4.1.6"&&named(fingerprint,"Fingerprint")->category=="Fraud prevention",
        "Fingerprint agent license banner");
    for(const auto& body:{std::string("/** Fingerprint v4.1.6 */"),std::string("const note=\"/** Fingerprint v4.1.6 - Copyright (c) FingerprintJS, Inc */\";"),
                          std::string("/** Example: Fingerprint v4.1.6 - Copyright (c) FingerprintJS, Inc */"),std::string("/** Fingerprint vNext - Copyright (c) FingerprintJS, Inc */")})
        check(!named(script(body),"Fingerprint"),"Fingerprint banner needs prefix, version and corroboration in a real comment");

    const auto base=page_markup("<button type=\"button\" data-base-ui-tooltip-trigger=\"\" id=\"base-ui-_R_6lmh4ltqbsdb_\">Docs</button>"
                                "<a href=\"/pricing\" data-base-ui-navigation-menu-trigger=\"\">Pricing</a>");
    check(named(base,"Base UI")&&named(base,"Base UI")->evidence.size()==2,"Base UI component attributes on real elements");
    check(named(page_markup("<button data-base-ui-tooltip-trigger=\"\" id=\"base-ui-_R_1_\">x</button>"),"Base UI")!=nullptr,"Base UI attribute with its generated id");
    for(const auto* body:{"<button data-base-ui-tooltip-trigger=\"\">x</button><a data-base-ui-tooltip-trigger=\"\">y</a>",
                          "<p>data-base-ui-tooltip-trigger data-base-ui-navigation-menu-trigger id=\"base-ui-_R_1_\"</p>",
                          "<template><button data-base-ui-tooltip-trigger=\"\" data-base-ui-click-trigger=\"\">x</button></template>",
                          "<meta data-base-ui-tooltip-trigger=\"\" data-base-ui-click-trigger=\"\">",
                          "<div data-base-uix-trigger=\"\" data-base-ui-=\"\" id=\"base-ui-\"></div>",
                          "<div id=\"base-ui-_R_1_\"></div><div id=\"base-ui-_R_2_\"></div>"})
        check(!named(page_markup(body),"Base UI"),"Base UI needs two component attributes, or one plus a generated id");

    const std::string preflight="*,:after,:before{--tw-border-spacing-x:0;--tw-translate-x:0;--tw-ring-shadow:0 0 #0000;--tw-shadow:0 0 #0000}";
    const auto tailwind=page_markup("<html><head><style data-href=\"/styles.css\">"+preflight+"</style></head></html>");
    check(named(tailwind,"Tailwind CSS")!=nullptr,"Tailwind CSS in an inline style element");
    check(named(tailwind,"Tailwind CSS")->evidence[0].source=="https://unseen.example/ | inline style | stylesheet declarations","inline style evidence names the page");
    const auto banner=page_markup("<style>/*! tailwindcss v4.1.18 | MIT License | https://tailwindcss.com */\n@layer theme{:root{--font-sans:x}}</style>");
    check(named(banner,"Tailwind CSS")&&named(banner,"Tailwind CSS")->version=="4.1.18","Tailwind CSS banner in an inline style element");
    for(const auto& body:{"<p>"+preflight+"</p>","<template><style>"+preflight+"</style></template>","<noscript><style>"+preflight+"</style></noscript>",
                          "<script>const css=`"+preflight+"`;</script>","<style>.doc{content:\""+preflight+"\"}</style>","<style>/*"+preflight+"*/</style>"})
        check(!named(page_markup(body),"Tailwind CSS"),"inline style rules need real style element declarations");
    check(!named(page_markup("<style>"+std::string(600*1024,' ')+preflight+"</style>"),"Tailwind CSS"),"inline style text beyond the sample excluded");

    const auto headless=page_markup("<button data-headlessui-state=\"selected\" id=\"headlessui-tabs-tab-_R_2q355_\">Plans</button><div>x</div>");
    check(named(headless,"Headless UI")&&named(headless,"Headless UI")->evidence.size()==2,"Headless UI state attribute with its generated id");
    check(named(page_markup("<button data-headlessui-state=\"\">a</button><a data-headlessui-state=\"open\">b</a>"),"Headless UI")!=nullptr,
        "Headless UI state attribute on two elements");
    for(const auto* body:{"<button data-headlessui-state=\"\">a</button>","<div id=\"headlessui-menu-button-1\"></div><div id=\"headlessui-menu-items-2\"></div>",
                          "<template><button data-headlessui-state=\"\">a</button><a data-headlessui-state=\"\">b</a></template>",
                          "<p>data-headlessui-state data-headlessui-state id=\"headlessui-tabs-tab-1\"</p>",
                          "<button data-headlessui-statex=\"\" id=\"headlessui-\">a</button><meta data-headlessui-state=\"\">"})
        check(!named(page_markup(body),"Headless UI"),"Headless UI needs two state attributes, or one plus a generated id");

    check(named(page_markup("<meta data-mint-head=\"\" name=\"generator\" content=\"Mintlify\">"),"Mintlify")!=nullptr,"Mintlify generator element");
    check(!named(page_markup("<meta name=\"generator\" content=\"Mintlify-like docs\">"),"Mintlify"),"Mintlify generator must be exact");

    const auto via=[](const std::string& value,const std::string& header="via") {
        HttpObservation h;h.url="https://unseen.example/";h.status=200;h.headers={{header,value}};std::vector<Finding> f;identify_http(h,f);return f;
    };
    const auto balancer=via("1.1 google");
    check(named(balancer,"Google Cloud Load Balancing")&&named(balancer,"Google Cloud Load Balancing")->category=="Load balancer",
        "Google Cloud load balancer Via header");
    check(named(via("1.1 varnish, 1.1 google"),"Google Cloud Load Balancing")!=nullptr,"Google Cloud load balancer in a Via list");
    for(const auto& [value,header]:std::vector<std::pair<std::string,std::string>>{{"1.1 googlebot","via"},{"1.1 google-proxy","via"},{"2.0 google","via"},
            {"1.1 notgoogle","via"},{"1.1 google","x-via"},{"1.1 google","server"}})
        check(!named(via(value,header),"Google Cloud Load Balancing"),"Google Cloud load balancer needs the exact Via entry");
    ScanReport lb;lb.findings={{"React","Library","","Medium",{}},{"Google Cloud Load Balancing","Load balancer","","High",{}}};
    check(infrastructure_entries(lb).front().name=="Google Cloud Load Balancing","load balancer rows sort with edge rows");

    const auto clerk=script_url("https://clerk.unseen.example/npm/@clerk/clerk-js@6.42.3/dist/clerk.browser.js");
    check(named(clerk,"Clerk")&&named(clerk,"Clerk")->version=="6.42.3"&&named(clerk,"Clerk")->category=="Authentication","Clerk script path with its exact release");
    const auto clerk_major=script_url("https://clerk.unseen.example/npm/@clerk/clerk-js@6/dist/clerk.browser.js");
    check(named(clerk_major,"Clerk")&&named(clerk_major,"Clerk")->version.empty(),"Clerk major-version tag is not reported as a release");
    check(named(script_url("https://cdn.jsdelivr.net/npm/@clerk/clerk-js@5.99.0/dist/321_clerk.browser_232d43_5.99.0.js"),"Clerk")!=nullptr,"Clerk chunk from a package CDN");
    for(const auto* url:{"https://unseen.example/npm/@clerk/clerk-react@5.0.0/dist/index.js","https://unseen.example/npm/@clerk/clerk-js@6.42.3/dist/clerk.browser.css",
                         "https://unseen.example/npm/@clerk/clerk-js@6.42.3/clerk.browser.js","https://unseen.example/static/app.js?u=/npm/@clerk/clerk-js@6/dist/clerk.browser.js",
                         "https://unseen.example/npm/@notclerk/clerk-js@6/dist/clerk.browser.js"})
        check(!named(script_url(url),"Clerk"),"Clerk needs its own browser package path");

    ScanReport strapi;strapi.target=parse_target("https://unseen.example/");
    BrowserRequest q;q.page=strapi.target.url();q.url="https://better-animal-d658c56969.strapiapp.com/api/orders?token=PRIVATE";q.type="Fetch";q.method="GET";q.status=200;
    identify_browser_vendor(q,strapi);
    check(vendor(strapi,"Strapi Cloud")&&report_json(strapi).find("PRIVATE")==std::string::npos,"Strapi Cloud project host request");
    for(const auto* url:{"https://strapiapp.com.unseen.example/api/orders","https://notstrapiapp.com/api/orders","https://unseen.example/strapiapp.com/api"}) {
        ScanReport r;r.target=parse_target("https://unseen.example/");q.url=url;identify_browser_vendor(q,r);
        check(!vendor(r,"Strapi Cloud"),"Strapi Cloud lookalike hosts rejected");
    }
}

void overview_page_checks() {
    ScanReport r;r.target=parse_target("https://unseen.example/");
    r.findings={{"Google Workspace mail","Inbound email","","High",{{"MX","aspmx.l.google.com"}}},
                {"Cloudflare DNS","Authoritative DNS","","High",{{"NS","ns.cloudflare.com"}}},
                {"Google mail authorization","Email authorization","","High",{{"SPF","include:_spf.google.com"}}},
                {"Vercel","Edge","","High",{{"header","x-vercel-id"}}},
                {"Next.js","Framework","16.3.3","High",{{"script","window.next"}}},
                {"Tailwind CSS","Library","","Medium",{{"css","--tw-shadow"}}},
                {"Microsoft 365 mail","DNS hosting alias","","High",{{"CNAME","autodiscover.outlook.com"}}},
                {"Turbopack","Build tool","","Medium",{{"script","TURBOPACK"}}}};
    VendorRelationship v;v.name="Intercom";v.category="Support / chat";v.relationship="Script";v.evidence_level="Observed response";
    v.evidence={{"browser","GET https://widget.intercom.io/widget/x / HTTP 200 / Script"}};r.vendors.push_back(v);
    std::vector<std::string> names;for(const auto& row:infrastructure_entries(r))names.push_back(row.name);
    const std::vector<std::string> expected={"Vercel","Next.js","Tailwind CSS","Turbopack","Intercom",
        "Google Workspace mail","Cloudflare DNS","Google mail authorization","Microsoft 365 mail"};
    check(names==expected,"page technologies and observed integrations precede DNS and mail roles");
    for(const auto& row:infrastructure_entries(r))
        if(row.finding>=0)check(r.findings[row.finding].name==row.name,"reordered rows keep their finding index");
        else check(row.vendor==0&&row.name=="Intercom","reordered vendor row keeps its vendor index");
    ScanReport dns_only;dns_only.findings={r.findings[0],r.findings[1],r.findings[2]};
    std::vector<std::string> mail;for(const auto& row:infrastructure_entries(dns_only))mail.push_back(row.name);
    check(mail==std::vector<std::string>{"Google Workspace mail","Cloudflare DNS","Google mail authorization"},"DNS roles alone keep their order");

    const std::string neon="[data-radix-popper-content-wrapper]{z-index:50!important}.TooltipContent{max-width:var(--container-xs)}"
        ".data-\\[swipe\\=move\\]\\:translate-x-\\[var\\(--radix-toast-swipe-move-x\\)\\][data-swipe=move]{--tw-translate-x:var(--radix-toast-swipe-move-x);"
        "translate:var(--tw-translate-x) var(--tw-translate-y)}";
    const auto found=stylesheet(neon);check(has(found,"Radix UI"),"Radix UI selector and variable read");
    for(const auto& f:found)if(f.name=="Radix UI") {
        check(f.category=="Library"&&f.version.empty()&&f.evidence.size()==2,"Radix UI keeps two stylesheet values and no version");
        check(f.evidence[0].value=="--tw-translate-x:var(--radix-toast-swipe-move-x)","Radix UI variable read evidence");
        check(f.evidence[1].value=="[data-radix-popper-content-wrapper] selector","Radix UI selector evidence");
        check(f.evidence[0].source=="https://unseen.example/opaque | stylesheet rules","Radix UI evidence names the stylesheet");
    }
    const std::string escaped=".after\\:content-\\[\\'\\'\\]:after{--tw-content:\"\";content:var(--tw-content)}.bg-\\[url\\(\\\"\\/a\\.png\\\"\\)\\]{x:y}";
    check(has(stylesheet(escaped+neon),"Radix UI"),"escaped quotes in utility class names do not hide later rules");
    check(!has(stylesheet(".a{content:'"+neon+"'}"),"Radix UI"),"real quoted strings stay excluded after the escape rule");
    const std::string shadcn="@keyframes accordion-down{from{height:0}to{height:var(--radix-accordion-content-height)}}"
        ".h-\\(--radix-select-trigger-height\\){height:var(--radix-select-trigger-height)}";
    check(has(stylesheet(shadcn),"Radix UI"),"variable reads from two different Radix components");
    check(!has(stylesheet(".a{height:var(--radix-accordion-content-height)}.b{width:var(--radix-accordion-content-height)}"),"Radix UI"),
        "one Radix variable read twice is insufficient");
    check(!has(stylesheet(".s[data-swipe=move]{--tw-translate-x:var(--radix-toast-swipe-move-x)}.e[data-swipe=end]{--tw-translate-x:var(--radix-toast-swipe-end-x)}"),"Radix UI"),
        "two variables of one Radix component are insufficient");
    check(!has(stylesheet(".v{height:var(--radix-navigation-menu-viewport-height)}.w{width:var(--radix-navigation-menu-viewport-width)}"),"Radix UI"),
        "one multi-word Radix component stays one component");
    check(!has(stylesheet("[data-radix-popper-content-wrapper]{z-index:50}[data-radix-portal]{position:fixed}"),"Radix UI"),"selectors alone are insufficient");
    check(!has(stylesheet("/*"+neon+"*/"),"Radix UI"),"commented Radix rules excluded");
    check(!has(stylesheet(".doc{content:\""+std::string("[data-radix-portal]{x:1} ;height:var(--radix-accordion-content-height) ;width:var(--radix-select-trigger-width)")+"\"}"),"Radix UI"),
        "quoted Radix text excluded");
    check(!has(stylesheet(".translate-x-\\[var\\(--radix-toast-swipe-move-x\\)\\]{color:red}.h-\\[var\\(--radix-select-trigger-height\\)\\]{color:blue}"
                          ".x\\[data-radix-portal\\]{color:green}"),"Radix UI"),"escaped utility class names are not declarations");
    check(!has(stylesheet(".a{height:var(--radixx-accordion-height)}.b{width:var(--my-radix-select-width)}[data-radixy-portal]{x:1}"
                          ".c{height:var(--radix)}"),"Radix UI"),"lookalike variable and attribute names rejected");
    check(!has(stylesheet(":root{--radix-accordion-content-height:10px;--radix-select-trigger-width:5px}[data-radix-portal]{x:1}"),"Radix UI"),
        "defining Radix-named variables is not reading them");
    check(!has(stylesheet(std::string(600*1024,' ')+neon),"Radix UI"),"Radix rules beyond the stylesheet sample excluded");
    page_rule_checks();
}
