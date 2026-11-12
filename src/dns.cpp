#include "proxy_transport.hpp"
#include "dns.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <memory>
#include <set>
#include <sstream>

namespace vectorapp {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t body_limit=64*1024, record_limit=64;
std::string dns_name(std::string value) {
    value=lower(trim(std::move(value)));
    if(!value.empty()&&value.back()=='.')value.pop_back();
    if(value.empty()||value.size()>253)return {};
    size_t label=0;
    for(unsigned char c:value) {
        if(c=='.'){if(!label||label>63)return {};label=0;}
        else if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')++label;
        else return {};
    }
    return label&&label<=63?value:std::string{};
}
bool below(const std::string& name,const std::string& suffix) {
    return name.size()>suffix.size()+1&&name.ends_with("."+suffix);
}
struct Zone {const char* suffix;const char* product;};
const Zone alias_zones[]={
    {"cloudfront.net","Amazon CloudFront"},{"vercel-dns.com","Vercel"},{"vercel-dns-0.com","Vercel"},
    {"github.io","GitHub Pages"},{"azurewebsites.net","Azure App Service"},
    {"edgekey.net","Akamai"},{"edgesuite.net","Akamai"},{"akamaiedge.net","Akamai"},{"akamaized.net","Akamai"},{"akamai.net","Akamai"},
    {"fastly.net","Fastly"},{"fastlylb.net","Fastly"},
    {"netlify.app","Netlify"},{"netlify.com","Netlify"},{"netlifyglobalcdn.com","Netlify"},
    {"pages.dev","Cloudflare Pages"},{"cdn.cloudflare.net","Cloudflare"},
    {"azurefd.net","Azure Front Door"},{"azureedge.net","Azure CDN"},{"trafficmanager.net","Azure Traffic Manager"},{"azurestaticapps.net","Azure Static Web Apps"},
    {"elb.amazonaws.com","AWS Elastic Load Balancing"},{"awsglobalaccelerator.com","AWS Global Accelerator"},
    {"elasticbeanstalk.com","AWS Elastic Beanstalk"},{"amplifyapp.com","AWS Amplify"},
    {"herokudns.com","Heroku"},{"herokuapp.com","Heroku"},{"myshopify.com","Shopify"},
    {"webflow.com","Webflow"},{"webflow.io","Webflow"},{"squarespace.com","Squarespace"},
    {"wpengine.com","WP Engine"},{"wpenginepowered.com","WP Engine"},{"kinsta.cloud","Kinsta"},{"pantheonsite.io","Pantheon"},
    {"fly.dev","Fly.io"},{"onrender.com","Render"},{"web.app","Firebase Hosting"},{"firebaseapp.com","Firebase Hosting"},
    {"incapdns.net","Imperva"},{"b-cdn.net","Bunny CDN"},{"kxcdn.com","KeyCDN"},{"cdn77.org","CDN77"},
    {"wixdns.net","Wix"},{"ghost.io","Ghost(Pro)"},{"hubspot.net","HubSpot CMS"},{"hs-sites.com","HubSpot CMS"},{"zendesk.com","Zendesk"}};
const Zone nameserver_zones[]={
    {"ns.cloudflare.com","Cloudflare DNS"},
    {"azure-dns.com","Azure DNS"},{"azure-dns.net","Azure DNS"},{"azure-dns.org","Azure DNS"},{"azure-dns.info","Azure DNS"},
    {"nsone.net","NS1"},{"dnsimple.com","DNSimple"},{"dnsimple-edge.net","DNSimple"},{"dnsimple-edge.org","DNSimple"},
    {"domaincontrol.com","GoDaddy DNS"},{"akam.net","Akamai Edge DNS"},
    {"ultradns.com","UltraDNS"},{"ultradns.net","UltraDNS"},{"ultradns.org","UltraDNS"},{"ultradns.biz","UltraDNS"},
    {"registrar-servers.com","Namecheap DNS"},{"vercel-dns.com","Vercel DNS"},{"dynect.net","Oracle Dyn DNS"},
    {"dnsmadeeasy.com","DNS Made Easy"},{"squarespacedns.com","Squarespace DNS"},{"wixdns.net","Wix DNS"},
    {"ns.hetzner.com","Hetzner DNS"},{"gandi.net","Gandi DNS"},{"alidns.com","Alibaba Cloud DNS"},{"dnspod.net","DNSPod"},
    {"digitalocean.com","DigitalOcean DNS"},{"linode.com","Linode DNS"}};
const Zone mail_zones[]={
    {"mail.protection.outlook.com","Microsoft 365 mail"},
    {"pphosted.com","Proofpoint"},{"ppe-hosted.com","Proofpoint Essentials"},
    {"mimecast.com","Mimecast"},{"messagelabs.com","Symantec Email Security"},
    {"barracudanetworks.com","Barracuda Email Security"},{"iphmx.com","Cisco Secure Email"},
    {"messagingengine.com","Fastmail"},{"mail.icloud.com","iCloud Mail"},{"mx.cloudflare.net","Cloudflare Email Routing"},
    {"emailsrvr.com","Rackspace Email"},{"secureserver.net","GoDaddy email"},{"titan.email","Titan Email"},
    {"privateemail.com","Namecheap Private Email"}};
template<size_t N> std::string zone_product(const std::string& host,const Zone (&zones)[N]) {
    for(const auto& zone:zones)if(below(host,zone.suffix))return zone.product;
    return {};
}
bool route53_nameserver(const std::string& host) {
    if(!host.starts_with("ns-"))return false;
    const auto dot=host.find('.');const std::string zone=".awsdns-";
    if(dot==std::string::npos||dot<4||dot>7||host.compare(dot,zone.size(),zone)!=0)return false;
    for(size_t i=3;i<dot;++i)if(host[i]<'0'||host[i]>'9')return false;
    const auto digits=dot+zone.size();
    if(host.size()<digits+3||host[digits]<'0'||host[digits]>'9'||host[digits+1]<'0'||host[digits+1]>'9')return false;
    const auto tld=host.substr(digits+2);
    return tld==".com"||tld==".net"||tld==".org"||tld==".co.uk";
}
int type_code(const std::string& type) {
    return type=="CNAME"?5:type=="MX"?15:type=="NS"?2:type=="TXT"?16:0;
}
std::string type_name(int type) {
    return type==5?"CNAME":type==15?"MX":type==2?"NS":type==16?"TXT":"";
}
bool number(const std::string& text,unsigned& value,unsigned limit) {
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    return result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&value<=limit;
}
std::string mx_host(const std::string& value) {
    std::istringstream input(value);std::string priority,host,extra;unsigned n=0;
    if(!(input>>priority>>host)||input>>extra||!number(priority,n,65535))return {};
    return dns_name(host);
}
std::string txt_text(const std::string& value) {
    if(value.empty()||value.front()!='"')return value;
    std::string out;size_t i=0;
    while(i<value.size()) {
        while(i<value.size()&&value[i]==' ')++i;
        if(i==value.size())break;
        if(value[i++]!='"')return {};
        bool closed=false;
        while(i<value.size()) {
            char c=value[i++];if(c=='"'){closed=true;break;}
            if(c=='\\') {
                if(i==value.size())return {};
                c=value[i++];if(c!='"'&&c!='\\')return {};
            }
            out+=c;
        }
        if(!closed)return {};
    }
    return out;
}
struct Transfer {
    CURL* handle=nullptr;
    DnsQuery query;
    std::string body;
    bool too_large=false,done=false;
    ~Transfer(){if(handle)curl_easy_cleanup(handle);}
};
size_t receive(char* bytes,size_t size,size_t count,void* data) {
    auto& transfer=*static_cast<Transfer*>(data);const size_t length=size*count;
    if(length>body_limit-transfer.body.size()){transfer.too_large=true;return 0;}
    transfer.body.append(bytes,length);return length;
}
int progress(void* data,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {
    return static_cast<std::atomic_bool*>(data)->load()?1:0;
}
bool resolver_allowed(const std::string& url) {
    if(url=="https://dns.google/resolve")return true;
    try {
        const auto parsed=parse_target(url);
        return parsed.scheme=="http"&&(parsed.host=="127.0.0.1"||parsed.host=="::1")&&
            url.find('?')==std::string::npos&&url.find('#')==std::string::npos;
    }catch(...){return false;}
}
}

DnsQuery parse_dns_json(const std::string& body,const std::string& name,const std::string& type) {
    DnsQuery result;result.name=dns_name(name);result.type=type;result.state="error";
    result.error="Malformed or mismatched DNS response.";
    if(body.size()>body_limit||result.name.empty()||!type_code(type))return result;
    const auto doc=nlohmann::json::parse(body,nullptr,false);
    if(!doc.is_object()||!doc.contains("Status")||!doc["Status"].is_number_integer()||
       !doc.contains("Question")||!doc["Question"].is_array()||doc["Question"].size()!=1)return result;
    const auto& question=doc["Question"][0];
    if(!question.is_object()||!question.contains("name")||!question["name"].is_string()||
       !question.contains("type")||!question["type"].is_number_integer()||
       dns_name(question["name"].get<std::string>())!=result.name||question["type"]!=type_code(type))return result;
    const auto status=doc["Status"].get<int64_t>();if(status<0||status>15)return result;
    result.rcode=static_cast<int>(status);result.error.clear();
    if(doc.contains("TC")&&!doc["TC"].is_boolean()){result.error="Invalid DNS truncation flag.";return result;}
    result.truncated=doc.value("TC",false);
    result.authenticated=doc.contains("AD")&&doc["AD"].is_boolean()&&doc["AD"].get<bool>();
    if(status!=0) {
        result.state=status==3?"nxdomain":"error";
        result.error=status==3?"DNS name does not exist.":"DNS resolver returned code "+std::to_string(status)+".";
        return result;
    }
    if(result.truncated){result.state="limited";result.error="Resolver returned a truncated answer; no findings derived.";return result;}
    result.state="no_records";
    if(!doc.contains("Answer"))return result;
    if(!doc["Answer"].is_array()){result.state="error";result.error="Invalid DNS answer list.";return result;}
    std::vector<DnsRecord> candidates;
    for(const auto& row:doc["Answer"]) {
        if(!row.is_object()||!row.contains("name")||!row["name"].is_string()||
           !row.contains("type")||!row["type"].is_number_integer()||!row.contains("data")||!row["data"].is_string()||
           !row.contains("TTL")||!row["TTL"].is_number_integer())continue;
        const auto raw_type=row["type"].get<int64_t>();const auto ttl=row["TTL"].get<int64_t>();
        if(raw_type!=type_code(type)&&raw_type!=5)continue;
        if(ttl<0||ttl>UINT32_MAX)continue;
        DnsRecord record{dns_name(row["name"].get<std::string>()),type_name(static_cast<int>(raw_type)),row["data"].get<std::string>(),static_cast<uint32_t>(ttl)};
        if(record.name.empty()||record.value.empty()||record.value.size()>2048||
           std::any_of(record.value.begin(),record.value.end(),[](unsigned char c){return c<32||c==127;}))continue;
        if((record.type=="CNAME"||record.type=="NS")&&dns_name(record.value).empty())continue;
        if(record.type=="MX"&&record.value!="0 ."&&mx_host(record.value).empty())continue;
        if(candidates.size()==record_limit){result.truncated=true;break;}
        candidates.push_back(std::move(record));
    }
    std::set<std::string> reached{result.name};
    for(size_t hop=0;hop<8;++hop) {
        const auto before=reached;
        for(const auto& row:candidates)if(row.type=="CNAME"&&before.contains(row.name))reached.insert(dns_name(row.value));
        if(reached.size()==before.size())break;
    }
    std::set<std::string> unique;
    for(auto& row:candidates)if(reached.contains(row.name)) {
        if(row.type=="CNAME"&&!reached.contains(dns_name(row.value))){result.truncated=true;continue;}
        if(unique.insert(row.name+"\n"+row.type+"\n"+row.value).second)result.records.push_back(std::move(row));
    }
    if(!result.records.empty())result.state="complete";
    if(result.truncated){result.state="limited";result.error="DNS answer reached a record or alias-depth limit; partial records retained.";}
    return result;
}

DnsObservation collect_dns_records(const std::string& host,std::atomic_bool& cancel,const std::string& resolver,int timeout_ms,const ProxyConfig& proxy) {
    DnsObservation result;const auto begin=Clock::now();
    const auto name=dns_name(host);
    if(cancel){result.state="cancelled";result.reason="DNS collection cancelled.";return result;}
    if(name.empty()||!resolver_allowed(resolver)){result.state="error";result.reason="Invalid DNS name or resolver.";return result;}
    const auto zone=name.starts_with("www.")?name.substr(4):name;
    std::array<Transfer,4> transfers;
    std::unique_ptr<CURLM,decltype(&curl_multi_cleanup)> multi(curl_multi_init(),curl_multi_cleanup);
    if(!multi){result.state="error";result.reason="Could not initialize DNS collection.";return result;}
    const int bound=std::clamp(timeout_ms,1,3000);const auto deadline=begin+std::chrono::milliseconds(bound);
    const std::array<const char*,4> types={"CNAME","MX","NS","TXT"};
    for(size_t i=0;i<transfers.size();++i) {
        auto& t=transfers[i];auto& q=t.query;
        q.name=i?zone:name;q.type=types[i];q.checked_at=utc_now();q.state="pending";
        q.source=resolver+"?name="+q.name+"&type="+q.type+"&edns_client_subnet=0.0.0.0%2F0";
        t.handle=curl_easy_init();
        if(!t.handle){q.state="error";q.error="Could not initialize DNS request.";t.done=true;continue;}
        curl_easy_setopt(t.handle,CURLOPT_URL,q.source.c_str());
        configure_proxy(t.handle,proxy);curl_easy_setopt(t.handle,CURLOPT_PROTOCOLS_STR,resolver.starts_with("https:")?"https":"http");
        curl_easy_setopt(t.handle,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(t.handle,CURLOPT_NOSIGNAL,1L);
        curl_easy_setopt(t.handle,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(t.handle,CURLOPT_SSL_VERIFYHOST,2L);
        curl_easy_setopt(t.handle,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(bound));curl_easy_setopt(t.handle,CURLOPT_TIMEOUT_MS,static_cast<long>(bound));
        curl_easy_setopt(t.handle,CURLOPT_USERAGENT,"Vector/0.1 (DNS infrastructure)");curl_easy_setopt(t.handle,CURLOPT_ACCEPT_ENCODING,"identity");
        curl_easy_setopt(t.handle,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(t.handle,CURLOPT_WRITEDATA,&t);
        curl_easy_setopt(t.handle,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(t.handle,CURLOPT_XFERINFODATA,&cancel);curl_easy_setopt(t.handle,CURLOPT_NOPROGRESS,0L);
        curl_easy_setopt(t.handle,CURLOPT_PRIVATE,&t);
        if(curl_multi_add_handle(multi.get(),t.handle)!=CURLM_OK){q.state="error";q.error="Could not schedule DNS request.";t.done=true;}
    }
    int active=0;CURLMcode multi_result=CURLM_OK;
    while(!cancel&&Clock::now()<deadline) {
        multi_result=curl_multi_perform(multi.get(),&active);if(multi_result!=CURLM_OK)break;
        int queued=0;
        while(auto* message=curl_multi_info_read(multi.get(),&queued))if(message->msg==CURLMSG_DONE) {
            Transfer* t=nullptr;curl_easy_getinfo(message->easy_handle,CURLINFO_PRIVATE,&t);if(!t)continue;
            t->done=true;auto& q=t->query;long status=0;curl_easy_getinfo(t->handle,CURLINFO_RESPONSE_CODE,&status);
            if(t->too_large){q.state="limited";q.truncated=true;q.error="DNS response exceeded 64 KiB.";}
            else if(message->data.result!=CURLE_OK){q.state=message->data.result==CURLE_OPERATION_TIMEDOUT?"timeout":"error";q.error=curl_easy_strerror(message->data.result);}
            else if(status!=200){q.state="error";q.error="DNS resolver returned HTTP "+std::to_string(status)+".";}
            else {auto parsed=parse_dns_json(t->body,q.name,q.type);parsed.source=q.source;parsed.checked_at=q.checked_at;q=std::move(parsed);}
        }
        if(!active)break;
        int count=0;const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();
        if(left>0)curl_multi_poll(multi.get(),nullptr,0,static_cast<int>(std::min<int64_t>(50,left)),&count);
    }
    size_t finished=0;bool retained=false;
    for(auto& t:transfers) {
        if(t.handle)curl_multi_remove_handle(multi.get(),t.handle);
        if(!t.done){t.query.state=cancel?"cancelled":multi_result==CURLM_OK?"timeout":"error";t.query.error=cancel?"DNS collection cancelled.":multi_result==CURLM_OK?"Shared DNS time limit reached.":curl_multi_strerror(multi_result);}
        if(t.query.state=="complete"||t.query.state=="no_records"||t.query.state=="nxdomain")++finished;
        retained=retained||!t.query.records.empty();
        result.queries.push_back(std::move(t.query));
    }
    result.state=cancel?"cancelled":finished==4?"complete":finished||retained?"partial":"unavailable";
    result.reason=std::to_string(finished)+" of 4 DNS queries completed through "+resolver+"; domain configuration, not proof of software on the selected IP.";
    result.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();return result;
}

DnsObservation inspect_dns(const Target& target,const std::string& address,std::atomic_bool& cancel,const ProxyConfig& proxy) {
    if(cancel){DnsObservation d;d.state="cancelled";d.reason="DNS collection cancelled.";return d;}
    const auto host=dns_name(target.host);
    if(target.is_ip||(!proxy.enabled&&!geolocation_eligible(address))||host.find('.')==std::string::npos||
       host.ends_with(".local")||host.ends_with(".localhost")||host.ends_with(".test")||host.ends_with(".invalid")||host.ends_with(".example")||host.ends_with(".internal")||host.ends_with(".home.arpa")) {
        DnsObservation d;d.state="skipped";d.reason="Domain DNS lookup requires a public hostname resolving to a public IP; IP inputs and local names are skipped.";return d;
    }
    return collect_dns_records(host,cancel,"https://dns.google/resolve",3000,proxy);
}

void identify_dns(const DnsObservation& dns,std::vector<Finding>& findings) {
    for(const auto& query:dns.queries) {
        if(query.state!="complete"&&query.state!="limited")continue;
        for(const auto& record:query.records) {
            std::string product,role;const auto host=dns_name(record.value);
            if(record.type=="CNAME"&&query.type=="CNAME") {
                role="DNS hosting alias";
                if(below(host,"amazonaws.com")&&(host.find(".s3-website-")!=std::string::npos||host.find(".s3-website.")!=std::string::npos))product="Amazon S3 website";
                else product=zone_product(host,alias_zones);
            } else if(record.type=="NS"&&query.type=="NS") {
                role="Authoritative DNS";
                if(route53_nameserver(host))product="Amazon Route 53";
                else if(host.starts_with("ns-cloud-")&&below(host,"googledomains.com"))product="Google Cloud DNS";
                else product=zone_product(host,nameserver_zones);
            } else if(record.type=="MX"&&query.type=="MX") {
                role="Inbound email";const auto mx=mx_host(record.value);
                if(mx=="smtp.google.com"||mx=="aspmx.l.google.com"||
                    (mx.size()==23&&mx.starts_with("alt")&&mx[3]>='1'&&mx[3]<='4'&&mx.substr(4)==".aspmx.l.google.com"))product="Google Workspace mail";
                else if(mx=="mxa.mailgun.org"||mx=="mxb.mailgun.org"||mx=="mxa.eu.mailgun.org"||mx=="mxb.eu.mailgun.org")product="Mailgun mail";
                else if(mx=="mail.protonmail.ch"||mx=="mailsec.protonmail.ch")product="Proton Mail";
                else if(mx=="mx.yandex.net"||mx=="mx.yandex.ru")product="Yandex Mail";
                else if(mx=="mx.sendgrid.net")product="SendGrid Inbound Parse";
                else if(mx.starts_with("inbound-smtp.")&&below(mx,"amazonaws.com"))product="Amazon SES or WorkMail mail";
                else if(mx.starts_with("mx")&&(below(mx,"zoho.com")||below(mx,"zoho.eu")||below(mx,"zoho.in")))product="Zoho Mail";
                else product=zone_product(mx,mail_zones);
            } else if(record.type=="TXT"&&query.type=="TXT") {
                const auto text=lower(txt_text(record.value));std::istringstream tokens(text);std::string token;
                if(!(tokens>>token)||token!="v=spf1")continue;
                const auto spf_count=std::count_if(query.records.begin(),query.records.end(),[&](const auto& other) {
                    if(other.type!="TXT"||other.name!=record.name)return false;
                    std::istringstream words(lower(txt_text(other.value)));std::string first;return (words>>first)&&first=="v=spf1";
                });
                if(spf_count!=1)continue;
                while(tokens>>token) {
                    if(!token.empty()&&token[0]=='+')token.erase(0,1);
                    std::string sender;
                    if(token=="include:_spf.google.com")sender="Google mail authorization";
                    else if(token=="include:spf.protection.outlook.com")sender="Microsoft 365 mail authorization";
                    else if(token.starts_with("include:")) {
                        struct Sender {const char* domain;const char* name;bool subdomains;};
                        static const Sender senders[]={
                            {"amazonses.com","Amazon SES mail authorization",false},
                            {"sendgrid.net","SendGrid mail authorization",false},
                            {"mailgun.org","Mailgun mail authorization",false},
                            {"mail.zendesk.com","Zendesk mail authorization",false},
                            {"_spf.salesforce.com","Salesforce mail authorization",false},
                            {"mktomail.com","Marketo mail authorization",false},
                            {"servers.mcsv.net","Mailchimp mail authorization",false},
                            {"spf.mandrillapp.com","Mailchimp Transactional mail authorization",false},
                            {"spf.mtasv.net","Postmark mail authorization",false},
                            {"mg-spf.greenhouse.io","Greenhouse mail authorization",false},
                            {"spf.mailjet.com","Mailjet mail authorization",false},
                            {"_spf.atlassian.net","Atlassian mail authorization",false},
                            {"hubspotemail.net","HubSpot mail authorization",true},
                            {"aspmx.pardot.com","Pardot mail authorization",false},
                            {"cust-spf.exacttarget.com","Salesforce Marketing Cloud mail authorization",false},
                            {"spf.brevo.com","Brevo mail authorization",false},
                            {"spf.sendinblue.com","Brevo mail authorization",false},
                            {"spf.messagingengine.com","Fastmail mail authorization",false},
                            {"zoho.com","Zoho mail authorization",false},
                            {"zohomail.com","Zoho mail authorization",false},
                            {"_spf.protonmail.ch","Proton Mail authorization",false},
                            {"helpscoutemail.com","Help Scout mail authorization",false},
                            {"email.freshdesk.com","Freshdesk mail authorization",false},
                            {"shops.shopify.com","Shopify mail authorization",false},
                            {"_spf.createsend.com","Campaign Monitor mail authorization",false},
                            {"_spf.firebasemail.com","Firebase mail authorization",false},
                            {"_netblocks.mimecast.com","Mimecast mail authorization",true},
                            {"pphosted.com","Proofpoint mail authorization",true}};
                        const auto included=token.substr(8);
                        for(const auto& known:senders)
                            if(included==known.domain||(known.subdomains&&below(included,known.domain))){sender=known.name;break;}
                    }
                    if(!sender.empty())add_finding(findings,{sender,"Email authorization",{},"Medium",{
                        {"DNS TXT / "+record.name,record.value},{"DNS meaning","Published SPF include; policy effectiveness, account activity and mail delivery are unverified."},
                        {"DNS resolver",query.source},{"DNS checked",query.checked_at},{"DNS TTL",std::to_string(record.ttl)+" seconds"}}});
                    if(token=="-all"||token=="~all"||token=="?all"||token=="+all"||token=="all")break;
                }
            }
            if(!product.empty())add_finding(findings,{product,role,{},"High",{
                {"DNS "+record.type+" / "+record.name,record.value},{"DNS scope",query.name+" / domain configuration; endpoint backend and service availability unverified"},
                {"DNS resolver",query.source},{"DNS checked",query.checked_at},{"DNS TTL",std::to_string(record.ttl)+" seconds"}}});
        }
    }
}
}
