#include "vector/model.hpp"
#include "vector/endpoint_connections.hpp"
#include "vector/api_endpoints.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace vectorapp {
std::string report_json(const ScanReport& r) {
    using nlohmann::json;
    json j = {{"schema_version",1},{"application","Vector"},{"status",r.status},{"error",r.error},
        {"target",{{"input",r.target.input},{"host",r.target.host},{"url",r.target.url()}}},
        {"started_at",r.started_at},{"finished_at",r.finished_at},{"duration_ms",r.duration_ms},
        {"address",r.address},{"addresses",r.addresses},{"reverse_dns",r.reverse_dns},{"notes",r.notes},
        {"services",json::array()},{"web",json::array()},{"findings",json::array()},
        {"vendors",json::array()},
        {"vendor_coverage",{{"method","Static and rendered references, browser responses and linked published provider disclosures"},{"truncated",r.vendors_truncated},
            {"markup_limit_bytes",3*1024*1024},{"tag_limit_per_document",8192},{"relationship_limit",256},
            {"vendor_requests_observed",false},{"account_access_verified",false},
            {"limits","Browser contact does not verify account permissions or server-side integrations. Static references and observed responses are labeled separately."}}}};
    const bool rendered=std::any_of(r.browser.pages.begin(),r.browser.pages.end(),[](const auto& p){return p.javascript_enabled;});
    j["endpoint_hosts"]=json::array();
    for(const auto& host:endpoint_hosts(r)) {
        json evidence=json::array();for(const auto& e:host.evidence)evidence.push_back({{"source",e.source},{"value",e.value}});
        j["endpoint_hosts"].push_back({{"host",host.host},{"relation",host.relation},{"providers",host.providers},{"roles",host.roles},{"evidence",evidence}});
    }
    const auto api=api_endpoint_list(r);
    j["api_endpoints"]=json::array();
    for(const auto& endpoint:api.rows) {
        json evidence=json::array();for(const auto& e:endpoint.evidence)evidence.push_back({{"source",e.source},{"value",e.value}});
        j["api_endpoints"].push_back({{"method",endpoint.method},{"url",endpoint.url},{"host",endpoint.host},{"path",endpoint.path},
            {"scope",endpoint.scope},{"provider",endpoint.provider},{"basis",endpoint.basis},{"status",endpoint.status},
            {"meaning",api_endpoint_meaning(endpoint)},{"evidence",evidence}});
    }
    j["linked_pages"]=nlohmann::json::array();
    for(const auto& page:r.linked_pages)
        j["linked_pages"].push_back({{"role",page.role},{"requested_url",page.requested_url},{"url",page.url},{"discovered_from",page.discovered_from},
            {"state",page.state},{"reason",page.reason},{"status",page.status},{"requests",page.requests},{"request_limit",3},{"first_seen_here",page.first_seen}});
    j["api_endpoint_coverage"]={{"method","Requests the inspected page made in the isolated browser, JSON responses to Vector's own reads, API description documents, page markup, collected script text and robots.txt"},
        {"truncated",api.truncated},{"row_limit",ApiEndpointList::row_limit},{"evidence_limit_per_row",ApiEndpointList::evidence_limit},
        {"reference_limit",ApiEndpointList::reference_limit},{"references_per_script_limit",ApiEndpointList::references_per_source},
        {"endpoints_requested_by_vector",false},
        {"limits","No request is sent to a listed endpoint. Observed means the page's own code made the request during the inspection. Described, declared, in-code and robots.txt rows were named by collected documents and were not requested; a code reference can be inactive code or a bundled library's endpoint. Query strings are omitted. Identifier-like path and host parts of observed requests are shown as {id}; {} marks a value filled in at run time."}};
    const auto& tcp=r.tcp_coverage;
    j["proxy"]={{"enabled",r.proxy.enabled},{"endpoint",r.proxy.endpoint},{"authenticated",r.proxy.authenticated},
        {"scope","TCP and web traffic, including browser and HTTPS metadata; ICMP and UDP are skipped when enabled. Remote target addresses are unavailable for proxied hostnames."}};
    j["favicon"]={{"url",r.favicon.url},{"content_type",r.favicon.content_type},{"bytes",r.favicon.body.size()},
        {"retained",!r.favicon.body.empty()},{"note","Raw favicon bytes stay in-process for the report UI; exports only record size."}};
    j["tcp_coverage"]={{"profile",tcp.profile},{"selected_ports",tcp.selected_ports},{"planned",tcp.selected_ports.size()},
        {"completed",tcp.completed},{"max_concurrent",tcp.max_concurrent}};
    const auto& web=r.web_coverage;
    json dns_queries=json::array();
    for(const auto& query:r.dns.queries) {
        json records=json::array();
        for(const auto& record:query.records)records.push_back({{"name",record.name},{"type",record.type},{"value",record.value},{"ttl",record.ttl}});
        dns_queries.push_back({{"name",query.name},{"type",query.type},{"source",query.source},{"checked_at",query.checked_at},
            {"state",query.state},{"error",query.error},{"rcode",query.rcode},{"truncated",query.truncated},{"resolver_authenticated_data",query.authenticated},{"records",records}});
    }
    j["dns"]={{"state",r.dns.state},{"reason",r.dns.reason},{"elapsed_ms",r.dns.elapsed_ms},{"queries",dns_queries},
        {"query_limit",4},{"time_limit_ms",3000},{"body_limit_bytes",65536},{"records_per_query_limit",64},
        {"scope","DNS configuration for the hostname; not service availability or software running on the selected IP."}};
    j["web_coverage"]={{"started",web.started},{"limited",web.limited},{"reasons",web.reasons},{"elapsed_ms",web.elapsed_ms},
        {"page_requests",web.page_requests},{"script_requests",web.script_requests},{"page_limit",WebCoverage::page_limit},
        {"script_limit",WebCoverage::script_limit},{"time_limit_ms",WebCoverage::time_limit_ms},
        {"scope","Shared across every web endpoint; page attempts include redirects and failures; time includes static and browser collection. Shutdown and analysis may add overhead."}};
    const bool contact=std::any_of(r.vendors.begin(),r.vendors.end(),[](const auto& v){return v.evidence_level=="Observed response";});
    j["vendor_coverage"]["vendor_requests_observed"]=contact;
    json browser_pages=json::array(), browser_requests=json::array();
    for(const auto& p:r.browser.pages)browser_pages.push_back({{"url",p.url},{"final_url",p.final_url},{"state",p.state},{"reason",p.reason},
        {"title",p.title},{"status",p.status},{"elapsed_ms",p.elapsed_ms},{"javascript_enabled",p.javascript_enabled},{"dom_truncated",p.dom_truncated},{"scripts_analyzed",p.scripts_analyzed},{"document_source",p.document_source},{"purpose",p.purpose}});
    for(const auto& q:r.browser.requests)browser_requests.push_back({{"url",q.url},{"page",q.page},{"initiator",q.initiator},{"type",q.type},
        {"method",q.method},{"state",q.state},{"error",q.error},{"status",q.status},{"address",q.address},{"mime",q.mime},{"transferred_bytes",q.transferred_bytes},{"collection",q.collection}});
    j["browser"]={{"state",r.browser.state},{"engine",r.browser.engine},{"reason",r.browser.reason},{"pages",browser_pages},{"requests",browser_requests},
        {"requests_truncated",r.browser.requests_truncated},{"profile","isolated temporary profile"},
        {"limits","Entry page per web endpoint; a rejected navigation can execute a collected complete HTTP 200 document, labeled captured_http. At most eight browser attempts within the shared web deadline. No clicks, scrolling or login. Up to 256 requests, 160 script bodies of 2 MiB, eight loaded CSS bodies of 512 KiB and 32 MiB transferred per attempt. Query values, request headers, cookies and request bodies omitted. Later loads may be missing."}};
    json contacts = json::array();
    for (const auto& c : r.registry.contacts) contacts.push_back({{"role",c.role},{"name",c.name},{"email",c.email},{"phone",c.phone}});
    j["registry"] = {{"scope",r.registry.scope},{"source",r.registry.source},{"handle",r.registry.handle},{"name",r.registry.name},
        {"type",r.registry.type},{"country",r.registry.country},{"cidr",r.registry.cidr},{"range",r.registry.range},{"asn",r.registry.asn},
        {"status",r.registry.status},{"org",r.registry.org},{"abuse_email",r.registry.abuse_email},{"description",r.registry.description},
        {"error",r.registry.error},{"contacts",contacts}};
    const auto& g=r.geolocation;
    j["geolocation"]={{"state",g.state},{"source",g.source},{"checked_at",g.checked_at},{"address",g.address},
        {"city",g.city},{"region",g.region},{"country",g.country},{"country_code",g.country_code},{"timezone",g.timezone},
        {"isp",g.isp},{"asn",g.asn},{"error",g.error},{"precision","Approximate IP location; not a device or street location."}};
    const auto reach=summarize_reachability(r);
    j["reachability"]={{"state",reach.state},{"summary",reach.summary},{"tcp_open",reach.tcp_open},{"tcp_refused",reach.tcp_refused},
        {"tcp_no_response",reach.tcp_no_response},{"tcp_other",reach.tcp_other},{"udp_replies",reach.udp_replies},
        {"icmp",{{"state",r.icmp.state},{"address",r.icmp.address},{"reply_address",r.icmp.reply_address},{"detail",r.icmp.detail},
        {"round_trip_ms",r.icmp.round_trip_ms},{"status_code",r.icmp.status_code},{"ttl",r.icmp.ttl}}}};
    for (const auto& s : r.services) j["services"].push_back({{"port",s.port},{"transport",s.protocol},{"state",s.state},{"identified_service",s.name},
        {"conventional_service",s.expected},{"banner",printable(s.banner,4096)},{"latency_ms",s.latency_ms},{"active_probe",s.active_probe},{"error",s.error}});
    for (const auto& h : r.web) {
        json headers = json::array(), cert = json::array();
        for (const auto& [name,value] : h.headers) headers.push_back({{"name",name},{"value",value}});
        for (const auto& [name,value] : h.certificate) cert.push_back({{"name",name},{"value",value}});
        j["web"].push_back({{"url",h.url},{"address",h.address},{"status",h.status},{"title",h.title},{"http_version",h.http_version},
            {"elapsed_ms",h.elapsed_ms},{"error",h.error},{"redirect",h.redirect},{"body_bytes",h.body.size()},
            {"body_truncated",h.body_truncated},{"tls_verified",h.tls_verified},{"headers",headers},{"certificate",cert}});
    }
    for (const auto& f : r.findings) {
        json evidence = json::array();
        for (const auto& e : f.evidence) evidence.push_back({{"source",e.source},{"value",e.value}});
        j["findings"].push_back({{"name",f.name},{"category",f.category},{"version",f.version},{"confidence",f.confidence},{"evidence",evidence}});
    }
    j["scripts"] = json::array();
    j["stylesheets"]=json::array();
    for(const auto& s:r.stylesheets)j["stylesheets"].push_back({{"url",s.url},{"referenced_by",s.referenced_by},
        {"state",s.state},{"reason",s.reason},{"status",s.status},{"body_bytes",s.body_bytes},
        {"analyzed",s.analyzed},{"body_truncated",s.body_truncated},{"collection",s.collection}});
    j["stylesheet_coverage"]={{"request_limit",8},{"reference_limit",128},{"body_limit_bytes",512*1024},
        {"browser_response_limit_per_page",8},
        {"scope","Static: eight same-host declared stylesheets, no redirects or imports, up to 128 references. Browser: eight already-loaded text/css responses per page, including dependency hosts. Both analyze up to 512 KiB per body within the shared 30-second web deadline."}};
    j["web_coverage"]["page_body_limit_bytes"]=1024*1024;
    for (const auto& s : r.scripts) j["scripts"].push_back({{"url",s.url},{"referenced_by",s.referenced_by},
        {"state",s.state},{"reason",s.reason},{"status",s.status},{"body_bytes",s.body_bytes},
        {"analyzed",s.analyzed},{"body_truncated",s.body_truncated},{"collection",s.collection}});
    j["script_coverage"] = {{"method","static markup plus isolated browser execution"},{"javascript_executed",rendered},
        {"references_truncated",r.script_references_truncated},
        {"limits","Static: 40 declared scripts per inspection, 512 KiB each: the page host's own first, then other hosts in the page's address scope (a public page reads public hosts only; a local page causes no outside request). Browser: up to 160 loaded scripts per page, 2 MiB each, including external hosts and dynamic imports. All web collection shares a 30-second time budget. Interaction-gated loads and private server dependencies are not inventoried."}};
    for (const auto& v : r.vendors) {
        json evidence=json::array();
        for(const auto& e:v.evidence)evidence.push_back({{"source",e.source},{"value",e.value}});
        j["vendors"].push_back({{"name",v.name},{"category",v.category},{"domain",v.domain},
            {"relationship",v.relationship},{"evidence_level",v.evidence_level},{"provider_confidence",v.confidence},
            {"potential_access",v.capability},{"access_status","unverified"},{"attribution_source",v.attribution_source},
            {"published_by",v.published_by},{"recorded_at",v.recorded_at},{"evidence",evidence}});
    }
    j["disclosures"]=json::array();
    for(const auto& d:r.disclosures) {
        json chain=json::array();for(const auto& e:d.discovery_chain)chain.push_back({{"source",e.source},{"value",e.value}});
        j["disclosures"].push_back({{"url",d.url},{"title",d.title},{"state",d.state},{"reason",d.reason},
            {"checked_at",d.checked_at},{"http_status",d.status},{"providers",d.providers},{"requested_url",d.requested_url},
            {"discovered_from",d.discovered_from},{"discovery_label",d.discovery_label},{"discovery_depth",d.depth},{"discovery_chain",chain}});
    }
    j["activity"] = json::array();
    for (const auto& e : r.activity) j["activity"].push_back({{"stage",stage_key(e.stage)},{"state",lower(activity_state_name(e.state))},
        {"elapsed_ms",e.elapsed_ms},{"item",e.item},{"detail",e.detail},{"completed",e.completed},{"total",e.total}});
    return j.dump(2,' ',false,json::error_handler_t::replace);
}
ScanReport demo_report() {
    ScanReport r;
    r.target = parse_target("http://192.0.2.42");
    r.address = "192.0.2.42"; r.addresses = {r.address};
    r.started_at = r.finished_at = utc_now(); r.status = "demo"; r.duration_ms = 1240;
    r.notes = {"Illustrative data. This example did not contact a network target.","Device and OS identities are hints from advertised data, not hardware verification."};
    r.services = {{22,"open","tcp","Unknown","SSH","SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13",{},14.2},
        {80,"open","tcp","HTTP","HTTP",{},{},12.4},{443,"closed","tcp","Unknown","HTTPS",{},{},11.8},
        {445,"no response","tcp","Unknown","SMB",{},"Connection timed out.",0}};
    HttpObservation h;
    h.url = "http://192.0.2.42:80/"; h.address = r.address; h.status = 200; h.http_version = "HTTP/1.1";
    h.title = "Lab inventory";
    h.headers = {{"server","nginx/1.26.2"},{"content-type","text/html"},{"x-powered-by","PHP/8.3.12"}};
    h.headers.emplace_back("content-security-policy-report-only","connect-src 'self' https://*.clarity.ms");
    h.body = R"(<meta name="generator" content="WordPress 6.6.2"><script src="/wp-includes/js/jquery/jquery-3.7.1.min.js"></script>
        <script src="https://js.stripe.com/v3/"></script>
        <script src="https://www.googletagmanager.com/gtag/js?id=EXAMPLE"></script>
        <script>const widget = 'https://widget.intercom.io/widget/example';</script>
        <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Example">
        <form action="https://forms.example.net/contact"></form>)";
    r.web.push_back(h);
    for (auto& s : r.services) identify_banner(s,r.findings);
    identify_http(h,r.findings);
    identify_vendors(h,r);
    const std::string demo_page="http://192.0.2.42/",demo_script="Script http://192.0.2.42/wp-content/themes/example/app.js";
    r.api_references={
        {"markup",{},demo_page+"wp-json/",demo_page,"link rel=\"https://api.w.org/\" (WordPress REST API root)"},
        {"markup",{},demo_page+"xmlrpc.php",demo_page,"link rel=\"EditURI\" (XML-RPC discovery document)"},
        {"robots",{},demo_page+"wp-json/wp/v2/users",demo_page+"robots.txt","Disallow: /wp-json/wp/v2/users"},
        {"code","GET","/wp-json/wp/v2/posts",demo_script,"fetch() argument: \"/wp-json/wp/v2/posts\" (query omitted)"},
        {"code","GET","/wp-json/wp/v2/comments",demo_script,"fetch() argument: \"/wp-json/wp/v2/comments\""},
        {"code","POST","/wp-admin/admin-ajax.php",demo_script,"XMLHttpRequest.open() argument: \"/wp-admin/admin-ajax.php\""},
        {"code","POST","/wp-json/contact-form-7/v1/contact-forms/{}/feedback",demo_script,".post() argument: \"/wp-json/contact-form-7/v1/contact-forms/{}/feedback\""}};
    r.registry.scope = "public";
    r.registry.source = "IANA";
    r.registry.name = "TEST-NET-1";
    r.registry.org = "Internet Assigned Numbers Authority";
    r.registry.country = "US";
    r.registry.cidr = "192.0.2.0/24";
    r.registry.abuse_email = "abuse@example.net";
    r.registry.description = "Illustrative registry row. This example did not query RDAP.";
    r.registry.contacts.push_back({"abuse", "IANA", "abuse@example.net", {}});
    r.geolocation.state="available";r.geolocation.address=r.address;r.geolocation.source="Illustrative data";
    r.geolocation.checked_at=r.finished_at;r.geolocation.city="Example City";r.geolocation.country="Example Country";
    r.icmp.state="reply";r.icmp.address=r.address;r.icmp.reply_address=r.address;r.icmp.round_trip_ms=12;
    r.icmp.detail="Illustrative echo reply. No packet was sent.";
    r.dns.state="complete";r.dns.reason="Illustrative DNS records. No resolver request was sent.";
    for(const auto& type:{"CNAME","MX","NS","TXT"}) {
        DnsQuery q;q.name="example.test";q.type=type;q.source="Illustrative data";q.checked_at=r.finished_at;q.state="complete";q.rcode=0;
        if(q.type=="CNAME")q.records.push_back({q.name,q.type,"fixture.cloudfront.net.",300});
        if(q.type=="MX")q.records.push_back({q.name,q.type,"0 fixture.mail.protection.outlook.com.",3600});
        if(q.type=="NS")q.records.push_back({q.name,q.type,"ada.ns.cloudflare.com.",86400});
        if(q.type=="TXT")q.records.push_back({q.name,q.type,"\"v=spf1 include:spf.protection.outlook.com -all\"",3600});
        if(q.type=="MX")q.records.push_back({q.name,q.type,"10 backup.example.test.",3600});
        if(q.type=="TXT")q.records.push_back({q.name,q.type,"\"verification=illustrative-only\"",3600});
        r.dns.queries.push_back(std::move(q));
    }
    identify_dns(r.dns,r.findings);
    return r;
}
}
