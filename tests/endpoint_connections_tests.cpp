#include "vector/endpoint_connections.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
namespace {
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
ScanReport sample(const std::string& host="site.example"){ScanReport r;r.target=parse_target("https://"+host+"/");return r;}
void request(ScanReport& r,const std::string& url,long status=200,const std::string& source="",const std::string& collection="network") {
    BrowserRequest q;q.url=url;q.page=source.empty()?r.target.url():source;q.method="GET";q.type="Script";q.status=status;
    q.state=status?"complete":"failed";q.collection=collection;r.browser.requests.push_back(q);identify_browser_vendor(q,r);
}
const EndpointHost& host(const std::vector<EndpointHost>& rows,const std::string& name) {
    auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.host==name;});
    if(found==rows.end())throw std::runtime_error("missing host "+name);return *found;
}
bool absent(const std::vector<EndpointHost>& rows,const std::string& name){return std::none_of(rows.begin(),rows.end(),[&](const auto& r){return r.host==name;});}
bool has(const std::vector<std::string>& values,const std::string& s){return std::find(values.begin(),values.end(),s)!=values.end();}
}
int main(){try {
    auto r=sample();request(r,"https://API.site.example./a?token=PRIVATE#ignored");request(r,"https://api.site.example/b");
    request(r,"https://site.example/");request(r,"https://site.example.evil.test/a");request(r,"https://notsite.example/a");
    request(r,"https://www.site.example/a");request(r,"https://cdn.usefathom.com/script.js");
    request(r,"https://failed.example/a",0);request(r,"https://denied.example/a",403);
    auto rows=endpoint_hosts(r);
    check(rows.size()==7,"exact hostname dedup and target exclusion");
    check(host(rows,"api.site.example").relation=="Subdomain","case, trailing dot and label-boundary subdomain");
    check(host(rows,"api.site.example").evidence.size()==2,"separate paths retain evidence");
    check(host(rows,"site.example.evil.test").relation=="External host","suffix lookalike is external");
    check(host(rows,"notsite.example").relation=="External host","substring is external");
    check(host(rows,"www.site.example").relation=="Subdomain","www redirect can be observed as subdomain");
    check(has(host(rows,"cdn.usefathom.com").providers,"Fathom Analytics"),"path-qualified provider attribution");
    check(has(host(rows,"failed.example").roles,"Request attempt"),"failed request is no response claim");
    check(has(host(rows,"denied.example").roles,"Browser response"),"HTTP error remains a browser response");
    auto exported=nlohmann::json::parse(report_json(r));
    check(exported.at("endpoint_hosts").size()==rows.size(),"projection exported with full report");
    check(exported.at("endpoint_hosts").dump().find("PRIVATE")==std::string::npos,"projection query values redacted");
    r=sample("www.site.example");request(r,"https://site.example/a");request(r,"https://api.site.example/a");
    rows=endpoint_hosts(r);check(rows.size()==1&&rows[0].relation=="Subdomain","exact www prefix has base scope");
    r=sample("shop.example.co.uk");request(r,"https://sibling.example.co.uk/a");request(r,"https://cdn.shop.example.co.uk/a");
    rows=endpoint_hosts(r);check(host(rows,"sibling.example.co.uk").relation=="External host","no guessed registrable parent");
    check(host(rows,"cdn.shop.example.co.uk").relation=="Subdomain","descendants of entered subdomain");
    r=sample("alice.github.io");request(r,"https://bob.github.io/a");rows=endpoint_hosts(r);
    check(rows[0].relation=="External host","shared hosting tenants remain separate");
    r=sample("127.0.0.1");request(r,"http://127.0.0.1:1234/a");request(r,"http://localhost/a");
    rows=endpoint_hosts(r);check(rows.size()==1&&rows[0].relation=="External host","IP has no inferred child domains");
    r=sample();request(r,"https://u:p@api.site.example/a");request(r,"data:text/html,hello");request(r,"https://*.site.example/a");
    check(endpoint_hosts(r).empty(),"credentials, nonweb and wildcard inputs rejected");
    BrowserPage p;p.url="https://trust.vendor.example/";p.purpose="disclosure";r.browser.pages.push_back(p);
    request(r,"https://trust-dependency.example/a",200,p.url);rows=endpoint_hosts(r);
    check(absent(rows,"trust-dependency.example"),"disclosure browser dependencies excluded");
    p.url=r.target.url()+"legal";r.browser.pages.push_back(p);request(r,"https://another-dependency.example/a",200,p.url);
    check(absent(endpoint_hosts(r),"another-dependency.example"),"same-host disclosure rendering also excluded");
    request(r,"https://captured.site.example/a",200,"","captured_http");
    check(has(host(endpoint_hosts(r),"captured.site.example").roles,"Captured document"),"captured HTTP not labeled new response");
    r=sample();HttpObservation h;h.url=r.target.url();h.status=200;h.headers={{"content-type","text/html"}};
    h.body=R"(<a href="https://support.site.example/path?secret=PRIVATE">Support</a><a href="//docs.site.example/">Docs</a>
      <a href="https://other.example/">Unrelated link</a><script src="https://js.stripe.com/v3/"></script>
      <link rel="preconnect" href="https://hint.example"><script>const s='https://literal.example';</script>
      <template><a href="https://inert.site.example/">Example</a></template><!-- <a href="https://comment.site.example/"> -->)";
    r.web.push_back(h);identify_vendors(h,r);rows=endpoint_hosts(r);
    check(has(host(rows,"support.site.example").roles,"Linked subdomain"),"subdomain navigation links identified without probing");
    check(host(rows,"docs.site.example").relation=="Subdomain","protocol-relative links");
    check(has(host(rows,"js.stripe.com").providers,"Stripe"),"static provider reference retained");
    check(absent(rows,"hint.example")&&absent(rows,"literal.example"),"weak hints excluded");
    check(absent(rows,"inert.site.example")&&absent(rows,"comment.site.example"),"inert and comment links excluded");
    check(absent(rows,"other.example"),"ordinary external anchor is not a provider");
    for(const auto* tag:{"xmp","iframe","noembed","noframes","textarea"}) {
        auto raw=sample();auto example=h;
        example.body="<"+std::string(tag)+"><a href='https://fake.site.example/'>example</a></"+tag+"><a href='https://real.site.example/'>real</a>";
        raw.web={example};const auto found=endpoint_hosts(raw);
        check(absent(found,"fake.site.example")&&!absent(found,"real.site.example"),"literal raw-text links excluded without losing later real link");
    }
    {auto raw=sample();auto example=h;example.body="<plaintext><a href='https://fake.site.example/'>example";raw.web={example};check(endpoint_hosts(raw).empty(),"plaintext remainder cannot create links");}
    check(nlohmann::json::parse(report_json(r))["endpoint_hosts"].dump().find("PRIVATE")==std::string::npos,"linked query redaction");
    VendorRelationship published;published.name="Declared Vendor";published.domain="declared.example";published.published_by="site.example";
    published.evidence_level="Page reference";published.relationship="Script";published.evidence={{h.url,"published claim"}};r.vendors.push_back(published);
    check(absent(endpoint_hosts(r),"declared.example"),"published relationship excluded from endpoint hosts");
    r=sample();DnsQuery q;q.name=r.target.host;q.state="complete";q.type="MX";q.source="Fixture resolver";q.checked_at="2026-11-09T00:00:00Z";
    q.records={{q.name,"MX","10 aspmx.l.google.com.",300},{q.name,"MX","0 .",300},{q.name,"MX","not-a-priority bad.example",300}};
    r.dns.queries.push_back(q);q.type="NS";q.records={{q.name,"NS","ada.ns.cloudflare.com.",300}};r.dns.queries.push_back(q);
    q.type="CNAME";q.records={{q.name,"CNAME","cdn.site.example.",60}};r.dns.queries.push_back(q);
    q.type="TXT";q.records={{q.name,"TXT","\"v=spf1 include:_spf.google.com \"\"include:mail.example -all include:ignored.example\"",300},
        {q.name,"TXT","google-site-verification=PRIVATE",300}};r.dns.queries.push_back(q);
    identify_dns(r.dns,r.findings);rows=endpoint_hosts(r);
    check(has(host(rows,"aspmx.l.google.com").providers,"Google Workspace mail"),"exact MX record attribution");
    check(has(host(rows,"ada.ns.cloudflare.com").providers,"Cloudflare DNS"),"exact NS record attribution");
    check(host(rows,"cdn.site.example").relation=="Subdomain","DNS alias child");
    check(has(host(rows,"_spf.google.com").roles,"SPF include reference"),"SPF domain from split TXT text");
    check(host(rows,"mail.example").providers.empty(),"TXT provider name not applied to unrelated include");
    check(absent(rows,"ignored.example")&&absent(rows,"bad.example"),"unreachable SPF and malformed MX excluded");
    check(dns_record_providers(r,r.dns.queries[0].records[0]).size()==1,"DNS row exposes known provider");
    r.dns.queries[0].state="error";check(absent(endpoint_hosts(r),"aspmx.l.google.com"),"failed DNS query cannot supply hosts");
    check(endpoint_hosts(sample()).empty(),"empty reports remain empty");
    r=sample();request(r,"HTTPS://docs.site.example/a");request(r,"https://[2001:db8::8]/a");
    request(r,"https://@outside.example/a");request(r,"https://foo..site.example/a");request(r,"https://outside.example:99999/a");
    rows=endpoint_hosts(r);check(rows.size()==2,"uppercase scheme and IPv6 accepted; malformed authority rejected");
    check(host(rows,"docs.site.example").relation=="Subdomain","uppercase scheme preserves normal classification");
    request(r,"https://xn--bcher-kva.example/a");check(!absent(endpoint_hosts(r),"xn--bcher-kva.example"),"ASCII punycode retained");
    q.type="CNAME";q.state="limited";q.records={{"metrics.site.example","CNAME","tenant.vendor.example.",60}};r.dns.queries={q};
    rows=endpoint_hosts(r);check(host(rows,"metrics.site.example").relation=="Subdomain","DNS alias owner retained separately from provider target");
    check(has(host(rows,"tenant.vendor.example").roles,"CNAME alias"),"partial DNS retains collected alias target");
    std::cout<<checks<<" endpoint connection checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
