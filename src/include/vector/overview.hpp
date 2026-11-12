#pragma once
#include "vector/model.hpp"
#include <algorithm>
#include <exception>

namespace vectorapp {
struct InfrastructureEntry {
    std::string name, detail;
    int finding = -1, vendor = -1;
};

inline std::vector<InfrastructureEntry> infrastructure_entries(const ScanReport& report) {
    std::vector<InfrastructureEntry> rows;
    const auto display_name=[](const std::string& name) {
        if(name=="Amazon CloudFront")return std::string("CloudFront");
        if(name=="YouTube player"||name=="YouTube web client")return std::string("YouTube");
        return name;
    };
    const auto dns_role=[](const Finding& f) {
        return f.category=="Inbound email"||f.category=="Email authorization"||
               f.category=="Authoritative DNS"||f.category=="DNS hosting alias";
    };
    const auto priority=[&](const Finding& f) {
        if(f.category=="Edge"||f.category=="Hosting"||f.category=="CDN"||f.category=="Load balancer")return 0;
        if(f.category=="Server")return 1;
        if(f.category=="Runtime")return 2;
        if(f.category=="CMS"||f.category=="Framework"||f.category=="Platform")return 3;
        if(dns_role(f))return 5;
        return 4;
    };
    std::vector<int> findings;
    for(size_t i=0;i<report.findings.size();++i) {
        const auto& f=report.findings[i];
        if(f.category=="Fingerprint"||f.category=="Service"||f.category=="Package"||
           f.category.find("hint")!=std::string::npos)continue;
        findings.push_back(static_cast<int>(i));
    }
    std::stable_sort(findings.begin(),findings.end(),[&](int a,int b){return priority(report.findings[a])<priority(report.findings[b]);});
    for(const int index:findings) {
        const auto& f=report.findings[index];
        const bool youtube=f.name=="YouTube player"||f.name=="YouTube web client";
        const auto name=display_name(f.name);
        if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.name==name;}))continue;
        rows.push_back({name,youtube?"Video":f.category=="Edge"?"Edge / CDN":f.category,index,-1});
    }
    const auto strength=[](const VendorRelationship& v) {
        if(!v.published_by.empty()||v.name=="Unattributed origin")return 0;
        if(v.evidence_level=="Observed response"||v.evidence_level=="Response header")return 3;
        if(v.evidence_level=="Page reference" && (v.relationship=="Script"||v.relationship=="Embedded frame"||
           v.relationship=="Stylesheet"||v.relationship=="Image / media"||v.relationship=="Form destination"))return 2;
        if(v.evidence_level=="Request attempt")return 2;
        if(v.relationship=="Supported source"&&v.evidence_level=="Form input")return 1;
        return 0;
    };
    for(size_t i=0;i<report.vendors.size();++i) {
        const auto& v=report.vendors[i];if(!strength(v))continue;
        const auto name=display_name(v.name);
        auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.name==name;});
        const auto basis=v.evidence_level=="Observed response"?"Observed contact":
            v.relationship=="Supported source"?"Supported source":
            v.evidence_level=="Response header"?"Response header":
            v.evidence_level=="Request attempt"?"Requested, no response":"Page reference";
        InfrastructureEntry entry{name,v.category+" / "+basis,-1,static_cast<int>(i)};
        if(found==rows.end())rows.push_back(std::move(entry));
        else if(found->vendor>=0&&strength(v)>strength(report.vendors[found->vendor]))*found=std::move(entry);
    }
    for(const auto& page:report.linked_pages) {
        if(page.state!="collected")continue;
        std::string host;
        try {
            host=parse_target(page.url).host;
            if(host==parse_target(page.discovered_from).host)continue;
        } catch(const std::exception&) {continue;}
        for(auto& row:rows)
            if(std::any_of(page.first_seen.begin(),page.first_seen.end(),[&](const std::string& name){return display_name(name)==row.name;}))
                row.detail+=" on "+host;
    }
    std::stable_partition(rows.begin(),rows.end(),[&](const InfrastructureEntry& row) {
        return row.finding<0||!dns_role(report.findings[row.finding]);
    });
    return rows;
}
}
