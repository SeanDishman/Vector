void discord_checks() {
    const std::string loader="function head(e){return e||document.head}var b=\"hCaptchaOnLoad\",S=\"script-error\";function params(e){return query({custom:e.custom,render:e.render,sentry:e.sentry,assethost:e.assethost,imghost:e.imghost,reportapi:e.reportapi,endpoint:e.endpoint,host:e.host,recaptchacompat:e.recaptchacompat,hl:e.hl})}";
    check(has(script(loader),"hCaptcha"),"hCaptcha loader hook name and query forwarding");
    const std::string ticked="const hook=`hCaptchaOnLoad`;function params(o){return query({host:o.host,recaptchacompat:o.recaptchacompat,hl:o.hl})}";
    check(has(script(ticked),"hCaptcha"),"hCaptcha loader with backtick strings");
    check(!has(script("var b=\"hCaptchaOnLoad\";window[b]=ready;"),"hCaptcha"),"one hCaptcha marker insufficient");
    check(!has(script("function params(e){return query({host:e.host,recaptchacompat:e.recaptchacompat})}"),"hCaptcha"),"hCaptcha query forwarding alone insufficient");
    check(!has(script("const allowed=[\"https://js.hcaptcha.com\",\"https://newassets.hcaptcha.com\"];const note=\"hCaptchaOnLoad recaptchacompat\";"),"hCaptcha"),"hCaptcha hosts in an allow-list and names in prose are not the loader");
    check(!has(script("/* var b=\"hCaptchaOnLoad\"; recaptchacompat:e.recaptchacompat */var ready=1;"),"hCaptcha"),"commented hCaptcha loader ignored");
    check(!has(script("var service={HCAPTCHA:\"hcaptcha\",RECAPTCHA:\"recaptcha\"};render({captchaService:service.HCAPTCHA});"),"hCaptcha"),"a captcha service list is not the hCaptcha loader");

    const auto score=[](const char* page,const char* link){return documentation_link_score(parse_target(page),parse_target(link));};
    check(score("https://discord.com/","https://discord.com/developers/docs/intro")>0,"documentation path on the page host");
    check(score("https://stripe.com/","https://docs.stripe.com/")>0,"documentation subdomain of the page host");
    check(score("https://www.kraken.com/","https://docs.kraken.com/")>0,"documentation subdomain of a www page");
    check(score("https://www.cloudflare.com/","https://developers.cloudflare.com/")>0,"developers subdomain");
    check(score("https://stripe.com/","https://docs.stripe.com/")>score("https://stripe.com/","https://stripe.com/docs/api/charges"),"a documentation host outranks a deep documentation path");
    check(score("https://site.example/","https://site.example/docs")>score("https://site.example/","https://site.example/blog/docs/old/page"),"a short documentation path outranks a deep one");
    check(score("http://127.0.0.1:8080/","http://127.0.0.1:8080/docs/intro")>0,"documentation path on an address");
    check(score("https://trello.com/","https://developer.atlassian.com/cloud/trello/")==0,"another company's developer host is not followed");
    check(score("https://stripe.com/","https://docs.stripe.com.other.example/")==0,"a lookalike suffix is not the site");
    check(score("https://stripe.com/","https://notdocs.stripe.com/")==0,"a subdomain that is not a documentation host");
    check(score("https://site.example/","https://site.example/documents/report")==0,"a documents path is not documentation");
    check(score("https://site.example/","https://site.example/docs-archive")==0,"a longer word is not the documentation segment");
    check(score("https://site.example/","http://site.example/docs")==0,"a scheme change is not followed");
    check(score("https://site.example/","https://site.example:8443/docs")==0,"another port is not followed");
    check(score("https://site.example/docs","https://site.example/docs")==0,"the page itself is not its own documentation link");
    check(score("http://127.0.0.1:8080/","http://localhost:8080/docs")==0,"another name for an address is not assumed to be the same site");
    check(score("https://localhost/","https://docs.localhost/")==0,"a single-label host has no documentation subdomain");

    ScanReport report;report.target=parse_target("https://site.example/");
    report.findings.push_back({"WordPress","CMS","6.8","High",{{"https://site.example:443/","generator"}}});
    report.findings.push_back({"Mintlify","Documentation","","High",{{"https://docs.site.example:443/intro","generator"}}});
    report.findings.push_back({"YouTube player","Library","","Medium",{{"Browser request from https://docs.site.example:443/intro","player"}}});
    LinkedPage linked;linked.role="documentation";linked.state="collected";linked.url="https://docs.site.example:443/intro";
    linked.discovered_from="https://site.example:443/";linked.first_seen={"Mintlify","YouTube player"};
    report.linked_pages.push_back(linked);
    const auto detail=[](const ScanReport& r,const std::string& name) {
        for(const auto& row:infrastructure_entries(r))if(row.name==name)return row.detail;
        return std::string("missing");
    };
    check(detail(report,"Mintlify")=="Documentation on docs.site.example","a name first seen on the linked page says where");
    check(detail(report,"YouTube")=="Video on docs.site.example","a renamed row first seen on the linked page says where");
    check(detail(report,"WordPress")=="CMS","a name from the entered site is unchanged");
    report.linked_pages[0].state="failed";
    check(detail(report,"Mintlify")=="Documentation","a failed linked page marks nothing");
    report.linked_pages[0].state="collected";report.linked_pages[0].url="https://site.example:443/docs/intro";
    check(detail(report,"Mintlify")=="Documentation","a linked page on the entered host adds no host note");
    const auto json=nlohmann::json::parse(report_json(report));
    check(json.at("linked_pages").size()==1&&json["linked_pages"][0].at("role")=="documentation","linked page exported");
    check(json["linked_pages"][0].at("first_seen_here").size()==2&&json["linked_pages"][0].at("discovered_from")=="https://site.example:443/","linked page export keeps its source and names");
}
