#include "vector/export.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace vectorapp;
namespace {
int checks=0;
void check(bool passed,const char* reason) {
    ++checks;if(!passed)throw std::runtime_error(reason);
}
std::string utf8_path(const std::filesystem::path& path) {
    const auto value=path.u8string();return {value.begin(),value.end()};
}
}
int main(int argc,char** argv) {
    try {
        const auto directory=argc>1?std::filesystem::path(std::u8string(argv[1],argv[1]+std::char_traits<char>::length(argv[1]))):
            std::filesystem::temp_directory_path()/"vector-export-tests";
        std::filesystem::create_directories(directory);
        auto report=demo_report();
        report.reverse_dns="fixture.example.test";
        report.notes.push_back("Commas, \"quotes\", CR\rLF\nTab\t and UTF-8: caf\xc3\xa9 \xe6\x9d\xb1\xe4\xba\xac");
        report.notes.push_back("=HYPERLINK(\"https://example.invalid\")");
        report.notes.push_back("+formula");report.notes.push_back("-formula");report.notes.push_back("@formula");
        report.notes.push_back("\\literal\\n and an empty string follow");report.notes.push_back("");
        report.notes.push_back("embedded NUL: "+std::string("a\0b",3));
        report.findings.push_back({"Fixture library","Library","1.2.3","High",{{"runtime","quoted \"value\"\nnext line"}}});
        report.web[0].certificate={{"subject","CN=fixture.example.test"},{"issuer","Local fixture"}};
        report.scripts.push_back({"http://fixture.test/module.js","http://fixture.test/","partial","body limit",200,2048,true,true,"browser"});
        report.browser.state="complete";report.browser.engine="Fixture";
        report.browser.pages.push_back({"http://fixture.test/","http://fixture.test/","complete","fixture","Test page",200,123,true,true,1,"captured_http"});
        report.browser.requests.push_back({"http://fixture.test/module.js","http://fixture.test/","loader.js","Script","GET","complete","","127.0.0.1","text/javascript",200,1234,"network"});
        report.browser.requests_truncated=true;
        report.web_coverage.started=true;report.web_coverage.limited=true;report.web_coverage.page_requests=12;
        report.web_coverage.reasons={"Page request limit reached"};
        report.disclosures.push_back({"https://fixture.test/subprocessors","Providers","reference","Dated fixture","2026-01-01",0,1,"https://fixture.test/subprocessors"});
        VendorRelationship provider;provider.name="Fixture provider";provider.evidence_level="Published reference";
        provider.published_by="Fixture publisher";provider.recorded_at="2026-01-01";
        provider.evidence={{"https://fixture.test/subprocessors","Provider: Fixture provider | Scope: example product"}};
        report.vendors.push_back(provider);
        ScanEvent event;event.item="fixture operation";event.detail="kept activity evidence";report.activity.push_back(event);
        const auto canonical=nlohmann::json::parse(report_json(report));
        check(canonical.at("reverse_dns")==report.reverse_dns,"reverse DNS must export with endpoint details");
        for(const auto format:{ReportFormat::Json,ReportFormat::Csv,ReportFormat::Txt}) {
            const auto expected=report_export(report,format);
            const auto path=directory/(std::string("complete")+report_extension(format));
            save_report(report,utf8_path(path),format);
            std::ifstream file(path,std::ios::binary);const std::string saved{std::istreambuf_iterator<char>(file),{}};
            check(saved==expected,"saved format must match complete serialization");
            check(saved.find("Fixture provider")!=std::string::npos,"published evidence retained");
            check(saved.find("kept activity evidence")!=std::string::npos,"activity evidence retained");
            check(saved.find("body limit")!=std::string::npos,"collection limits retained");
            bool failed=false;
            try { save_report(report,utf8_path(directory),format); } catch(const std::exception&) { failed=true; }
            check(failed,"write failure must not report success");
        }
        check(nlohmann::json::parse(report_export(report,ReportFormat::Json))==canonical,"JSON export schema unchanged");
        save_report(report,utf8_path(directory/"legacy.json"));
        std::ifstream legacy(directory/"legacy.json");
        check(nlohmann::json::parse(legacy)==canonical,"existing JSON save API remains compatible");
        const auto unicode_path=directory/std::filesystem::path(u8"rapport-caf\u00e9.txt");
        save_report(report,utf8_path(unicode_path),ReportFormat::Txt);
        check(std::filesystem::file_size(unicode_path)>0,"Unicode paths work");
        report.status="cancelled";report.error="Partial fixture";
        for(const auto format:{ReportFormat::Json,ReportFormat::Csv,ReportFormat::Txt})
            save_report(report,utf8_path(directory/(std::string("partial")+report_extension(format))),format);
        auto empty=ScanReport{};empty.status="error";empty.error="No responses collected";
        for(const auto format:{ReportFormat::Json,ReportFormat::Csv,ReportFormat::Txt})
            save_report(empty,utf8_path(directory/(std::string("empty")+report_extension(format))),format);
        std::cout<<checks<<" export checks passed.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n';return 1; }
}
