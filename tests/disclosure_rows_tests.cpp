#include "../src/disclosure_rows.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace vectorapp;
int checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try {
    using disclosure_rows::extract;
    if(argc==3&&std::string(argv[1])=="--html") {
        std::ifstream file(argv[2],std::ios::binary);if(!file)throw std::runtime_error("Cannot read HTML");
        const std::string html{std::istreambuf_iterator<char>(file),{}};
        const auto result=extract(html);
        for(const auto& row:result.rows){std::cout<<row.heading<<" | "<<row.name<<'\n';for(const auto& f:row.fields)std::cout<<"  "<<f.first<<": "<<f.second<<'\n';}
        std::cout<<result.rows.size()<<" structured disclosure rows; truncated="<<result.truncated<<'\n';return 0;
    }
    const std::string heading="<h2>Our subprocessors</h2>";
    check(markup::entities("&amp;#x27;")=="&#x27;","entity decoding does not recursively interpret an escaped example");
    const std::string card="<article><h3>Orchid Data Ltd</h3><dl><dt>Purpose</dt><dd>Managed storage</dd><dt>Location</dt><dd>Europe</dd></dl></article>";
    auto r=extract(heading+card);
    check(r.rows.size()==1&&r.rows[0].name=="Orchid Data Ltd","heading and labeled card values");
    check(r.rows[0].heading=="Our subprocessors"&&r.rows[0].fields.size()==2,"disclosure context and fields retained");
    r=extract(heading+"<div><h3><strong>Orchid Data Ltd</strong></h3><p>Purpose: Hosting</p></div>");
    check(r.rows.size()==1,"nested name formatting does not create multiple cards");
    r=extract(heading+"<ul><li><strong>Marigold &amp; Co</strong><p>Service: Messaging</p><p>Country: Canada</p></li></ul>");
    check(r.rows.size()==1&&r.rows[0].name=="Marigold & Co","list extraction and entities");
    r=extract(heading+"<ul><li><strong>Fresh Provider, Ltd. (EU)</strong>: Queue &amp; event routing</li><li><b><a href='/vendor'>New Supplier</a></b>: User support</li></ul>");
    check(r.rows.size()==2&&r.rows[0].name=="Fresh Provider, Ltd. (EU)"&&r.rows[1].name=="New Supplier","inline list names come from visible bold labels");
    check(r.rows[0].fields.size()==1&&r.rows[0].fields[0].first=="Description"&&r.rows[0].fields[0].second=="Queue & event routing","inline description and entities retained");
    check(extract(heading+"<li>We discuss <strong>Fresh Provider</strong>: Example</li>").rows.empty(),"inline name must begin its row");
    check(extract(heading+"<li><strong>Fresh Provider</strong> provides our storage.</li>").rows.empty(),"unlabeled list prose needs a delimiter");
    check(extract(heading+"<li><strong>Fresh Provider</strong>: </li>").rows.empty(),"empty inline role rejected");
    check(extract(heading+"<li><strong>Fresh Provider</strong>: <b>Another Company</b> hosting</li>").rows.empty(),"ambiguous inline names rejected");
    check(extract(heading+"<li><strong>Service</strong>: Hosting</li>").rows.empty(),"inline field label cannot become a provider");
    check(extract(heading+"<li hidden><strong>Fresh Provider</strong>: Hosting</li>").rows.empty(),"hidden inline provider rejected");
    check(extract("<h2>Customers</h2><li><strong>Fresh Provider</strong>: Hosting</li>").rows.empty(),"customer inline list rejected");
    check(extract(heading+"<li><strong>Fresh Provider</strong>: Hosting").rows.empty(),"clipped inline row rejected");
    const std::string column_a="<div><div><p>New Cloud 83</p></div><div><p>Hosting &amp; storage</p></div></div>";
    const std::string column_b="<div><div><p>New Support 71</p></div><div><p>Messaging</p></div></div>";
    r=extract(heading+"<div>"+column_a+column_b+"</div>");
    check(r.rows.size()==2&&r.rows[0].name=="New Cloud 83"&&r.rows[1].name=="New Support 71","repeated two-column div rows supply names without classes or a name registry");
    check(r.rows[0].fields.size()==1&&r.rows[0].fields[0].second=="Hosting & storage","two-column description retains source text");
    check(extract("<header>"+heading+"<div>"+column_a+column_b+"</div></header>").rows.size()==2,"semantic header can contain a visible disclosure document");
    check(extract("<header><nav>"+heading+"<div>"+column_a+column_b+"</div></nav></header>").rows.empty(),"navigation remains excluded inside a header");
    check(extract("<header role='navigation'>"+heading+"<div>"+column_a+column_b+"</div></header>").rows.empty(),"navigation role cannot supply provider rows");
    check(extract(heading+column_a).rows.empty(),"isolated paragraph pair is not a provider row group");
    check(extract("<h2>Customers</h2><div>"+column_a+column_b+"</div>").rows.empty(),"customer grids are not provider lists");
    check(extract("<div>"+column_a+column_b+"</div>").rows.empty(),"column rows require disclosure context");
    check(extract(heading+"<div hidden>"+column_a+column_b+"</div>").rows.empty(),"hidden column group supplies no records");
    check(extract(heading+"<div>"+column_a+"<h2>Customers</h2>"+column_b+"</div>").rows.empty(),"row repetition cannot cross a heading boundary");
    check(extract(heading+"<div>"+column_a+column_b.substr(0,column_b.size()-6)+"</div>").rows.empty(),"incomplete row group cannot establish a column pattern");
    check(extract(heading+"<div><div><p>Name</p><p>Purpose</p></div><div><p>Only Name</p><p>Hosting</p></div></div>").rows.empty(),"header labels do not establish repeated provider records");
    r=extract(heading+"<div>"+column_a+column_b+"</div><h2>Customers</h2><div>"+column_a+column_b+"</div>");
    check(r.rows.size()==2,"column rows do not leak beyond a disclosure heading");
    r=extract(heading+"<dl><dt>Provider Q9</dt><dd><p>Purpose: Hosting</p></dd><dt>Provider R7</dt><dd><p>Location: Australia</p></dd></dl>");
    check(r.rows.size()==2&&r.rows[1].name=="Provider R7","definition lists keep separate providers");
    r=extract(heading+"<div><p>Company: Silverbirch</p><p>Service: Support</p></div>");
    check(r.rows.size()==1&&r.rows[0].name=="Silverbirch","explicit company label works without bold styling");
    r=extract(heading+"<div><p>Company: Unseen UUID 93fa</p><p>Purpose: Queue management</p></div>");
    check(r.rows.size()==1&&r.rows[0].name=="Unseen UUID 93fa","names are extracted without a product catalog");
    const std::string logo="<div><div><img alt='Pine Systems'></div><div><div>Pine Systems<span> · EU</span></div><div>Storage infrastructure services.</div></div></div>";
    r=extract(heading+logo);
    check(r.rows.size()==1&&r.rows[0].name=="Pine Systems","logo alt must match visible card name");
    check(r.rows[0].fields[0].second=="Storage infrastructure services.","unlabeled card preserves description text");
    r=extract("<button role='tab' id='chosen' aria-selected='true'>Subprocessors</button><div role='tabpanel' aria-labelledby='chosen'>"+logo+"</div>");
    check(r.rows.size()==1&&r.rows[0].name=="Pine Systems","visible accessible tab label establishes disclosure context");
    check(extract("<button role='tab' id='chosen'>Customers</button><div role='tabpanel' aria-labelledby='chosen'>"+logo+"</div>").rows.empty(),"customer tab does not establish provider context");
    check(extract("<button hidden id='chosen'>Subprocessors</button><div role='tabpanel' aria-labelledby='chosen'>"+logo+"</div>").rows.empty(),"hidden label cannot qualify a panel");
    check(extract("<button id='chosen'>Subprocessors</button><div hidden role='tabpanel' aria-labelledby='chosen'>"+logo+"</div>").rows.empty(),"hidden disclosure tab cannot supply cards");
    r=extract("<div role='region' aria-label='Service providers'>"+logo+"</div>"+logo);
    check(r.rows.size()==1,"accessible section boundary does not leak into later cards");
    for(const auto* wrapper:{"script","style","template","noscript","nav","footer","pre","code"})
        check(extract("<"+std::string(wrapper)+">"+heading+card+"</"+wrapper+">").rows.empty(),"inactive or irrelevant containers ignored");
    for(const auto* attribute:{"hidden","aria-hidden='true'","style='display: none'","style='visibility: hidden'"})
        check(extract("<div "+std::string(attribute)+">"+heading+card+"</div>").rows.empty(),"hidden disclosure rows ignored");
    check(extract("<!--"+heading+card+"-->").rows.empty(),"comment examples ignored");
    check(extract("<h2>Customers</h2>"+card).rows.empty(),"customer cards not providers");
    check(extract("<p>Our subprocessors include these organizations.</p>"+card).rows.empty(),"prose does not qualify provider section");
    check(extract("<h2>Subprocessor updates</h2>"+card).rows.empty(),"historical update lists not current disclosure");
    check(extract(heading+"<p>We use Alpine for hosting.</p>").rows.empty(),"ordinary provider mention not a structured row");
    check(extract(heading+"<ul><li>Hosting</li><li>Analytics</li></ul>").rows.empty(),"generic purpose list not provider names");
    check(extract(heading+"<article><h3>Purpose</h3><p>Country: UK</p></article>").rows.empty(),"field labels never become provider names");
    check(extract(heading+"<article><h3>Logo only</h3><p>An example mention.</p></article>").rows.empty(),"unlabeled prose card rejected");
    check(extract(heading+"<div><img alt='Pine'><div><div>Another name</div><p>Storage infrastructure services.</p></div></div>").rows.empty(),"mismatched logo rejected");
    r=extract(heading+card+"<h2>Customers</h2>"+card);
    check(r.rows.size()==1,"a later heading ends disclosure context");
    r=extract("<section>"+heading+card+"</section>"+card);
    check(r.rows.size()==1,"section boundary ends disclosure context");
    r=extract(heading+"<div>"+card+card+"</div>");
    check(r.rows.size()==1,"duplicate visible source rows deduplicated");
    r=extract(heading+card+"<article><h3>Orchid Data Ltd</h3><p>Purpose: Support</p></article>");
    check(r.rows.size()==2,"same provider retains different source purposes");
    r=extract(heading+"<div><div><h3>Same Co</h3><p>Purpose: Hosting</p></div><div><h3>Same Co</h3><p>Purpose: Support</p></div></div>");
    check(r.rows.size()==2&&r.rows[0].fields.size()==1&&r.rows[1].fields.size()==1,"wrapper cannot merge distinct cards for the same name");
    const auto alternate=logo.substr(0,logo.find("Storage infrastructure services."))+"Customer messaging infrastructure."+logo.substr(logo.find("Storage infrastructure services.")+std::string("Storage infrastructure services.").size());
    r=extract(heading+"<div>"+logo+alternate+"</div>");
    check(r.rows.size()==2,"repeated logo cards retain different purposes");
    check(extract(heading+"<div><h2>Customers</h2>"+logo+"</div>").rows.empty(),"outer card wrapper cannot bypass a later customer heading");
    check(extract(heading+"<article><h3>Clipped Name</h3><p>Purpose: Hosting</p>").rows.empty(),"unclosed row cannot emit a provider");
    check(extract(heading+"<div><p>Company: Left</p><p>Company: Right</p><p>Purpose: Hosting</p></div>").rows.empty(),"ambiguous named row rejected");
    check(extract(heading+"<table><tr><td>Company: Table row</td><td>Purpose: Hosting</td></tr></table>").rows.empty(),"table parser remains the table owner");
    r=extract("<script>const x='</scripture>"+heading+card+"';</script>"+heading+card);
    check(r.rows.size()==1,"raw text closing boundaries respected");
    std::string many=heading;for(int i=0;i<160;++i)many+="<article><h3>Fresh Name "+std::to_string(i)+"</h3><p>Purpose: Hosting</p></article>";
    r=extract(many);check(r.rows.size()==128&&r.truncated,"provider cap is explicit");
    r=extract(heading+"<article><h3>Orchid\xC2\xA0Ltd</h3><p>Purpose: EU&nbsp;hosting</p></article>");
    check(r.rows.size()==1&&r.rows[0].name=="Orchid Ltd"&&r.rows[0].fields[0].second=="EU hosting","nonbreaking whitespace normalized");
    r=extract(std::string(3*1024*1024,' ')+heading+card);
    check(r.rows.empty()&&r.truncated,"body bound stops extraction beyond collection limit");
    std::cout<<checks<<" structured disclosure checks passed.\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
