#include "vector/model.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
using namespace vectorapp;
int checks = 0;
void check(bool value,const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
int main(int argc,char** argv) {
    try {
        if (argc==3 && std::string(argv[1])=="--cancel") {
            const auto target=parse_target(argv[2]);
            if (!target.is_ip || !(target.host.starts_with("127.") || target.host=="::1"))
                throw std::runtime_error("Cancellation fixture requires loopback");
            std::atomic_bool cancel{false},waiting{false},finished{false};
            std::thread stop([&] {
                while (!waiting && !finished) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                if (!finished) { std::this_thread::sleep_for(std::chrono::milliseconds(50));cancel=true; }
            });
            ScanOptions options;options.browser_runtime=false;options.timeout_ms=1000;options.http_timeout_ms=100;
            auto r=scan(target,options,cancel,{},[&](const ScanEvent& e) {
                if (e.stage==ScanStage::Services && e.detail=="Connected; waiting for a service banner") waiting=true;
            });
            finished=true;stop.join();
            std::cout<<report_json(r)<<'\n';
            return r.status=="cancelled"?130:1;
        }
        const auto deep=plan_tcp_scan(parse_target("192.0.2.12"),{});
        check(deep.profile=="deep_ip" && deep.max_concurrent==32,"bare IPv4 selects the deeper profile");
        const std::set<uint16_t> ports(deep.selected_ports.begin(),deep.selected_ports.end());
        check(ports.size()==deep.selected_ports.size(),"each selected port is checked once");
        check(deep.selected_ports.front()==21,"common services precede the low-port sweep");
        bool low_range=true;
        for (int p=1;p<=1024;++p) low_range=low_range && ports.contains(static_cast<uint16_t>(p));
        check(low_range,"entire low-port range covered");
        for (int p:{1080,1433,3306,3389,5432,5672,6379,8443,9100,11211,27017,50000})
            check(ports.contains(static_cast<uint16_t>(p)),"higher service port covered");
        check(ports.size()>1024 && ports.size()<1200 && !ports.contains(65535),"automatic scan is bounded, not all 65535 ports");
        for (const auto* input:{"::1","[::1]","2001:db8::1234"})
            check(plan_tcp_scan(parse_target(input),{}).selected_ports==deep.selected_ports,"bare IPv6 gets the same coverage");
        const auto common=plan_tcp_scan(parse_target("localhost"),{});
        check(common.profile=="common" && common.selected_ports==default_ports() && common.max_concurrent==8,"hostname stays on common ports");
        for (const auto* input:{"https://192.0.2.12/","http://192.0.2.12/","https://[::1]/","192.0.2.12:6379","[::1]:6379","https://example.test/","example.test:8080","192.0.2.12/app"}) {
            const auto target=parse_target(input);const auto plan=plan_tcp_scan(target,{});
            check(plan.profile=="endpoint" && plan.selected_ports==std::vector<uint16_t>{target.port} && plan.max_concurrent==8,"explicit endpoint stays scoped");
        }
        ScanOptions options;options.web_only=true;
        check(plan_tcp_scan(parse_target("192.0.2.12"),options).selected_ports==std::vector<uint16_t>{80},"web-only overrides bare IP");
        options.ports={6379,81,6379};
        const auto custom=plan_tcp_scan(parse_target("https://192.0.2.12/"),options);
        check(custom.profile=="custom" && custom.selected_ports==std::vector<uint16_t>({81,6379}) && custom.max_concurrent==8,"custom list wins and deduplicates");
        options.ports={0};bool rejected=false;
        try {plan_tcp_scan(parse_target("192.0.2.12"),options);} catch (const std::invalid_argument&) {rejected=true;}
        check(rejected,"programmatic zero port rejected");
        options.ports.assign(257,80);rejected=false;
        try {plan_tcp_scan(parse_target("192.0.2.12"),options);} catch (const std::invalid_argument&) {rejected=true;}
        check(rejected,"custom cap remains 256 ports");
        ScanReport report;report.target=parse_target("192.0.2.12");report.status="cancelled";report.tcp_coverage=deep;report.tcp_coverage.completed=5;
        const auto exported=nlohmann::json::parse(report_json(report)).at("tcp_coverage");
        check(exported.at("profile")=="deep_ip" && exported.at("planned")==ports.size() && exported.at("completed")==5 && exported.at("max_concurrent")==32,"partial report retains exact selection and completed count");
        check(exported.at("selected_ports").get<std::vector<uint16_t>>()==deep.selected_ports,"all selected ports are exported");
        std::cout<<checks<<" port-selection checks passed. Deep IP ports: "<<ports.size()<<".\n";
        return 0;
    } catch (const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
