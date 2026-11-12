#include "vector/model.hpp"
#include "vector/overview.hpp"
#include "../src/dns.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace vectorapp;
using nlohmann::json;
namespace {
int checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
json reply(const std::string& name,int type,const std::string& data) {
    return {{"Status",0},{"Question",json::array({{{"name",name+"."},{"type",type}}})},
        {"Answer",json::array({{{"name",name+"."},{"type",type},{"TTL",300},{"data",data}}})}};
}
std::vector<Finding> match(const json& value,const std::string& type,const std::string& name="site.example") {
    DnsObservation dns;dns.state="complete";auto q=parse_dns_json(value.dump(),name,type);
    q.source="fixture resolver";q.checked_at="2026-01-01T00:00:00Z";dns.queries.push_back(q);
    std::vector<Finding> findings;identify_dns(dns,findings);return findings;
}
}
int main(int argc,char** argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    try {
        std::atomic_bool cancel{false};
        if(argc>=4&&std::string(argv[1])=="--fixture") {
            const auto endpoint=parse_target(argv[2]);
            if(endpoint.scheme!="http"||(endpoint.host!="127.0.0.1"&&endpoint.host!="::1"))throw std::runtime_error("Fixture resolver must be loopback HTTP");
            const int cancel_ms=argc>4?std::stoi(argv[4]):0,timeout=argc>5?std::stoi(argv[5]):3000;
            std::jthread stopper([&](std::stop_token stop){const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(cancel_ms);
                while(cancel_ms>0&&!stop.stop_requested()&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(5));
                if(cancel_ms>0&&!stop.stop_requested())cancel=true;});
            ScanReport report;report.target=parse_target(argv[3]);report.address="192.0.2.1";
            report.dns=collect_dns_records(report.target.host,cancel,argv[2],timeout);identify_dns(report.dns,report.findings);
            std::cout<<report_json(report)<<'\n';return 0;
        }
        struct RuleCase{const char* type;int code;const char* data;const char* product;const char* category;};
        const RuleCase cases[]={
            {"CNAME",5,"d123.cloudfront.net.","Amazon CloudFront","DNS hosting alias"},
            {"CNAME",5,"cname.vercel-dns-0.com.","Vercel","DNS hosting alias"},
            {"CNAME",5,"cname.vercel-dns.com.","Vercel","DNS hosting alias"},
            {"CNAME",5,"owner.github.io.","GitHub Pages","DNS hosting alias"},
            {"CNAME",5,"app.azurewebsites.net.","Azure App Service","DNS hosting alias"},
            {"NS",2,"ada.ns.cloudflare.com.","Cloudflare DNS","Authoritative DNS"},
            {"NS",2,"ns1-37.azure-dns.com.","Azure DNS","Authoritative DNS"},
            {"NS",2,"ns2-37.azure-dns.net.","Azure DNS","Authoritative DNS"},
            {"NS",2,"ns3-37.azure-dns.org.","Azure DNS","Authoritative DNS"},
            {"NS",2,"ns4-37.azure-dns.info.","Azure DNS","Authoritative DNS"},
            {"MX",15,"1 smtp.google.com.","Google Workspace mail","Inbound email"},
            {"MX",15,"1 aspmx.l.google.com.","Google Workspace mail","Inbound email"},
            {"MX",15,"5 alt1.aspmx.l.google.com.","Google Workspace mail","Inbound email"},
            {"MX",15,"0 tenant.mail.protection.outlook.com.","Microsoft 365 mail","Inbound email"},
            {"TXT",16,"\"v=spf1 include:_spf.google.com ~all\"","Google mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.protection.outlook.com -all\"","Microsoft 365 mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:amazonses.com ~all\"","Amazon SES mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:sendgrid.net -all\"","SendGrid mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:mailgun.org -all\"","Mailgun mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:mail.zendesk.com -all\"","Zendesk mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:_spf.salesforce.com -all\"","Salesforce mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:mktomail.com -all\"","Marketo mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:servers.mcsv.net -all\"","Mailchimp mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.mandrillapp.com -all\"","Mailchimp Transactional mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.mtasv.net -all\"","Postmark mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:mg-spf.greenhouse.io -all\"","Greenhouse mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.mailjet.com -all\"","Mailjet mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:_spf.atlassian.net -all\"","Atlassian mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:12345.spf07.hubspotemail.net -all\"","HubSpot mail authorization","Email authorization"},
            {"CNAME",5,"wildcard.site.example.edgekey.net.","Akamai","DNS hosting alias"},
            {"CNAME",5,"site.example.edgesuite.net.","Akamai","DNS hosting alias"},
            {"CNAME",5,"e12345.b.akamaiedge.net.","Akamai","DNS hosting alias"},
            {"CNAME",5,"site.akamaized.net.","Akamai","DNS hosting alias"},
            {"CNAME",5,"a1234.g.akamai.net.","Akamai","DNS hosting alias"},
            {"CNAME",5,"site.map.fastly.net.","Fastly","DNS hosting alias"},
            {"CNAME",5,"dualstack.p.sni.global.fastly.net.","Fastly","DNS hosting alias"},
            {"CNAME",5,"site.netlify.app.","Netlify","DNS hosting alias"},
            {"CNAME",5,"apex-loadbalancer.netlify.com.","Netlify","DNS hosting alias"},
            {"CNAME",5,"project.pages.dev.","Cloudflare Pages","DNS hosting alias"},
            {"CNAME",5,"site.example.cdn.cloudflare.net.","Cloudflare","DNS hosting alias"},
            {"CNAME",5,"endpoint-abc.z01.azurefd.net.","Azure Front Door","DNS hosting alias"},
            {"CNAME",5,"site.azureedge.net.","Azure CDN","DNS hosting alias"},
            {"CNAME",5,"site.trafficmanager.net.","Azure Traffic Manager","DNS hosting alias"},
            {"CNAME",5,"calm-sea-01.5.azurestaticapps.net.","Azure Static Web Apps","DNS hosting alias"},
            {"CNAME",5,"web-123.us-east-1.elb.amazonaws.com.","AWS Elastic Load Balancing","DNS hosting alias"},
            {"CNAME",5,"a1b2c3.awsglobalaccelerator.com.","AWS Global Accelerator","DNS hosting alias"},
            {"CNAME",5,"env.eba-x.us-east-1.elasticbeanstalk.com.","AWS Elastic Beanstalk","DNS hosting alias"},
            {"CNAME",5,"main.d111.amplifyapp.com.","AWS Amplify","DNS hosting alias"},
            {"CNAME",5,"bucket.s3-website-us-east-1.amazonaws.com.","Amazon S3 website","DNS hosting alias"},
            {"CNAME",5,"bucket.s3-website.ap-south-1.amazonaws.com.","Amazon S3 website","DNS hosting alias"},
            {"CNAME",5,"quiet-otter-abc.herokudns.com.","Heroku","DNS hosting alias"},
            {"CNAME",5,"app.herokuapp.com.","Heroku","DNS hosting alias"},
            {"CNAME",5,"shops.myshopify.com.","Shopify","DNS hosting alias"},
            {"CNAME",5,"proxy-ssl.webflow.com.","Webflow","DNS hosting alias"},
            {"CNAME",5,"site.webflow.io.","Webflow","DNS hosting alias"},
            {"CNAME",5,"ext-cust.squarespace.com.","Squarespace","DNS hosting alias"},
            {"CNAME",5,"site.wpengine.com.","WP Engine","DNS hosting alias"},
            {"CNAME",5,"site.wpenginepowered.com.","WP Engine","DNS hosting alias"},
            {"CNAME",5,"site.kinsta.cloud.","Kinsta","DNS hosting alias"},
            {"CNAME",5,"live-site.pantheonsite.io.","Pantheon","DNS hosting alias"},
            {"CNAME",5,"app.fly.dev.","Fly.io","DNS hosting alias"},
            {"CNAME",5,"app.onrender.com.","Render","DNS hosting alias"},
            {"CNAME",5,"project.web.app.","Firebase Hosting","DNS hosting alias"},
            {"CNAME",5,"project.firebaseapp.com.","Firebase Hosting","DNS hosting alias"},
            {"CNAME",5,"abc.x.incapdns.net.","Imperva","DNS hosting alias"},
            {"CNAME",5,"zone.b-cdn.net.","Bunny CDN","DNS hosting alias"},
            {"CNAME",5,"zone-1a2b.kxcdn.com.","KeyCDN","DNS hosting alias"},
            {"CNAME",5,"123456.rsc.cdn77.org.","CDN77","DNS hosting alias"},
            {"CNAME",5,"cdn1.wixdns.net.","Wix","DNS hosting alias"},
            {"CNAME",5,"site.ghost.io.","Ghost(Pro)","DNS hosting alias"},
            {"CNAME",5,"123.group1.sites.hubspot.net.","HubSpot CMS","DNS hosting alias"},
            {"CNAME",5,"site.hs-sites.com.","HubSpot CMS","DNS hosting alias"},
            {"CNAME",5,"company.zendesk.com.","Zendesk","DNS hosting alias"},
            {"NS",2,"ns-815.awsdns-37.net.","Amazon Route 53","Authoritative DNS"},
            {"NS",2,"ns-1516.awsdns-61.org.","Amazon Route 53","Authoritative DNS"},
            {"NS",2,"ns-1675.awsdns-17.co.uk.","Amazon Route 53","Authoritative DNS"},
            {"NS",2,"ns-407.awsdns-50.com.","Amazon Route 53","Authoritative DNS"},
            {"NS",2,"ns-cloud-a1.googledomains.com.","Google Cloud DNS","Authoritative DNS"},
            {"NS",2,"dns1.p01.nsone.net.","NS1","Authoritative DNS"},
            {"NS",2,"ns1.dnsimple.com.","DNSimple","Authoritative DNS"},
            {"NS",2,"ns2.dnsimple-edge.net.","DNSimple","Authoritative DNS"},
            {"NS",2,"ns01.domaincontrol.com.","GoDaddy DNS","Authoritative DNS"},
            {"NS",2,"a1-23.akam.net.","Akamai Edge DNS","Authoritative DNS"},
            {"NS",2,"pdns1.ultradns.net.","UltraDNS","Authoritative DNS"},
            {"NS",2,"dns1.registrar-servers.com.","Namecheap DNS","Authoritative DNS"},
            {"NS",2,"ns1.vercel-dns.com.","Vercel DNS","Authoritative DNS"},
            {"NS",2,"ns1.p01.dynect.net.","Oracle Dyn DNS","Authoritative DNS"},
            {"NS",2,"ns10.dnsmadeeasy.com.","DNS Made Easy","Authoritative DNS"},
            {"NS",2,"dns1.p01.squarespacedns.com.","Squarespace DNS","Authoritative DNS"},
            {"NS",2,"ns0.wixdns.net.","Wix DNS","Authoritative DNS"},
            {"NS",2,"hydrogen.ns.hetzner.com.","Hetzner DNS","Authoritative DNS"},
            {"NS",2,"ns-12-a.gandi.net.","Gandi DNS","Authoritative DNS"},
            {"NS",2,"ns1.alidns.com.","Alibaba Cloud DNS","Authoritative DNS"},
            {"NS",2,"f1g1ns1.dnspod.net.","DNSPod","Authoritative DNS"},
            {"NS",2,"ns1.digitalocean.com.","DigitalOcean DNS","Authoritative DNS"},
            {"NS",2,"ns1.linode.com.","Linode DNS","Authoritative DNS"},
            {"MX",15,"10 mxa.mailgun.org.","Mailgun mail","Inbound email"},
            {"MX",15,"10 mxb.eu.mailgun.org.","Mailgun mail","Inbound email"},
            {"MX",15,"10 mail.protonmail.ch.","Proton Mail","Inbound email"},
            {"MX",15,"10 mx.yandex.net.","Yandex Mail","Inbound email"},
            {"MX",15,"10 mx.sendgrid.net.","SendGrid Inbound Parse","Inbound email"},
            {"MX",15,"10 inbound-smtp.us-west-2.amazonaws.com.","Amazon SES or WorkMail mail","Inbound email"},
            {"MX",15,"10 mx.zoho.com.","Zoho Mail","Inbound email"},
            {"MX",15,"20 mx2.zoho.eu.","Zoho Mail","Inbound email"},
            {"MX",15,"10 mx0a-00123.pphosted.com.","Proofpoint","Inbound email"},
            {"MX",15,"10 mx1-us1.ppe-hosted.com.","Proofpoint Essentials","Inbound email"},
            {"MX",15,"10 us-smtp-inbound-1.mimecast.com.","Mimecast","Inbound email"},
            {"MX",15,"10 cluster1.us.messagelabs.com.","Symantec Email Security","Inbound email"},
            {"MX",15,"10 d123.ess.barracudanetworks.com.","Barracuda Email Security","Inbound email"},
            {"MX",15,"10 mx1.hc123-45.iphmx.com.","Cisco Secure Email","Inbound email"},
            {"MX",15,"10 in1-smtp.messagingengine.com.","Fastmail","Inbound email"},
            {"MX",15,"10 mx01.mail.icloud.com.","iCloud Mail","Inbound email"},
            {"MX",15,"10 route1.mx.cloudflare.net.","Cloudflare Email Routing","Inbound email"},
            {"MX",15,"10 mx1.emailsrvr.com.","Rackspace Email","Inbound email"},
            {"MX",15,"0 smtp.secureserver.net.","GoDaddy email","Inbound email"},
            {"MX",15,"10 mx1.titan.email.","Titan Email","Inbound email"},
            {"MX",15,"10 mx1.privateemail.com.","Namecheap Private Email","Inbound email"},
            {"TXT",16,"\"v=spf1 include:aspmx.pardot.com -all\"","Pardot mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:cust-spf.exacttarget.com -all\"","Salesforce Marketing Cloud mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.brevo.com -all\"","Brevo mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.sendinblue.com -all\"","Brevo mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf.messagingengine.com -all\"","Fastmail mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:zoho.com -all\"","Zoho mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:zohomail.com -all\"","Zoho mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:_spf.protonmail.ch -all\"","Proton Mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:helpscoutemail.com -all\"","Help Scout mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:email.freshdesk.com -all\"","Freshdesk mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:shops.shopify.com -all\"","Shopify mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:_spf.createsend.com -all\"","Campaign Monitor mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:_spf.firebasemail.com -all\"","Firebase mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:us._netblocks.mimecast.com -all\"","Mimecast mail authorization","Email authorization"},
            {"TXT",16,"\"v=spf1 include:spf-00123.pphosted.com -all\"","Proofpoint mail authorization","Email authorization"}};
        for(const auto& c:cases) {
            const auto good=reply("site.example",c.code,c.data);const auto findings=match(good,c.type);
            check(findings.size()==1&&findings[0].name==c.product&&findings[0].category==c.category,"DNS provider role positively identified");
            check(findings[0].evidence.size()>=5&&findings[0].evidence[0].value==c.data,"exact bytes, resolver, date and TTL retained");
            auto bad=good;bad["Answer"][0]["name"]="unrelated.example.";check(match(bad,c.type).empty(),"unrelated owner never authorizes a finding");
            bad=good;bad["Question"][0]["name"]="other.example.";check(match(bad,c.type).empty(),"question mismatch rejected");
        }
        for(const auto* host:{"evilcloudfront.net","d1.cloudfront.net.attacker.test","github.io.attacker.test","azurewebsites.net.attacker.test","vercel-dns.com.attacker.test"})
            check(match(reply("site.example",5,host),"CNAME").empty(),"misleading hostname boundaries rejected");
        for(const auto* host:{"notedgekey.net","edgekey.net","site.edgekey.net.attacker.test","evilfastly.net","pages.dev","bucket.s3.us-east-1.amazonaws.com",
            "s3-website-us-east-1.amazonaws.com.attacker.test","myelb.amazonaws.com","notherokuapp.com","web.app"})
            check(match(reply("site.example",5,host),"CNAME").empty(),"provider zone itself, lookalike and unrelated AWS hosts are not hosting aliases");
        for(const auto* host:{"ns-815.awsdns-37.attacker.test","ns-815.awsdns-37.io","ns-abc.awsdns-37.net","ns-815.awsdns-3.net","ns-815.awsdns-377.net","xns-815.awsdns-37.net",
            "ns-815.notawsdns-37.net","ns-12345.awsdns-37.net","ns-cloud-a1.googledomains.com.attacker.test","other.googledomains.com","evilnsone.net","akam.net"})
            check(match(reply("site.example",2,host),"NS").empty(),"malformed or lookalike nameserver names are not providers");
        for(const auto* value:{"10 mxc.mailgun.org.","10 mxa.mailgun.org.attacker.test.","10 smtp.mailgun.org.","10 evilpphosted.com.","10 mail.zoho.com.","10 mx.zoho.com.attacker.test.",
            "10 inbound-smtp.us-west-2.amazonaws.com.attacker.test.","10 smtp.us-west-2.amazonaws.com.","10 protonmail.ch.","10 mx.yandex.net.attacker.test."})
            check(match(reply("site.example",15,value),"MX").empty(),"lookalike or undocumented mail hosts are not providers");
        for(const auto* value:{"\"v=spf1 include:aspmx.pardot.com.attacker.test -all\"","\"v=spf1 include:notaspmx.pardot.com -all\"","\"v=spf1 -include:aspmx.pardot.com -all\"",
            "\"v=spf1 a:aspmx.pardot.com -all\"","\"v=spf1 include:mail.zoho.com -all\"","\"v=spf1 include:evilpphosted.com -all\"","\"v=spf1 include:pphosted.com.attacker.test -all\"",
            "\"mentions include:spf.brevo.com\""})
            check(match(reply("site.example",16,value),"TXT").empty(),"lookalike, negated and mentioned sender hosts are not authorization");
        auto bybit_like=reply("site.example",16,"\"v=spf1 include:_spf.google.com include:_spf.salesforce.com include:aspmx.pardot.com -all\"");
        check(match(bybit_like,"TXT").size()==3,"each documented include in one policy is reported once");
        for(const auto* value:{"\"google-site-verification=token\"","\"MS=ms1234\"","\"v=spf1 -include:_spf.google.com -all\"",
            "\"v=spf1 include:_spf.google.com.attacker.test -all\"","\"v=spf1 -all include:_spf.google.com\"","\"mentions include:_spf.google.com\"",
            "\"v=spf1 include:amazonses.com.attacker.test -all\"","\"v=spf1 include:notamazonses.com -all\"","\"v=spf1 include:mail.amazonses.com -all\"",
            "\"v=spf1 -include:amazonses.com -all\"","\"v=spf1 include:hubspotemail.net.attacker.test -all\"","\"v=spf1 include:nothubspotemail.net -all\"",
            "\"v=spf1 a:amazonses.com exists:sendgrid.net -all\"","\"mentions include:sendgrid.net\""})
            check(match(reply("site.example",16,value),"TXT").empty(),"TXT mentions, verification and negative SPF are not active sender authorization");
        auto split=reply("site.example",16,"\"v=spf1 include:_spf.\" \"google.com -all\"");
        check(match(split,"TXT").size()==1,"DNS TXT segments concatenate before SPF parsing");
        auto duplicate_spf=split;duplicate_spf["Answer"].push_back({{"name","site.example."},{"type",16},{"TTL",300},{"data","\"v=spf1 include:spf.protection.outlook.com -all\""}});
        check(match(duplicate_spf,"TXT").empty(),"multiple SPF policies do not imply sending authorization");
        auto chained=reply("site.example",5,"alias.example.");
        chained["Answer"].push_back({{"name","alias.example."},{"type",5},{"TTL",40},{"data","d123.cloudfront.net."}});
        check(match(chained,"CNAME").size()==1,"CNAME chain remains attached to the requested owner");
        auto deep=reply("site.example",5,"hop0.example.");
        for(int i=0;i<8;++i)deep["Answer"].push_back({{"name","hop"+std::to_string(i)+".example."},{"type",5},{"TTL",40},
            {"data",i==7?"d123.cloudfront.net.":"hop"+std::to_string(i+1)+".example."}});
        check(match(deep,"CNAME").empty()&&parse_dns_json(deep.dump(),"site.example","CNAME").truncated,"CNAME chain is bounded to eight aliases");
        auto failed=chained;failed["Status"]=2;check(match(failed,"CNAME").empty(),"SERVFAIL does not use attached answers");
        failed["Status"]=3;check(parse_dns_json(failed.dump(),"site.example","CNAME").state=="nxdomain","NXDOMAIN explicit");
        failed=chained;failed["TC"]=true;check(match(failed,"CNAME").empty(),"truncated resolver answer does not infer products");
        failed=chained;failed["TC"]="true";check(match(failed,"CNAME").empty(),"wrong JSON field type rejected");
        failed=chained;failed["Answer"]=json::array();check(parse_dns_json(failed.dump(),"site.example","CNAME").state=="no_records","empty answers explicit");
        check(parse_dns_json("not json","site.example","MX").state=="error","invalid JSON handled");
        check(parse_dns_json(std::string(65537,' '),"site.example","MX").state=="error","body cap enforced by parser");
        check(match(reply("site.example",15,"0 ."),"MX").empty(),"null MX does not invent a provider");
        check(match(reply("site.example",15,"garbage smtp.google.com"),"MX").empty(),"invalid MX preference rejected");
        auto uppercase=reply("site.example",2,"ADA.NS.CLOUDFLARE.COM.");check(match(uppercase,"NS").size()==1,"DNS names are case insensitive");
        auto limited=reply("site.example",2,"ada.ns.cloudflare.com.");
        for(int i=0;i<100;++i)limited["Answer"].push_back({{"name","site.example."},{"type",2},{"TTL",300},{"data","ns"+std::to_string(i)+".example."}});
        auto q=parse_dns_json(limited.dump(),"site.example","NS");check(q.truncated&&q.records.size()==64,"record cap retains marked partial results");
        const auto ip=parse_target("192.0.2.1");check(inspect_dns(ip,ip.host,cancel).state=="skipped","IP inputs never trigger company domain guesses");
        check(inspect_dns(parse_target("localhost"),"127.0.0.1",cancel).state=="skipped","local fixture does not call external resolver");
        check(inspect_dns(parse_target("router.local"),"8.8.8.8",cancel).state=="skipped","local names not submitted");
        cancel=true;check(collect_dns_records("site.example",cancel).state=="cancelled","pre-cancelled collection sends no request");cancel=false;
        check(collect_dns_records("site.example",cancel,"https://attacker.invalid/query").state=="error","arbitrary resolver substitution rejected");
        ScanReport r;r.dns.state="complete";r.dns.queries.push_back(parse_dns_json(chained.dump(),"site.example","CNAME"));identify_dns(r.dns,r.findings);
        const auto exported=json::parse(report_json(r));check(exported["dns"]["queries"][0]["records"][1]["ttl"]==40,"DNS record TTL exported");
        const auto overview=infrastructure_entries(r);check(overview.size()==1&&overview[0].detail=="DNS hosting alias","native overview receives qualified DNS role");
        check(r.services.empty(),"DNS results do not invent endpoint service responses");
        std::cout<<checks<<" DNS evidence checks passed.\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
    curl_global_cleanup();
}
