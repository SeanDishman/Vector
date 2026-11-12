#include "vector/endpoint_connections.hpp"
#include "markup.hpp"
#include <algorithm>
#include <map>
#include <optional>
#include <sstream>

namespace vectorapp {
namespace {
void unique(std::vector<std::string>& values,const std::string& value) {
    if(!value.empty()&&std::find(values.begin(),values.end(),value)==values.end())values.push_back(value);
}
std::string hostname(std::string name) {
    name=lower(trim(std::move(name)));
    if(name.ends_with('.'))name.pop_back();
    if(name.empty()||name.size()>253)return {};
    size_t label=0;
    for(unsigned char c:name) {
        if(c=='.'){if(!label||label>63)return {};label=0;}
        else if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_')++label;
        else return {};
    }
    return label&&label<=63?name:std::string{};
}
std::optional<Target> url_target(std::string value) {
    if(value.starts_with("Browser network / "))value.erase(0,18);
    const auto scheme=lower(value.substr(0,8));
    if(!scheme.starts_with("https://")&&!scheme.starts_with("http://"))return {};
    try {
        auto t=parse_target(value);
        if(!t.is_ip)t.host=hostname(t.host);
        if(t.host.empty())return {};
        t.path=t.path.substr(0,t.path.find('?'));
        return t;
    }catch(...){return {};}
}
bool child_of(const std::string& host,const std::string& parent) {
    return !parent.empty()&&host.size()>parent.size()+1&&host.ends_with("."+parent);
}
bool endpoint_source(const ScanReport& report,const std::string& value) {
    const auto source=url_target(value);if(!source)return false;
    bool endpoint=false;
    for(const auto& page:report.browser.pages) {
        for(const auto& u:{page.url,page.final_url}) {
            const auto p=url_target(u);
            if(p&&p->url()==source->url()) {
                if(page.purpose=="disclosure")return false;
                endpoint=true;
            }
        }
    }
    if(endpoint)return true;
    return same_website_host(report.target,*source);
}
std::string txt_policy(const std::string& value) {
    if(!value.starts_with('"'))return value;
    std::string out;size_t i=0;
    while(i<value.size()) {
        while(i<value.size()&&value[i]==' ')++i;
        if(i==value.size())break;
        if(value[i++]!='"')return {};
        bool closed=false;
        while(i<value.size()) {
            char c=value[i++];if(c=='"'){closed=true;break;}
            if(c=='\\'){if(i==value.size())return {};c=value[i++];if(c!='"'&&c!='\\')return {};}
            out+=c;
        }
        if(!closed)return {};
    }
    return out;
}
std::vector<std::pair<std::string,std::string>> dns_hosts(const DnsRecord& record) {
    std::vector<std::pair<std::string,std::string>> out;
    const auto add=[&](const std::string& value,const char* role){const auto host=hostname(value);if(!host.empty())out.emplace_back(host,role);};
    if(record.type=="CNAME")add(record.value,"CNAME alias");
    else if(record.type=="NS")add(record.value,"NS / authoritative DNS");
    else if(record.type=="MX") {
        std::istringstream words(record.value);std::string priority,host,extra;
        if((words>>priority>>host)&&!(words>>extra)&&!priority.empty()&&priority.size()<=5&&
           std::all_of(priority.begin(),priority.end(),[](char c){return c>='0'&&c<='9';})&&std::stoul(priority)<=65535)
            add(host,"MX / inbound mail");
    } else if(record.type=="TXT") {
        std::istringstream words(lower(txt_policy(record.value)));std::string token;
        if(!(words>>token)||token!="v=spf1")return out;
        while(words>>token) {
            if(token=="all"||token=="+all"||token=="-all"||token=="~all"||token=="?all")break;
            if(!token.empty()&&(token[0]=='+'||token[0]=='-'||token[0]=='~'||token[0]=='?'))token.erase(0,1);
            if(token.starts_with("include:"))add(token.substr(8),"SPF include reference");
            else if(token.starts_with("redirect="))add(token.substr(9),"SPF redirect reference");
        }
    }
    return out;
}
}

std::vector<std::string> dns_record_providers(const ScanReport& report,const DnsRecord& record) {
    std::vector<std::string> names;
    const auto source="DNS "+record.type+" / "+record.name;
    for(const auto& f:report.findings)for(const auto& e:f.evidence)
        if(e.source==source&&e.value==record.value)unique(names,f.name);
    return names;
}

std::vector<EndpointHost> endpoint_hosts(const ScanReport& report) {
    std::map<std::string,EndpointHost> hosts;
    auto base=hostname(report.target.host);
    if(base.starts_with("www."))base.erase(0,4);
    const auto add=[&](const std::string& host,const std::string& role,const Evidence& evidence,
                       const std::vector<std::string>& providers=std::vector<std::string>{}) {
        if(host.empty()||host==lower(report.target.host)||host==base)return;
        auto& row=hosts[host];row.host=host;
        row.relation=!report.target.is_ip&&child_of(host,base)?"Subdomain":"External host";
        unique(row.roles,role);
        for(const auto& name:providers)unique(row.providers,name);
        if(std::none_of(row.evidence.begin(),row.evidence.end(),[&](const auto& e){return e.source==evidence.source&&e.value==evidence.value;}))row.evidence.push_back(evidence);
    };
    for(const auto& page:report.web) {
        const auto source=url_target(page.url);
        if(!source||!endpoint_source(report,page.url)||page.status<200||page.status>=400)continue;
        bool html=false;
        for(const auto& [name,value]:page.headers)if(lower(name)=="content-type")html=lower(trim(value.substr(0,value.find(';'))))=="text/html";
        if(!html)continue;
        for(const auto& tag:markup::tags(page.body)) {
            if(tag.name!="a"&&tag.name!="area")continue;
            auto link=trim(tag.attr("href"));if(link.starts_with("//"))link=source->scheme+":"+link;
            const auto destination=url_target(link);
            if(destination&&!report.target.is_ip&&child_of(destination->host,base))
                add(destination->host,"Linked subdomain",{"Page link / "+source->url(),destination->url()+" / not requested by this observation"});
        }
    }
    for(const auto& q:report.browser.requests) {
        const auto destination=url_target(q.url),source=url_target(q.page);
        if(!destination||!source||!endpoint_source(report,q.page))continue;
        const auto basis=q.collection=="captured_http"?"Captured document":q.status>0?"Browser response":"Request attempt";
        add(destination->host,basis,{"Browser / "+source->url(),q.method+" "+destination->url()+" / "+q.type+" / "+q.state+
            (q.status>0?" / HTTP "+std::to_string(q.status):"")+" / "+q.collection});
    }
    for(const auto& v:report.vendors) {
        if(!v.published_by.empty()||v.evidence_level!="Page reference")continue;
        if(v.relationship!="Script"&&v.relationship!="Stylesheet"&&v.relationship!="Embedded frame"&&
           v.relationship!="Image / media"&&v.relationship!="Form destination")continue;
        const auto host=hostname(v.domain);if(host.empty())continue;
        for(const auto& e:v.evidence)if(endpoint_source(report,e.source))
            add(host,"Page reference / "+v.relationship,e,v.name=="Unattributed origin"?std::vector<std::string>{}:std::vector<std::string>{v.name});
    }
    for(const auto& v:report.vendors) {
        if(!v.published_by.empty()||v.evidence_level!="Observed response"||v.name=="Unattributed origin")continue;
        const auto found=hosts.find(hostname(v.domain));if(found==hosts.end())continue;
        if(std::any_of(v.evidence.begin(),v.evidence.end(),[&](const auto& e){return endpoint_source(report,e.source);}))unique(found->second.providers,v.name);
    }
    for(const auto& query:report.dns.queries) {
        if(query.state!="complete"&&query.state!="limited")continue;
        for(const auto& record:query.records) {
            const auto names=dns_record_providers(report,record);
            const auto owner=hostname(record.name);
            if(!report.target.is_ip&&child_of(owner,base))
                add(owner,"DNS owner / "+record.type,{"DNS "+record.type+" / "+record.name,record.value});
            for(const auto& [host,role]:dns_hosts(record)) {
                add(host,role,{"DNS "+record.type+" / "+record.name,record.value},record.type=="TXT"?std::vector<std::string>{}:names);
                add(host,role,{"DNS resolver",query.source});
                add(host,role,{"DNS checked",query.checked_at});
            }
        }
    }
    std::vector<EndpointHost> rows;
    for(auto& [host,row]:hosts){(void)host;rows.push_back(std::move(row));}
    std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.relation=="Subdomain"&&b.relation!="Subdomain";});
    return rows;
}
}
