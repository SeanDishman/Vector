#include "vector/model.hpp"
#include <csignal>
#include <iostream>
#include <stdexcept>

namespace { std::atomic_bool cancelled{false}; void stop(int) { cancelled.store(true); } }
int main(int argc,char** argv) {
    using namespace vectorapp;
    try {
        ScanOptions options;
        std::string target,output;
        bool demo=false;
        for (int i=1;i<argc;++i) {
            std::string a=argv[i];
            auto value=[&]() { if (++i>=argc) throw std::invalid_argument("Missing value for " + a); return std::string(argv[i]); };
            if (a=="--target") target=value();
            else if (a=="--ports") options.ports=parse_ports(value());
            else if (a=="--timeout") options.timeout_ms=std::stoi(value());
            else if (a=="--http-timeout") options.http_timeout_ms=std::stoi(value());
            else if (a=="--output") output=value();
            else if (a=="--web-only") options.web_only=true;
            else if (a=="--proxy") options.proxy=parse_proxy(value());
            else if (a=="--proxy-user") options.proxy.username=value();
            else if (a=="--proxy-password") options.proxy.password=value();
            else if (a=="--no-browser") options.browser_runtime=false;
            else if (a=="--browser-timeout") options.browser_timeout_ms=std::stoi(value());
            else if (a=="--allow-untrusted-tls") options.allow_untrusted_tls=true;
            else if (a=="--demo") demo=true;
            else if (a=="--help" || a=="-h") {
                std::cout << "Vector 0.1 - host and software fingerprinting\n\n"
                    "vector-cli --target <IP|host|URL> [--ports 22,80,443] [--output report.json]\n"
                    "  --timeout N        TCP connect timeout in ms (100-10000; default 1600)\n"
                    "  --http-timeout N   HTTP request timeout in ms (100-30000; default 6000)\n"
                    "  --web-only         Inspect the target's HTTP(S) port only\n"
                    "  --proxy ADDRESS    Proxy as host:port; its type is detected. A http://, https:// or socks5:// prefix forces one\n"
                    "  --proxy-user USER  Proxy username; --proxy-password PASS supplies its password\n"
                    "  --no-browser       Static collection only; do not execute page JavaScript\n"
                    "  --browser-timeout N  Browser observation window in ms (1000-30000; default 12000)\n"
                    "  --allow-untrusted-tls  Inspect self-signed HTTPS; marks peer unverified\n"
                    "  --demo             Print an illustrative offline report\n"
                    "Bare IPs check TCP 1-1024 plus common higher ports (32 connections at once).\n"
                    "Hostnames use 26 common TCP ports; explicit URLs/ports select that endpoint.\n"
                    "Custom port lists stay limited to 256 ports. TCP selection is exported.\n"
                    "Use on systems you own or have permission to inspect. Ctrl+C cancels.\n";
                return 0;
            } else if (!a.starts_with('-') && target.empty()) target=a;
            else throw std::invalid_argument("Unknown argument: " + a);
        }
        std::signal(SIGINT,stop);
        auto report=demo ? demo_report() : scan(parse_target(target),options,cancelled,[](float,const std::string& text) { std::cerr << text << '\n'; });
        if (!output.empty()) save_report(report,output);
        std::cout << report_json(report) << '\n';
        return report.status=="error" ? 1 : report.status=="cancelled" ? 130 : 0;
    } catch (const std::exception& e) { std::cerr << "Vector: " << e.what() << '\n'; return 2; }
}
