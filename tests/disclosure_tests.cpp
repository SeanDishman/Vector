#include "vector/model.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace vectorapp;
int checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
const std::string table="<h1>Our Service Partners</h1><table><tr><th>Company</th><th>Country/Region</th></tr><tr><td>Acme &amp; Co</td><td>Canada</td></tr><tr><td>Example Cloud</td><td>France</td></tr></table>";
ScanReport inspect(const std::string& body,long status=200,const std::string& url="https://privacy.example.test/subprocessors") {
    ScanReport r;r.target=parse_target("https://example.test");HttpObservation h;h.url=url;h.body=body;h.status=status;
    identify_disclosures(h,r);return r;
}
int main(int argc,char** argv){try{
    if(argc==5&&std::string(argv[1])=="--document") {
        std::ifstream input(argv[2],std::ios::binary);if(!input)throw std::runtime_error("Cannot read retained disclosure document");
        HttpObservation page;page.body=std::string(std::istreambuf_iterator<char>(input),{});page.url=argv[3];page.status=200;
        ScanReport document;document.target=parse_target(argv[4]);identify_disclosures(page,document);
        std::cout<<report_json(document);return 0;
    }
    auto r=inspect(table);check(r.vendors.size()==2,"actual provider rows extracted");
    check(r.vendors[0].name=="Acme & Co"&&r.vendors[0].published_by=="example.test","exact published name and publisher");
    check(r.vendors[0].evidence_level=="Published disclosure"&&r.findings.empty(),"published organizations never become observed software");
    check(r.vendors[0].evidence[0].value.find("Country/Region: Canada")!=std::string::npos&&!r.vendors[0].recorded_at.empty(),"row and collection date retained");
    const auto escaped=inspect("<h1>Subprocessors</h1><table><tr><th>Company</th><th>Purpose</th></tr><tr><td>Unseen&#x27;s Tools</td><td>Publisher&#X27;s event delivery</td></tr></table>");
    check(escaped.vendors.size()==1&&escaped.vendors[0].name=="Unseen's Tools","hexadecimal HTML apostrophe retained as visible name text");
    check(escaped.vendors[0].evidence[0].value.find("Purpose: Publisher's event delivery")!=std::string::npos,"hexadecimal apostrophe decoded in supporting fields");
    HttpObservation duplicate;duplicate.url="https://privacy.example.test/service-partners";duplicate.status=200;duplicate.body=table;
    check(identify_disclosures(duplicate,r)==2&&r.vendors.size()==2&&r.vendors[0].evidence.size()==2,"a second supporting source counts matched providers without duplicate rows");
    check(identify_disclosures(duplicate,r)==2&&r.vendors[0].evidence.size()==2,"repeat source does not duplicate evidence");
    for(const auto& wrapper: {std::pair{"<!--","-->"},std::pair{"<script>const example=`","`;</script>"},std::pair{"<template>","</template>"},std::pair{"<nav>","</nav>"},std::pair{"<div hidden>","</div>"},std::pair{"<div aria-hidden='true'>","</div>"},std::pair{"<div style='display: none'>","</div>"},std::pair{"<pre>","</pre>"}})
        check(inspect(wrapper.first+table+wrapper.second).vendors.empty(),"inactive or example text cannot create vendors");
    check(inspect(table,403).vendors.empty(),"HTTP rejection cannot supply current disclosure rows");
    check(inspect(table,200,"https://privacy.example.test.evil.test/subprocessors").vendors.empty(),"lookalike publisher rejected");
    check(inspect(table,200,"http://privacy.example.test/subprocessors").vendors.empty(),"related-host HTTP downgrade rejected");
    check(inspect("<h1>Our customers</h1>"+table.substr(table.find("<table>"))).vendors.empty(),"customer table not a vendor disclosure");
    check(inspect("<h1>Subprocessors</h1><p>We mention React, Slack and Stripe here.</p>").vendors.empty(),"ordinary prose not matched");
    check(inspect("<h1>Subprocessors</h1><table><tr><th>Software</th><th>Country</th></tr><tr><td>React</td><td>US</td></tr></table>").vendors.empty(),"unqualified table columns rejected");
    check(inspect("<h1>Service providers</h1><table><tr><th>Service provider</th></tr><tr><td>Acme</td></tr></table>").vendors.empty(),"provider column is not its own supporting context column");
    const std::string nested_table="<table><tr><th>Entity</th><th>Location</th><th>Use</th></tr><tr><td>Unseen Entity 83</td><td>Canada</td><td>Message delivery</td></tr></table>";
    r=inspect("<h1>Published subprocessors</h1><h2>Entity Name &amp; Location</h2>"+nested_table);
    check(r.vendors.size()==1&&r.vendors[0].name=="Unseen Entity 83","neutral lower heading retains explicit provider parent");
    check(r.vendors[0].evidence[0].value.find("Published subprocessors / Entity Name & Location")!=std::string::npos,"parent and detail headings retained in evidence");
    check(inspect("<h1>Subprocessors</h1><h2></h2>"+nested_table).vendors.size()==1,"empty formatting heading does not erase disclosure context");
    check(inspect("<h1>Subprocessors</h1><h1>Entity Name &amp; Location</h1>"+nested_table).vendors.empty(),"same-level heading ends provider scope");
    check(inspect("<h1>Customers</h1><h2>Entity Name &amp; Location</h2>"+nested_table).vendors.empty(),"neutral heading cannot create provider context");
    check(inspect("<h1>Subprocessors</h1><h2>Our customers</h2>"+nested_table).vendors.empty(),"customer subsection resets disclosure context");
    check(inspect("<h1>Subprocessors</h1><h2>Example names</h2>"+nested_table).vendors.empty(),"examples do not inherit disclosure context");
    check(inspect("<h1>Subprocessors</h1><h2>Customers</h2><h3>Entity Name &amp; Location</h3>"+nested_table).vendors.empty(),"neutral subheading cannot revive an ended disclosure section");
    for(const auto* column:{"Entity","Sub-processor Name","Subprocessor name","Vendor name","Provider name","Service provider name"}) {
        const auto rows=std::string("<table><tr><th>")+column+"</th><th>Purpose for Processing</th><th>Entity Country</th></tr><tr><td>Unseen Supplier 27</td><td>Event delivery</td><td>Norway</td></tr></table>";
        auto names=inspect("<h1>Subprocessors</h1>"+rows);
        check(names.vendors.size()==1&&names.vendors[0].name=="Unseen Supplier 27","common name-header forms extract arbitrary entities");
        check(names.vendors[0].evidence[0].value.find(std::string(column)+": Unseen Supplier 27")!=std::string::npos,"original name-header label retained");
        check(inspect("<h1>Customers</h1>"+rows).vendors.empty(),"name-header aliases still require disclosure context");
    }
    r=inspect("<script>const t='</scripture>"+table+"';</script>"+table);
    check(r.vendors.size()==2,"raw-text closing boundary respected");
    r=inspect("<h1>Subprocessors</h1><table><tr><th>Name</th><th>Purpose</th></tr><tr><td>Acme</td><td>Hosting</td></tr></table>");
    check(r.vendors.size()==1&&r.vendors[0].relationship=="Subprocessor","subprocessor role retained");
    HttpObservation page;page.url="https://example.test/";
    page.body="<a href='/subprocessors'>Partners</a><a href='https://privacy.example.test/service-providers'>Legal</a><a href='https://evil.test/subprocessors'>Bad</a><script>\"<a href='/subprocessors-fake'>Fake</a>\"</script>";
    check(disclosure_links(page,parse_target(page.url)).size()==2,"bounded same-company source discovery, no external or script links");
    const auto j=nlohmann::json::parse(report_json(r));
    check(j["vendors"][0]["access_status"]=="unverified"&&j["vendors"][0]["published_by"]=="example.test","export retains observed publisher and access limit");
    std::string many="<h1>Subprocessors</h1><table><tr><th>Company</th><th>Purpose</th></tr>";
    for(int i=0;i<400;++i)many+="<tr><td>Provider "+std::to_string(i)+"</td><td>Hosting</td></tr>";
    r=inspect(many+"</table>");check(r.vendors.size()==128,"bounded provider extraction");
    const std::string entity_table="<table><tr><th colspan='3'>Acme Affiliates</th></tr><tr><td>Entity name</td><td>Relevant Service(s)</td><td>Activity</td></tr><tr><td>Acme Support Ltd</td><td>Workspace Support</td><td>Technical Support</td></tr></table>";
    r=inspect("<h4>Third-Party Subprocessors</h4>"+entity_table);
    check(r.vendors.size()==1&&r.vendors[0].relationship=="Subprocessor","entity header after caption and h4 heading accepted");
    check(r.vendors[0].evidence[0].value.find("Relevant Service(s): Workspace Support")!=std::string::npos&&r.vendors[0].evidence[0].value.find("Table context: Acme Affiliates")!=std::string::npos,"service scope and table caption retained");
    r=inspect("<strong>Google Group Subprocessors</strong>"+entity_table);
    check(r.vendors.size()==1&&r.vendors[0].relationship=="Group subprocessor","group affiliate retains first-party relationship");
    r=inspect("<strong>Affiliates of Third-Party Subprocessors</strong>"+entity_table);
    check(r.vendors.size()==1&&r.vendors[0].relationship=="Subprocessor affiliate","provider affiliate role retained");
    check(inspect("<h4>Our customers</h4>"+entity_table).vendors.empty(),"entity column alone cannot qualify customers as providers");
    check(inspect("<p>We work with third-party subprocessors.</p>"+entity_table).vendors.empty(),"ordinary prose cannot qualify a table");
    check(inspect("<h4>Subprocessors</h4><table><tr><th>Entity name</th><th>Relevant Service(s)</th></tr><tr><td>ENTITY NAME</td><td>Services</td></tr></table>").vendors.empty(),"repeated column headers never become provider names");
    r=inspect("<h4>Subprocessors</h4><table><tr><th>Entity name</th><th>Purpose</th></tr><tr><td>Acme\xC2\xA0Ltd\xC2\xA0</td><td><p>Technical Support</p><p>Data Labeling</p></td></tr></table>");
    check(r.vendors.size()==1&&r.vendors[0].name=="Acme Ltd","literal nonbreaking spaces normalize before provider deduplication");
    check(r.vendors[0].evidence[0].value.find("Technical Support Data Labeling")!=std::string::npos,"paragraph boundaries preserve separate activities in a table cell");
    r=inspect("<h4>Third-Party Subprocessors</h4>"+entity_table+"<h4>Customers</h4>"+entity_table);
    check(r.vendors[0].evidence.size()==1,"later non-provider heading resets table context");
    std::cout<<checks<<" disclosure checks passed.\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
