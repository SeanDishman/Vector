#include "vector/model.hpp"
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int main(){try {
    int checks=0;
    const auto check=[&](bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);};
    auto subject=parse_target("https://new-company.test");
    HttpObservation page;page.url=subject.url();page.status=200;
    page.body="<a href='/legal'>Legal</a><a aria-label='Open to Subprocessors' href='/documents?id=72'>Open</a>";
    auto links=disclosure_candidates(page,subject);
    check(links.size()==2&&links.front().priority==3,"accessible label identifies an opaque source and outranks intermediate pages");
    check(links.front().url.find("id=72")!=std::string::npos&&links.front().depth==1&&links.front().chain.size()==1,"source URL and link provenance retained");
    auto parent=links.back();page.url=parent.url;
    page.body="<a href='https://hosted-center.test/org/517'>Trust center</a><a href='https://unrelated.test/subprocessors'>News</a>";
    links=disclosure_candidates(page,subject,&parent);
    check(links.size()==1&&links.front().url.find("hosted-center.test")!=std::string::npos,"explicit trust link authorizes hosted source but URL keyword alone does not");
    check(links.front().depth==2&&links.front().chain.size()==2,"multi-host source chain retained");
    parent=links.front();page.url=parent.url;page.body="<a href='/opaque' title='Subprocessor list'>Read</a>";
    links=disclosure_candidates(page,subject,&parent);
    check(links.size()==1&&links.front().discovered_from==page.url,"hosted trust source can lead to its own provider document");
    parent.depth=4;check(disclosure_candidates(page,subject,&parent).empty(),"discovery depth bounded");
    page.url=subject.url();const std::string anchor="<a href='/subprocessors'>Subprocessors</a>";
    for(const auto& pair:{std::pair{"<!--","-->"},std::pair{"<script>","</script>"},std::pair{"<template>","</template>"},std::pair{"<pre>","</pre>"},std::pair{"<div hidden>","</div>"},std::pair{"<div aria-hidden='true'>","</div>"}}) {
        page.body=pair.first+anchor+pair.second;check(disclosure_candidates(page,subject).empty(),"hidden/examples cannot initiate disclosure requests");
    }
    page.body="<footer>"+anchor+anchor+"</footer>";check(disclosure_candidates(page,subject).size()==1,"visible footer links deduplicate");
    page.status=403;check(disclosure_candidates(page,subject).empty(),"rejected documents cannot grant discovery scope");page.status=200;
    page.headers={{"content-type","application/pdf"}};check(disclosure_candidates(page,subject).empty(),"non-HTML response cannot supply source links");page.headers.clear();
    page.url="https://unlinked.test";check(disclosure_candidates(page,subject).empty(),"unlinked origin cannot seed a company discovery chain");page.url=subject.url();
    page.body="<a href='http://new-company.test/privacy'>Privacy</a><a href='https://127.0.0.1/subprocessors'>Subprocessors</a>";
    check(disclosure_candidates(page,subject).empty(),"downgrades and unrelated IP links rejected");
    ScanReport report;report.target=subject;page.url="https://hosted-center.test/opaque";
    page.body="<h1>Subprocessors</h1><table><tr><th>Name</th><th>Purpose</th></tr><tr><td>Newly Named Provider</td><td>Storage</td></tr></table>";
    check(identify_disclosures(page,report)==0,"unlinked external source has no company attribution");
    parent.url=page.url;parent.depth=2;parent.chain={{subject.url(),"Link: Trust center -> "+page.url}};
    check(identify_disclosures(page,report,&parent)==1&&report.vendors.front().evidence.front().value.find("Discovery:")!=std::string::npos,"linked external provider retains attribution chain");
    check(report.findings.empty(),"published rows are not software findings");
    std::cout<<checks<<" source discovery checks passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
