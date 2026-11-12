#include "vector/model.hpp"
#include "markup.hpp"
#include "disclosure_rows.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>

namespace vectorapp {
namespace {
std::string company_host(std::string host) {
    host=lower(host);if(host.ends_with('.'))host.pop_back();
    if(host.starts_with("www."))host.erase(0,4);
    return host;
}
std::string resolve_link(const std::string& base,const std::string& ref) {
    std::unique_ptr<CURLU,decltype(&curl_url_cleanup)> url(curl_url(),curl_url_cleanup);
    if(!url||curl_url_set(url.get(),CURLUPART_URL,base.c_str(),0)||curl_url_set(url.get(),CURLUPART_URL,ref.c_str(),0))return {};
    curl_url_set(url.get(),CURLUPART_FRAGMENT,nullptr,0);
    char* text=nullptr;std::string result;
    if(curl_url_get(url.get(),CURLUPART_URL,&text,0)==CURLUE_OK){result=text;curl_free(text);}
    return result;
}
std::string plain(std::string text) {
    text=markup::entities(std::move(text));
    for(const auto& token:{"&nbsp;","&#160;","\xC2\xA0"}) {
        size_t at=0;while((at=text.find(token,at))!=std::string::npos)text.replace(at,std::char_traits<char>::length(token)," ");
    }
    std::string out;bool blank=true;
    for(unsigned char c:text) {
        if(std::isspace(c)){if(!blank)out+=' ';blank=true;}
        else if(c>=32){out+=static_cast<char>(c);blank=false;}
    }
    return trim(out);
}
bool disclosure_heading(const std::string& heading) {
    const auto h=lower(heading);
    for(const auto* word:{"subprocessor","sub-processor","service providers","service partners","our vendors","third-party providers"})
        if(h.find(word)!=std::string::npos)return true;
    return false;
}
bool heading_tag(const std::string& name) {return name.size()==2&&name[0]=='h'&&name[1]>='1'&&name[1]<='6';}
bool bold_disclosure_heading(const std::string& text) {
    const auto h=lower(plain(text));
    return h=="third-party subprocessors"||h=="affiliates of third-party subprocessors"||h.ends_with(" group subprocessors");
}
bool table_detail_heading(std::string text) {
    text=lower(plain(std::move(text)));
    for(char& c:text)if(!std::isalnum(static_cast<unsigned char>(c)))c=' ';
    std::istringstream words(text);std::string word;bool any=false;
    static const std::set<std::string> labels={"entity","entities","name","names","company","companies","provider","providers",
        "vendor","vendors","country","countries","location","locations","region","regions","service","services",
        "purpose","purposes","use","uses","processing","activity","activities","data","center","centers","product","products","and"};
    while(words>>word){if(!labels.contains(word))return false;any=true;}
    return any;
}
std::string relationship_for(const std::string& heading) {
    const auto h=lower(heading);
    if(h.find("group subprocessors")!=std::string::npos)return "Group subprocessor";
    if(h.find("affiliates of")!=std::string::npos)return "Subprocessor affiliate";
    return h.find("processor")!=std::string::npos?"Subprocessor":"Service partner";
}
struct Table { std::string heading;std::vector<std::vector<std::string>> rows; };


std::vector<Table> tables(const std::string& input) {
    const auto html=input.substr(0,3*1024*1024);std::vector<Table> result;
    struct Element {std::string name;bool hidden;};std::vector<Element> stack;
    std::string heading,heading_text,bold_text,cell,disclosure_parent;int heading_depth=0,table_depth=0,disclosure_level=0;bool in_cell=false,in_bold=false;
    Table table;std::vector<std::string> row;
    auto hidden=[&](){return !stack.empty()&&stack.back().hidden;};
    auto end_cell=[&](){if(in_cell){row.push_back(plain(cell));cell.clear();in_cell=false;}};
    auto end_row=[&](){end_cell();if(!row.empty()&&table.rows.size()<260)table.rows.push_back(std::move(row));row.clear();};
    size_t pos=0,tokens=0;
    while(pos<html.size()&&tokens++<150000) {
        if(html[pos]!='<') {
            auto end=html.find('<',pos);if(end==std::string::npos)end=html.size();
            if(!hidden()) {
                if(heading_depth&&heading_text.size()<512)heading_text+=html.substr(pos,std::min(end-pos,512-heading_text.size()));
                if(in_bold&&bold_text.size()<160)bold_text+=html.substr(pos,std::min(end-pos,160-bold_text.size()));
                if(in_cell&&table_depth==1&&cell.size()<1024)cell+=html.substr(pos,std::min(end-pos,1024-cell.size()));
            }
            pos=end;continue;
        }
        if(html.compare(pos,4,"<!--")==0){auto end=html.find("-->",pos+4);pos=end==std::string::npos?html.size():end+3;continue;}
        size_t end=pos+1;char quote=0;
        for(;end<html.size();++end) {
            const char c=html[end];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break;
        }
        if(end==html.size())break;
        const auto opening=html.substr(pos,end-pos+1);pos=end+1;
        size_t start=1;const bool closing=opening.size()>2&&opening[1]=='/';if(closing)++start;
        size_t n=start;while(n<opening.size()&&(std::isalnum(static_cast<unsigned char>(opening[n]))||opening[n]=='-'))++n;
        const auto name=lower(opening.substr(start,n-start));if(name.empty())continue;
        if(closing) {
            const bool was_hidden=hidden();
            if(!was_hidden) {
                if(in_cell&&table_depth==1&&(name=="p"||name=="div"||name=="li"||name=="sup"))cell+=' ';
                if((name=="td"||name=="th")&&table_depth==1)end_cell();
                if(name=="tr"&&table_depth==1)end_row();
                if(name=="table"&&table_depth>0) {
                    if(--table_depth==0){end_row();if(!table.rows.empty()&&result.size()<16)result.push_back(std::move(table));table={};}
                }
                if(heading_tag(name)&&heading_depth) {
                    const auto text=plain(heading_text);const int level=name[1]-'0';
                    if(!text.empty()) {
                        if(disclosure_heading(text)){heading=disclosure_parent=text;disclosure_level=level;}
                        else if(!disclosure_parent.empty()&&level>disclosure_level&&table_detail_heading(text))heading=disclosure_parent+" / "+text;
                        else {heading=text;disclosure_parent.clear();disclosure_level=0;}
                    }
                    heading_depth=0;
                }
                if(name=="strong"&&in_bold){if(bold_disclosure_heading(bold_text)){heading=plain(bold_text);disclosure_parent.clear();}in_bold=false;}
            }
            for(size_t i=stack.size();i>0;--i)if(stack[i-1].name==name){stack.resize(i-1);break;}
            continue;
        }
        auto parsed=markup::tags(opening);std::map<std::string,std::string> attrs;
        if(!parsed.empty())attrs=parsed.front().attributes;
        auto style=lower(attrs["style"]);
        style.erase(std::remove_if(style.begin(),style.end(),markup::space),style.end());
        const bool concealed=hidden()||attrs.contains("hidden")||lower(attrs["aria-hidden"])=="true"||
            style.find("display:none")!=std::string::npos||style.find("visibility:hidden")!=std::string::npos||
            name=="nav"||name=="footer"||name=="template"||name=="noscript";
        if(name=="script"||name=="style"||name=="textarea"||name=="pre"||name=="code"||name=="title") {
            auto close=markup::find_ci(html,"</"+name,pos);
            while(close!=std::string::npos) {
                const auto after=close+2+name.size();if(after>=html.size()||html[after]=='>'||markup::space(html[after]))break;
                close=markup::find_ci(html,"</"+name,after);
            }
            pos=close==std::string::npos?html.size():close;continue;
        }
        const bool void_element=name=="br"||name=="hr"||name=="img"||name=="input"||name=="meta"||name=="link"||name=="source"||name=="wbr";
        if(!void_element) {if(stack.size()>=128)break;stack.push_back({name,concealed});}
        if(concealed)continue;
        if(heading_tag(name)){heading_depth=1;heading_text.clear();}
        if(name=="strong"&&!table_depth){in_bold=true;bold_text.clear();}
        if(name=="table") {
            if(table_depth++==0){table={};table.heading=heading;row.clear();cell.clear();in_cell=false;}
        }
        if(table_depth==1) {
            if(name=="tr"){end_row();}
            if(name=="td"||name=="th"){end_cell();in_cell=true;}
            if(in_cell&&(name=="br"||name=="p"||name=="div"||name=="li"||name=="sup"))cell+=' ';
        }
    }
    return result;
}
size_t add_provider(ScanReport& report,const std::string& name,const std::string& publisher,const std::string& relationship,
                    const std::string& source,const std::string& date,const std::string& value,const std::string& scope={}) {
    if(name.empty()||name.size()>160)return 0;
    const std::string basis="Published disclosure";
    const auto evidence=value+"\nEvidence basis: "+basis;
    const auto row_key=[](const std::string& text){return text.substr(0,text.find("\nCollected: "));};
    for(auto& existing:report.vendors)if(existing.published_by==publisher&&lower(existing.name)==lower(name)) {
        if(std::none_of(existing.evidence.begin(),existing.evidence.end(),[&](const auto& e){return e.source==source&&row_key(e.value)==row_key(evidence);})) {
            if(existing.evidence.size()>=8){report.vendors_truncated=true;return 0;}
            existing.evidence.push_back({source,evidence});
            if(existing.relationship!=relationship)existing.relationship="Published provider";
            if(existing.evidence_level!=basis)existing.evidence_level="Published sources";
            if(date>existing.recorded_at)existing.recorded_at=date;
            existing.capability=publisher+" names this organization in multiple published records. Each source below retains its product scope, role and date. These relationships do not identify software running on this endpoint or verify account permissions.";
        }
        return 1;
    }
    if(report.vendors.size()>=256){report.vendors_truncated=true;return 0;}
    VendorRelationship v;v.name=name;v.category="Published provider";v.relationship=relationship;
    v.evidence_level=basis;v.confidence="High";
    v.published_by=publisher;v.recorded_at=date;v.attribution_source=source;
    v.capability=publisher+" names this organization as a "+lower(relationship)+". This is a published relationship, not observed software or verified account permissions.";
    if(!scope.empty())v.capability+=" Product scope: "+scope+"; this does not establish use by the inspected endpoint.";
    v.evidence.push_back({source,evidence});report.vendors.push_back(std::move(v));return 1;
}
}
bool disclosure_url_allowed(const Target& subject,const std::string& url) {
    try {
        const auto target=parse_target(url);const auto host=company_host(subject.host);
        if(target.port==9100)return false;
        if(target.host==subject.host&&target.port==subject.port) return subject.scheme!="https"||target.scheme=="https";
        if(subject.is_ip||target.is_ip||target.scheme!="https"||target.port!=443)return false;
        return company_host(target.host)==host||target.host=="privacy."+host||target.host=="trust."+host||target.host=="security."+host||target.host=="legal."+host;
    }catch(const std::exception&){return false;}
}
namespace {
int link_priority(std::string text) {
    text=lower(plain(std::move(text)));
    if(text.size()>160||text.find("://")!=std::string::npos)return 0;
    for(char& c:text)if(c=='-'||c=='_'||c=='&')c=' ';
    text=plain(text);
    for(const size_t copies:{3U,2U}) {
        if((text.size()+1)%copies)continue;
        const size_t width=(text.size()+1)/copies-1;
        if(!width)continue;
        const auto label=text.substr(0,width);bool repeated=true;
        for(size_t i=1;i<copies;++i)
            if(text[i*(width+1)-1]!=' '||text.substr(i*(width+1),width)!=label)repeated=false;
        if(repeated)text=label;
    }
    static const std::regex providers(R"(\b(subprocessors?|sub processors?|service providers|service partners|third party providers|vendor list)\b)");
    static const std::regex trust(R"(\b(data processing|dpa|trust cent(?:er|re)|security(?: and)? compliance|compliance cent(?:er|re))\b)");
    static const std::regex legal(R"(^(?:(?:read|view|visit|explore|see|learn more about|about) )?(?:(?:our|the) )?(?:privacy(?: policy| notice)?|legal(?: notice| information)?|security(?: overview| policy)?|trust|terms(?: of service| of use| and conditions)?|compliance(?: documentation| overview)?)$)");
    if(std::regex_search(text,providers))return 3;
    if(std::regex_search(text,trust))return 2;
    if(std::regex_match(text,legal))return 1;
    return 0;
}
int path_priority(std::string path) {
    path.resize(path.find_first_of("?#")==std::string::npos?path.size():path.find_first_of("?#"));
    std::istringstream parts(lower(path));std::string segment;int priority=0;
    while(std::getline(parts,segment,'/')) {
        if(segment=="blog"||segment=="news"||segment=="showcase"||segment=="customers"||segment=="case-studies")return 0;
        for(const auto* extension:{".html",".htm",".php",".aspx",".pdf",".xml"})
            if(segment.ends_with(extension))segment.resize(segment.size()-std::char_traits<char>::length(extension));
        priority=std::max(priority,link_priority(segment));
    }
    return priority;
}
bool linked_destination(const Target& subject,const std::string& from,const std::string& url,bool explicit_label) {
    try {
        const auto dest=parse_target(url),origin=parse_target(from);
        if(dest.port==9100||(origin.scheme=="https"&&dest.scheme!="https"))return false;
        if(disclosure_url_allowed(subject,url))return true;
        if(dest.host==origin.host&&dest.port==origin.port)return true;
        return explicit_label&&dest.scheme=="https"&&dest.port==443&&!dest.is_ip&&!subject.is_ip;
    }catch(const std::exception&){return false;}
}
struct SourceLink {std::string ref,label;};
std::vector<SourceLink> source_links(const std::string& input) {
    const auto html=input.substr(0,3*1024*1024);std::vector<SourceLink> links;
    struct Node {std::string name;bool hidden;};std::vector<Node> stack;
    SourceLink active;bool in_link=false;size_t pos=0,tokens=0;
    auto hidden=[&](){return !stack.empty()&&stack.back().hidden;};
    auto finish=[&](){if(in_link&&!active.ref.empty()&&links.size()<512)links.push_back(std::move(active));active={};in_link=false;};
    while(pos<html.size()&&tokens++<150000) {
        if(html[pos]!='<') {
            auto end=html.find('<',pos);if(end==std::string::npos)end=html.size();
            if(in_link&&!hidden()&&active.label.size()<512)active.label+=html.substr(pos,std::min(end-pos,512-active.label.size()));
            pos=end;continue;
        }
        if(html.compare(pos,4,"<!--")==0){auto end=html.find("-->",pos+4);pos=end==std::string::npos?html.size():end+3;continue;}
        size_t end=pos+1;char quote=0;
        for(;end<html.size();++end){char c=html[end];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break;}
        if(end==html.size())break;
        const auto opening=html.substr(pos,end-pos+1);pos=end+1;
        size_t start=1;const bool closing=opening.size()>2&&opening[1]=='/';if(closing)++start;
        size_t n=start;while(n<opening.size()&&(std::isalnum(static_cast<unsigned char>(opening[n]))||opening[n]=='-'))++n;
        const auto name=lower(opening.substr(start,n-start));if(name.empty())continue;
        if(closing) {
            if(name=="a")finish();
            for(size_t i=stack.size();i>0;--i)if(stack[i-1].name==name){stack.resize(i-1);break;}
            continue;
        }
        auto parsed=markup::tags(opening);std::map<std::string,std::string> attrs;if(!parsed.empty())attrs=parsed.front().attributes;
        auto style=lower(attrs["style"]);style.erase(std::remove_if(style.begin(),style.end(),markup::space),style.end());
        const bool concealed=hidden()||attrs.contains("hidden")||lower(attrs["aria-hidden"])=="true"||style.find("display:none")!=std::string::npos||style.find("visibility:hidden")!=std::string::npos||name=="template"||name=="noscript";
        if(name=="script"||name=="style"||name=="textarea"||name=="pre"||name=="code"||name=="title") {
            auto close=markup::find_ci(html,"</"+name,pos);
            while(close!=std::string::npos) {const auto after=close+2+name.size();if(after>=html.size()||html[after]=='>'||markup::space(html[after]))break;close=markup::find_ci(html,"</"+name,after);}
            pos=close==std::string::npos?html.size():close;continue;
        }
        const bool empty=name=="br"||name=="hr"||name=="img"||name=="input"||name=="meta"||name=="link"||name=="source"||name=="wbr"||name=="area";
        if(!empty){if(stack.size()>=128)break;stack.push_back({name,concealed});}
        if(concealed)continue;
        if(name=="a") {finish();in_link=true;active.ref=attrs["href"];active.label=attrs["aria-label"]+" "+attrs["title"]+" ";}
        if(in_link&&name=="img")active.label+=" "+attrs["alt"];
    }
    return links;
}
}
std::vector<DisclosureCandidate> disclosure_candidates(const HttpObservation& page,const Target& subject,const DisclosureCandidate* parent) {
    std::vector<DisclosureCandidate> result;std::set<std::string> seen;
    if((page.status&& (page.status<200||page.status>=300))||!page.error.empty()||(parent&&parent->depth>=4))return result;
    for(const auto& [key,value]:page.headers)if(lower(key)=="content-type"&&lower(value).find("html")==std::string::npos)return result;
    try {if(!parent&&!disclosure_url_allowed(subject,page.url)&&!same_website_host(subject,parse_target(page.url)))return result;}
    catch(const std::exception&){return result;}
    for(const auto& link:source_links(page.body)) {
        auto url=resolve_link(page.url,link.ref);if(url.empty()||url.size()>2048)continue;
        try {
            const auto dest=parse_target(url);url=dest.url();
            const auto label=plain(link.label);const auto label_priority=link_priority(label);
            const auto priority=std::max(label_priority,path_priority(dest.path));
            if(!priority||url==parse_target(page.url).url()||!linked_destination(subject,page.url,url,label_priority>0)||!seen.insert(url).second)continue;
            DisclosureCandidate next;next.url=url;next.discovered_from=page.url;next.label=label;next.priority=priority;next.depth=parent?parent->depth+1:1;
            if(parent)next.chain=parent->chain;
            next.chain.push_back({page.url,"Link: "+(label.empty()?link.ref:label)+" -> "+url});
            result.push_back(std::move(next));
        }catch(const std::exception&){}
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.priority>b.priority;});
    if(result.size()>24)result.resize(24);
    return result;
}
std::vector<std::string> disclosure_links(const HttpObservation& page,const Target& subject) {
    std::vector<std::string> urls;for(const auto& candidate:disclosure_candidates(page,subject))urls.push_back(candidate.url);return urls;
}
size_t identify_disclosures(const HttpObservation& page,ScanReport& report,const DisclosureCandidate* source) {
    if((!disclosure_url_allowed(report.target,page.url)&&(!source||source->url!=page.url||source->chain.empty()))||page.status<200||page.status>=300||!page.error.empty())return 0;
    for(const auto& [name,value]:page.headers)if(name=="content-type"&&lower(value).find("html")==std::string::npos)return 0;
    const auto publisher=company_host(report.target.host);
    const auto scope=page.title;
    const auto date=utc_now();size_t added=0;std::set<std::string> seen;
    for(const auto& table:tables(page.body)) {
        if(!disclosure_heading(table.heading)||table.rows.size()<2)continue;
        size_t header=0,company=0;bool qualified=false;
        for(;header<std::min<size_t>(4,table.rows.size());++header) {
            const auto& columns=table.rows[header];company=columns.size();bool context=false;
            for(size_t i=0;i<columns.size();++i) {
                const auto label=lower(columns[i]);
                if(company==columns.size()&&disclosure_rows::detail::name_label(label))company=i;
            }
            for(size_t i=0;i<columns.size();++i)if(i!=company) {
                const auto label=lower(columns[i]);
                if(label.find("country")!=std::string::npos||label.find("location")!=std::string::npos||label.find("purpose")!=std::string::npos||label.find("service")!=std::string::npos||label.find("processing")!=std::string::npos)context=true;
            }
            if(company<columns.size()&&context&&columns.size()<=12){qualified=true;break;}
            if(columns.size()!=1)break;
        }
        if(!qualified)continue;
        const auto& columns=table.rows[header];
        for(size_t i=header+1;i<table.rows.size();++i) {
            const auto& cells=table.rows[i];if(cells.size()!=columns.size()||cells[company].empty()||cells[company].size()>160)continue;
            if(cells==columns||lower(cells[company])==lower(columns[company]))continue;
            const auto key=lower(cells[company]);
            if(!seen.contains(key)&&added>=128){report.vendors_truncated=true;continue;}
            std::string evidence="Heading: "+table.heading;
            for(size_t c=0;c<header;++c)evidence+="\nTable context: "+table.rows[c][0];
            const auto relationship=relationship_for(table.heading);
            evidence+="\nRelationship: "+relationship;
            if(!scope.empty())evidence+="\nProduct scope: "+scope;
            for(size_t c=0;c<cells.size();++c)evidence+="\n"+columns[c]+": "+cells[c];
            if(source)for(const auto& step:source->chain)evidence+="\nDiscovery: "+step.source+" / "+step.value;
            evidence+="\nCollected: "+date;
            if(add_provider(report,cells[company],publisher,relationship,page.url,date,evidence,scope)&&seen.insert(key).second)++added;
        }
    }
    const auto cards=disclosure_rows::extract(page.body);
    if(cards.truncated)report.vendors_truncated=true;
    for(const auto& row:cards.rows) {
        const auto key=lower(row.name);
        if(!seen.contains(key)&&added>=128){report.vendors_truncated=true;continue;}
        const auto relationship=relationship_for(row.heading);
        std::string evidence="Heading: "+row.heading+"\nRelationship: "+relationship+"\nName: "+row.name;
        if(!scope.empty())evidence+="\nProduct scope: "+scope;
        for(const auto& [label,value]:row.fields)evidence+="\n"+label+": "+value;
        if(source)for(const auto& step:source->chain)evidence+="\nDiscovery: "+step.source+" / "+step.value;
        evidence+="\nCollected: "+date;
        if(add_provider(report,row.name,publisher,relationship,page.url,date,evidence,scope)&&seen.insert(key).second)++added;
    }
    return added;
}
}
