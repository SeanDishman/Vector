#pragma once
#include "markup.hpp"
#include <set>

namespace vectorapp::disclosure_rows {



struct Row {
    std::string name, heading;
    std::vector<std::pair<std::string,std::string>> fields;
};
struct Result { std::vector<Row> rows;bool truncated=false; };

namespace detail {
inline std::string plain(std::string value) {
    value=markup::entities(std::move(value));
    for(const auto* entity:{"&nbsp;","&#160;","&#xA0;","\xC2\xA0"}) {
        size_t at=0;while((at=value.find(entity,at))!=std::string::npos)value.replace(at,std::char_traits<char>::length(entity)," ");
    }
    std::string out;bool blank=true;
    for(unsigned char c:value) {
        if(std::isspace(c)){if(!blank)out+=' ';blank=true;}
        else if(c>=32){out+=static_cast<char>(c);blank=false;}
    }
    return trim(out);
}
inline std::string label(std::string text) {
    text=lower(plain(std::move(text)));if(text.ends_with(':'))text.pop_back();return trim(text);
}
inline bool name_label(const std::string& value) {
    const auto s=label(value);
    return s=="name"||s=="company"||s=="company name"||s=="entity"||s=="entity name"||s=="provider"||s=="provider name"||
        s=="vendor"||s=="vendor name"||s=="subprocessor"||s=="subprocessor name"||s=="sub-processor"||s=="sub-processor name"||
        s=="service provider"||s=="service provider name";
}
inline bool context_label(const std::string& value) {
    const auto s=label(value);
    return s=="purpose"||s=="purpose of processing"||s=="description"||s=="description of processing"||s=="processing activities"||
        s=="service"||s=="services"||s=="service provided"||s=="product"||s=="products"||s=="applicable services"||
        s=="location"||s=="country"||s=="countries"||s=="processing location"||s=="location of processing";
}
inline bool disclosure_heading(const std::string& text) {
    auto s=lower(plain(text));
    if(s.find('?')!=std::string::npos||s.find("update")!=std::string::npos||s.find("change")!=std::string::npos||s.find("notification")!=std::string::npos)return false;
    for(const auto* word:{"subprocessors","sub-processors","service providers","service partners","our vendors","third-party providers"})
        if(s.find(word)!=std::string::npos)return true;
    return false;
}
inline bool heading(const std::string& tag) {return tag.size()==2&&tag[0]=='h'&&tag[1]>='1'&&tag[1]<='6';}
inline bool name_value(const std::string& text) {
    return !text.empty()&&text.size()<=160&&!name_label(text)&&!context_label(text)&&!disclosure_heading(text)&&
        text.find("://")==std::string::npos&&text.find('\n')==std::string::npos&&
        std::any_of(text.begin(),text.end(),[](unsigned char c){return std::isalnum(c)||c>=128;});
}
struct Node {
    std::string tag,alt,own,id,role,labelledby,accessible_name;
    size_t parent=0,begin=0,end=0,last=0;
    std::vector<size_t> children;
    bool complete=false;
};
struct Document {
    std::vector<Node> nodes{Node{}};std::string visible;bool truncated=false;
    std::string text(size_t i) const {
        const auto& n=nodes[i];if(n.end<n.begin||n.end-n.begin>4096)return {};
        return plain(visible.substr(n.begin,n.end-n.begin));
    }
};
inline bool block(const std::string& name) {
    return name=="div"||name=="p"||name=="li"||name=="dt"||name=="dd"||name=="br"||name=="section"||name=="article"||heading(name);
}
inline Document parse(std::string_view input) {
    Document doc;constexpr size_t bytes=3*1024*1024;
    const std::string html(input.substr(0,bytes));doc.truncated=input.size()>bytes;
    struct Frame {std::string tag;size_t node;bool hidden;};
    std::vector<Frame> stack{{"",0,false}};
    auto space=[&](){if(!doc.visible.empty()&&doc.visible.back()!=' ')doc.visible+=' ';};
    size_t pos=0,tokens=0;
    while(pos<html.size()) {
        if(++tokens>150000||doc.nodes.size()>=24000||stack.size()>=96){doc.truncated=true;break;}
        if(html[pos]!='<') {
            auto end=html.find('<',pos);if(end==std::string::npos)end=html.size();
            if(!stack.back().hidden) {
                const auto text=html.substr(pos,end-pos);doc.visible+=text;
                auto& own=doc.nodes[stack.back().node].own;
                if(own.size()<1025)own+=text.substr(0,1025-own.size());
            }
            pos=end;continue;
        }
        if(html.compare(pos,4,"<!--")==0){auto end=html.find("-->",pos+4);pos=end==std::string::npos?html.size():end+3;continue;}
        size_t end=pos+1;char quote=0;
        for(;end<html.size();++end){char c=html[end];if(quote){if(c==quote)quote=0;}else if(c=='\''||c=='"')quote=c;else if(c=='>')break;}
        if(end==html.size()){doc.truncated=true;break;}
        const auto opening=html.substr(pos,end-pos+1);pos=end+1;
        size_t start=1;const bool closing=opening.size()>2&&opening[1]=='/';if(closing)++start;
        size_t n=start;while(n<opening.size()&&(std::isalnum(static_cast<unsigned char>(opening[n]))||opening[n]=='-'))++n;
        const auto tag=lower(opening.substr(start,n-start));if(tag.empty())continue;
        if(closing) {
            for(size_t s=stack.size();s>1;--s)if(stack[s-1].tag==tag) {
                for(size_t j=s-1;j<stack.size();++j)if(!stack[j].hidden) {
                    auto& node=doc.nodes[stack[j].node];node.end=doc.visible.size();node.last=doc.nodes.size()-1;node.complete=j==s-1;
                }
                stack.resize(s-1);break;
            }
            if(!stack.back().hidden&&block(tag))space();
            continue;
        }
        if(tag=="script"||tag=="style"||tag=="textarea"||tag=="title") {
            auto close=markup::find_ci(html,"</"+tag,pos);
            while(close!=std::string::npos) {
                const auto after=close+2+tag.size();if(after>=html.size()||html[after]=='>'||html[after]=='/'||markup::space(html[after]))break;
                close=markup::find_ci(html,"</"+tag,after);
            }
            pos=close==std::string::npos?html.size():close;continue;
        }
        const auto parsed=markup::tags(opening);
        const auto attrs=parsed.empty()?markup::Tag{}:parsed.front();
        auto style=lower(attrs.attr("style"));style.erase(std::remove_if(style.begin(),style.end(),markup::space),style.end());
        const bool hidden=stack.back().hidden||attrs.attributes.contains("hidden")||lower(attrs.attr("aria-hidden"))=="true"||
            style.find("display:none")!=std::string::npos||style.find("visibility:hidden")!=std::string::npos||
            tag=="template"||tag=="noscript"||tag=="nav"||lower(attrs.attr("role"))=="navigation"||tag=="footer"||tag=="pre"||tag=="code"||tag=="table";
        const bool empty=tag=="br"||tag=="hr"||tag=="img"||tag=="input"||tag=="meta"||tag=="link"||tag=="source"||tag=="wbr"||tag=="area"||tag=="base"||tag=="embed"||tag=="param"||tag=="track";
        size_t index=0;
        if(!hidden) {
            if(block(tag))space();
            index=doc.nodes.size();Node node;node.tag=tag;node.alt=plain(attrs.attr("alt"));node.parent=stack.back().node;
            node.id=attrs.attr("id");node.role=lower(attrs.attr("role"));node.labelledby=attrs.attr("aria-labelledby");node.accessible_name=plain(attrs.attr("aria-label"));
            node.begin=node.end=doc.visible.size();node.last=index;node.complete=empty;
            doc.nodes[node.parent].children.push_back(index);doc.nodes.push_back(std::move(node));
        }
        if(!empty)stack.push_back({tag,index,hidden});
    }
    for(size_t s=1;s<stack.size();++s)if(!stack[s].hidden){auto& n=doc.nodes[stack[s].node];n.end=doc.visible.size();n.last=doc.nodes.size()-1;}
    doc.nodes[0].last=doc.nodes.size()-1;doc.nodes[0].end=doc.visible.size();doc.nodes[0].complete=true;
    return doc;
}
inline void field(Row& row,std::string key,std::string value) {
    key=plain(std::move(key));value=plain(std::move(value));
    if(key.ends_with(':'))key.pop_back();
    if(key.empty()||key.size()>80||value.empty()||value.size()>1024||row.fields.size()>=12)return;
    if(std::none_of(row.fields.begin(),row.fields.end(),[&](const auto& f){return f.first==key&&f.second==value;}))row.fields.emplace_back(std::move(key),std::move(value));
}
inline void fields(const Document& doc,size_t first,size_t last,Row& row) {
    for(size_t i=first;i<=last;++i) {
        const auto& node=doc.nodes[i];if(!node.complete)continue;
        if(node.tag=="dl") {
            std::string key;
            for(auto child:node.children) {
                const auto& part=doc.nodes[child];if(!part.complete)continue;
                if(part.tag=="dt")key=doc.text(child);
                else if(part.tag=="dd"&&(name_label(key)||context_label(key)))field(row,key,doc.text(child));
                else key.clear();
            }
        }
        if(node.tag=="p"||node.tag=="div"||node.tag=="li"||node.tag=="dd") {
            const auto text=doc.text(i);const auto colon=text.find(':');
            if(colon<80&&(name_label(text.substr(0,colon))||context_label(text.substr(0,colon)))) {
                auto value=trim(text.substr(colon+1));
                if(value.find(':')==std::string::npos)field(row,text.substr(0,colon),value);
            }
        }
    }
}
inline bool supported(const Row& row) {
    return name_value(row.name)&&std::any_of(row.fields.begin(),row.fields.end(),[](const auto& f){return context_label(f.first);});
}
inline std::string column_text(const Document& doc,size_t index) {
    const auto& cell=doc.nodes[index];
    if(!cell.complete||cell.last-index>16)return {};
    size_t leaf=index;
    while(doc.nodes[leaf].children.size()==1&&plain(doc.nodes[leaf].own).empty()&&
          (doc.nodes[leaf].tag=="div"||doc.nodes[leaf].tag=="span"))leaf=doc.nodes[leaf].children.front();
    const auto& part=doc.nodes[leaf];
    if(part.tag!="p"&&part.tag!="div"&&part.tag!="span")return {};
    for(size_t j=leaf+1;j<=part.last;++j) {
        const auto& child=doc.nodes[j];
        if(!child.complete||(child.tag!="span"&&child.tag!="a"&&child.tag!="strong"&&child.tag!="b"&&
            child.tag!="em"&&child.tag!="small"&&child.tag!="sup"&&child.tag!="br"))return {};
    }
    return doc.text(index);
}
inline Row repeated_columns(const Document& doc,size_t index,const std::string& section) {
    Row row;const auto& node=doc.nodes[index];
    if(node.tag!="div"||!node.complete||!doc.nodes[node.parent].complete||node.children.size()!=2||!plain(node.own).empty())return row;
    const auto first=node.children[0],second=node.children[1];
    const auto name=column_text(doc,first),description=column_text(doc,second);
    if(!name_value(name)||description.empty()||description.size()>1024)return row;
    const auto& siblings=doc.nodes[node.parent].children;
    if(siblings.size()<2||siblings.size()>256)return row;
    bool repeated=false;
    const auto position=std::find(siblings.begin(),siblings.end(),index);
    if(position==siblings.end())return row;
    const size_t at=static_cast<size_t>(position-siblings.begin());
    for(size_t n=(at?at-1:at);n<std::min(siblings.size(),at+2);++n) {
        const auto sibling=siblings[n];
        const auto& other=doc.nodes[sibling];
        if(sibling==index||other.tag!=node.tag||!other.complete||other.children.size()!=2||!plain(other.own).empty())continue;
        if(doc.nodes[other.children[0]].tag!=doc.nodes[first].tag||doc.nodes[other.children[1]].tag!=doc.nodes[second].tag)continue;
        const auto other_name=column_text(doc,other.children[0]),other_description=column_text(doc,other.children[1]);
        if(name_value(other_name)&&!other_description.empty()&&other_description.size()<=1024){repeated=true;break;}
    }
    if(repeated){row.name=name;row.heading=section;field(row,"Description",description);}
    return row;
}
inline Row card(const Document& doc,size_t index,const std::string& section) {
    Row row;row.heading=section;const auto& node=doc.nodes[index];
    if(!node.complete||node.last-index>96||node.end-node.begin>4096)return row;
    for(size_t j=index+1;j<=node.last;++j)
        if(doc.nodes[j].tag=="article"||doc.nodes[j].tag=="li"||doc.nodes[j].tag=="h1"||doc.nodes[j].tag=="h2")return row;
    fields(doc,index,node.last,row);std::set<std::string> names;
    for(const auto& f:row.fields)if(name_label(f.first))names.insert(f.second);
    if(names.empty())for(size_t j=index+1;j<=node.last;++j) {
        const auto& child=doc.nodes[j];
        if(child.complete&&(heading(child.tag)||child.tag=="strong"||child.tag=="b")) {
            const auto text=doc.text(j);if(name_value(text)&&text.find(':')==std::string::npos)names.insert(text);
        }
    }
    if(names.size()==1)row.name=*names.begin();
    size_t anchors=0;
    if(!row.name.empty())for(size_t j=index+1;j<=node.last;++j) {
        const auto& child=doc.nodes[j];if(!child.complete)continue;
        const auto text=doc.text(j);const auto colon=text.find(':');
        if((heading(child.tag)||child.tag=="strong"||child.tag=="b")&&text==row.name) {
            const bool nested=std::any_of(child.children.begin(),child.children.end(),[&](size_t c){return (heading(doc.nodes[c].tag)||doc.nodes[c].tag=="strong"||doc.nodes[c].tag=="b")&&doc.text(c)==text;});
            if(!nested)++anchors;
        }
        else if((child.tag=="p"||child.tag=="div")&&colon<80&&name_label(text.substr(0,colon))&&trim(text.substr(colon+1))==row.name) {
            const bool nested=std::any_of(child.children.begin(),child.children.end(),[&](size_t c){return doc.text(c)==text;});
            if(!nested)++anchors;
        } else if(child.tag=="dd"&&text==row.name)++anchors;
    }
    if(anchors>1)return Row{};
    if(supported(row))return row;
    if(node.tag=="li"&&anchors==1&&name_value(row.name)) {
        const auto text=doc.text(index);
        if(text.starts_with(row.name)) {
            const auto tail=trim(text.substr(row.name.size()));
            if(tail.starts_with(':'))field(row,"Description",tail.substr(1));
            if(supported(row))return row;
        }
    }
    row.name.clear();row.fields.clear();
    std::vector<std::string> logos;
    for(size_t j=index+1;j<=node.last;++j) {
        if(heading(doc.nodes[j].tag))return Row{};
        if(doc.nodes[j].tag=="img"&&name_value(doc.nodes[j].alt))logos.push_back(doc.nodes[j].alt);
    }
    if(logos.size()!=1)return row;
    const auto& name=logos.front();size_t name_node=0;
    for(size_t j=index+1;j<=node.last;++j)if(doc.nodes[j].complete&&plain(doc.nodes[j].own)==name){name_node=j;break;}
    if(!name_node)return row;
    const auto parent=doc.nodes[name_node].parent;
    for(auto sibling:doc.nodes[parent].children)if(sibling>name_node&&doc.nodes[sibling].complete) {
        const auto& part=doc.nodes[sibling];const auto text=doc.text(sibling);
        if((part.tag=="p"||part.tag=="div")&&text.size()>=12&&text.size()<=1024&&text!=name&&part.last-sibling<=8) {
            row.name=name;field(row,"Description",text);break;
        }
    }
    return row;
}
}

inline Result extract(std::string_view html) {
    const auto doc=detail::parse(html);Result result;result.truncated=doc.truncated;
    std::string section;size_t boundary=doc.nodes.size();std::set<std::string> seen;
    std::map<std::string,size_t> labels;
    for(size_t i=1;i<doc.nodes.size();++i)if(doc.nodes[i].complete&&!doc.nodes[i].id.empty())labels.emplace(doc.nodes[i].id,i);
    auto append=[&](Row row) {
        if(!detail::supported(row))return false;
        std::string key=row.heading+"\n"+row.name;for(const auto& f:row.fields)key+="\n"+f.first+": "+f.second;
        if(!seen.insert(key).second)return true;
        if(result.rows.size()>=128){result.truncated=true;return true;}
        result.rows.push_back(std::move(row));return true;
    };
    for(size_t i=1;i<doc.nodes.size();++i) {
        const auto& node=doc.nodes[i];if(i>boundary)section.clear();
        if(node.complete&&(node.role=="tabpanel"||node.role=="region")) {
            auto name=node.accessible_name;
            if(!node.labelledby.empty()) {
                name.clear();size_t pos=0;
                while(pos<node.labelledby.size()) {
                    const auto start=node.labelledby.find_first_not_of(" \t\r\n",pos);if(start==std::string::npos)break;
                    const auto end=node.labelledby.find_first_of(" \t\r\n",start);const auto key=node.labelledby.substr(start,end-start);
                    if(const auto it=labels.find(key);it!=labels.end()){if(!name.empty())name+=' ';name+=doc.text(it->second);}
                    if(end==std::string::npos)break;
                    pos=end;
                }
            }
            section=detail::disclosure_heading(name)?name:std::string{};boundary=node.last;
        }
        if(detail::heading(node.tag)&&node.complete) {
            const auto text=doc.text(i);section=detail::disclosure_heading(text)?text:std::string{};
            boundary=doc.nodes.size();
            for(size_t parent=node.parent;parent;parent=doc.nodes[parent].parent)
                if(doc.nodes[parent].tag=="section"||doc.nodes[parent].tag=="article"||doc.nodes[parent].tag=="main"){boundary=doc.nodes[parent].last;break;}
            i=node.last;continue;
        }
        if(section.empty())continue;
        if(node.tag=="dl"&&node.complete) {
            bool emitted=false;
            for(size_t c=0;c+1<node.children.size();++c) {
                const auto term=node.children[c],description=node.children[c+1];
                if(doc.nodes[term].tag!="dt"||doc.nodes[description].tag!="dd"||!doc.nodes[term].complete||!doc.nodes[description].complete)continue;
                Row row;row.heading=section;row.name=doc.text(term);
                detail::fields(doc,description,doc.nodes[description].last,row);emitted=append(std::move(row))||emitted;
            }
            if(emitted){i=node.last;continue;}
        }
        if(node.tag=="li"||node.tag=="article"||node.tag=="div"||node.tag=="section") {
            auto row=detail::card(doc,i,section);
            if(!detail::supported(row))row=detail::repeated_columns(doc,i,section);
            if(append(std::move(row)))i=node.last;
        }
    }
    return result;
}
}
