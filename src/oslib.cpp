#include "vector/model.hpp"
#include <regex>
#include <string>
#include <string_view>

namespace vectorapp {
namespace {
void hit(std::vector<Finding>& all, const std::string& source, std::string name, std::string category,
         std::string version, const char* confidence, const std::string& evidence) {
    add_finding(all, Finding{std::move(name), std::move(category), std::move(version), confidence, {{source, printable(evidence, 300)}}});
}

std::string canonical_library(std::string package) {
    package = lower(std::move(package));
    if (package == "jquery") return "jQuery";
    if (package == "lodash" || package == "lodash-es" || package == "lodash.core") return "lodash";
    if (package == "axios") return "axios";
    if (package == "leaflet") return "Leaflet";
    if (package == "three") return "three.js";
    if (package == "chart.js" || package == "chartjs") return "Chart.js";
    if (package == "socket.io" || package == "socket.io-client") return "Socket.IO";
    if (package == "moment") return "Moment.js";
    if (package == "dayjs") return "Day.js";
    if (package == "swiper") return "Swiper";
    if (package == "gsap") return "GSAP";
    if (package == "core-js") return "core-js";
    if (package == "datatables" || package == "datatables.net") return "DataTables";
    if (package == "ckeditor" || package == "ckeditor4" || package == "ckeditor5") return "CKEditor";
    if (package == "tinymce") return "TinyMCE";
    if (package == "font-awesome" || package == "fontawesome" || package == "@fortawesome/fontawesome-free") return "Font Awesome";
    if (package == "popper.js" || package == "@popperjs/core") return "Popper";
    if (package == "bootstrap") return "Bootstrap";
    if (package == "vue" || package == "vue.js") return "Vue.js";
    if (package == "react" || package == "react-dom") return "React";
    if (package == "alpinejs") return "Alpine.js";
    if (package == "htmx.org" || package == "htmx") return "HTMX";
    if (package == "angular") return "AngularJS";
    return {};
}

void parenthetical_os(const std::string& inside, const std::string& source, const std::string& evidence, std::vector<Finding>& all) {
    const auto text = lower(inside);
    auto add = [&](const char* name) { hit(all, source, name, "OS hint", {}, "Medium", evidence); };
    if (text.find("ubuntu") != std::string::npos) add("Ubuntu Linux");
    if (text.find("debian") != std::string::npos) add("Debian Linux");
    if (text.find("win32") != std::string::npos || text.find("win64") != std::string::npos) add("Windows");
    if (text.find("centos") != std::string::npos) add("CentOS Linux");
    if (text.find("red hat") != std::string::npos || text.find("redhat") != std::string::npos) add("Red Hat Enterprise Linux");
    if (text.find("fedora") != std::string::npos) add("Fedora Linux");
    if (text.find("rocky") != std::string::npos) add("Rocky Linux");
    if (text.find("almalinux") != std::string::npos || text.find("alma linux") != std::string::npos) add("AlmaLinux");
    if (text.find("freebsd") != std::string::npos) add("FreeBSD");
    if (text.find("alpine") != std::string::npos) add("Alpine Linux");
    if (text.find("raspbian") != std::string::npos) add("Raspberry Pi OS");
    if (text.find("amazon") != std::string::npos) add("Amazon Linux");
}

void versioned_path(const std::string& text, const std::string& source, std::vector<Finding>& all) {
    static const std::regex cdn(
        R"((?:cdnjs\.cloudflare\.com/ajax/libs/([A-Za-z0-9_.@/-]+)/([0-9]+\.[0-9][0-9A-Za-z.-]*)|cdn\.jsdelivr\.net/npm/(@?[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)?)@([0-9]+\.[0-9][0-9A-Za-z.-]*)|unpkg\.com/(@?[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)?)@([0-9]+\.[0-9][0-9A-Za-z.-]*)))",
        std::regex::icase);
    for (auto it = std::sregex_iterator(text.begin(), text.end(), cdn); it != std::sregex_iterator(); ++it) {
        std::string package, version;
        for (int i = 1; i <= 5; i += 2) {
            if ((*it)[i].matched) { package = (*it)[i].str(); version = (*it)[i + 1].str(); break; }
        }
        auto name = canonical_library(package);
        if (!name.empty()) hit(all, source, std::move(name), "Library", version, "High", it->str());
    }
    struct FileRule { const char *name, *pattern; const char *confidence; };
    static const FileRule files[] = {
        {"lodash", R"((?:^|[/@])lodash(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"axios", R"((?:^|[/@])axios(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"Leaflet", R"((?:^|[/@])leaflet(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"three.js", R"((?:^|[/@])three(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "Medium"},
        {"Chart.js", R"((?:^|[/@])chart(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "Medium"},
        {"Socket.IO", R"((?:^|[/@])socket\.io(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"Moment.js", R"((?:^|[/@])moment(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"Day.js", R"((?:^|[/@])dayjs(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"Swiper", R"((?:^|[/@])swiper(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:-bundle)?(?:\.min)?\.js)", "High"},
        {"GSAP", R"((?:^|[/@])gsap(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"core-js", R"((?:^|[/@])core-js(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "Medium"},
        {"DataTables", R"((?:^|[/@])datatables(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"CKEditor", R"((?:^|[/@])ckeditor(?:[.-]([0-9]+\.[0-9]+(?:\.[0-9]+)?))?(?:\.min)?\.js)", "High"},
        {"TinyMCE", R"((?:^|[/@])tinymce(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "High"},
        {"Font Awesome", R"((?:^|[/@])font-?awesome(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.(?:js|css))", "High"},
        {"Popper", R"((?:^|[/@])popper(?:[.-]([0-9]+\.[0-9]+\.[0-9]+))?(?:\.min)?\.js)", "Medium"},
    };
    std::smatch m;
    for (const auto& rule : files) {
        std::regex re(rule.pattern, std::regex::icase | std::regex::optimize);
        if (!std::regex_search(text, m, re)) continue;
        hit(all, source, rule.name, "Library", m[1].matched ? m[1].str() : "", rule.confidence, m.str());
    }
}
}

void identify_os_libraries(const std::string& text, const std::string& source, std::vector<Finding>& findings) {
    if (text.empty()) return;
    const std::string sample = text.size() > 4096 ? text.substr(0, 4096) : text;
    std::smatch m;

    static const std::regex ssh_debian(R"(OpenSSH_[0-9][A-Za-z0-9.p]*[ \t]+Debian-([0-9]+\+deb([0-9]+)u[0-9]+))", std::regex::icase);
    static const std::regex ssh_ubuntu(R"(OpenSSH_[0-9][A-Za-z0-9.p]*[ \t]+Ubuntu-([0-9][A-Za-z0-9.+~-]*))", std::regex::icase);
    static const std::regex ssh_windows(R"(OpenSSH_for_Windows_([0-9][A-Za-z0-9.p]*))", std::regex::icase);
    static const std::regex ssh_freebsd(R"(OpenSSH_[0-9][A-Za-z0-9.p]*[ \t]+FreeBSD-([0-9]{8}))", std::regex::icase);
    static const std::regex alpine_ver(R"(Alpine Linux v([0-9]+\.[0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    static const std::regex openssl_re(R"(\bOpenSSL/([0-9]+\.[0-9]+\.[0-9]+[a-z0-9]*))", std::regex::icase);
    static const std::regex libressl_re(R"(\bLibreSSL/([0-9]+\.[0-9]+\.[0-9]+[a-z0-9]*))", std::regex::icase);
    static const std::regex paren_re(R"(\b(?:Apache|nginx)/[0-9][0-9.]*[ \t]+\(([^)]{1,80})\))", std::regex::icase);
    static const std::regex winnt(R"(Windows NT ([0-9]+\.[0-9]+))", std::regex::icase);
    static const std::regex httpapi(R"(Microsoft-HTTPAPI/([0-9]+\.[0-9]+))", std::regex::icase);
    static const std::regex passenger(R"(Phusion Passenger(?:[ /]([0-9]+\.[0-9]+(?:\.[0-9]+)?))?)");
    static const std::regex python_re(R"(\bPython/([0-9]+\.[0-9]+(?:\.[0-9]+)?))");
    static const std::regex ruby_re(R"(\bRuby/([0-9]+\.[0-9]+(?:\.[0-9]+)?))");
    static const std::regex perl_re(R"(\bPerl/v?([0-9]+\.[0-9]+(?:\.[0-9]+)?))");
    static const std::regex modphp(R"(\bmod_php/([0-9]+\.[0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    static const std::regex modwsgi(R"(\bmod_wsgi/([0-9]+\.[0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    static const std::regex modperl(R"(\bmod_perl/([0-9]+\.[0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    static const std::regex servlet_re(R"(\bServlet[/ ]([0-9]+\.[0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    static const std::regex kestrel_re(R"((?:^|[\s/(])Kestrel\b)");
    static const std::regex cowboy_re(R"(\bCowboy/([0-9]+\.[0-9]+(?:\.[0-9]+)?))");
    static const std::regex arr_re(R"(\bARR/([0-9]+\.[0-9]+))");

    if (std::regex_search(sample, m, ssh_debian)) {
        hit(findings, source, "Debian Linux", "OS hint", m[2].str(), "Medium", m.str());
        hit(findings, source, "Debian OpenSSH package", "Package", m[1].str(), "High", m.str());
    }
    if (std::regex_search(sample, m, ssh_ubuntu)) {
        hit(findings, source, "Ubuntu Linux", "OS hint", {}, "Medium", m.str());
        hit(findings, source, "Ubuntu OpenSSH package", "Package", m[1].str(), "High", m.str());
    }
    if (std::regex_search(sample, m, ssh_windows)) {
        hit(findings, source, "Windows", "OS hint", {}, "Medium", m.str());
        hit(findings, source, "OpenSSH for Windows", "Service", m[1].str(), "High", m.str());
    }
    if (std::regex_search(sample, m, ssh_freebsd)) {
        hit(findings, source, "FreeBSD", "OS hint", {}, "Medium", m.str());
        hit(findings, source, "FreeBSD OpenSSH snapshot", "Package", m[1].str(), "High", m.str());
    }
    if (std::regex_search(sample, m, alpine_ver)) hit(findings, source, "Alpine Linux", "OS hint", m[1].str(), "Medium", m.str());
    if (std::regex_search(sample, m, openssl_re)) hit(findings, source, "OpenSSL", "Library", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, libressl_re)) hit(findings, source, "LibreSSL", "Library", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, paren_re)) parenthetical_os(m[1].str(), source, m.str(), findings);
    if (std::regex_search(sample, m, winnt)) hit(findings, source, "Windows", "OS hint", m[1].str(), "Medium", m.str());
    if (std::regex_search(sample, m, httpapi)) {
        hit(findings, source, "HTTP.sys", "Server", m[1].str(), "High", m.str());
        hit(findings, source, "Windows", "OS hint", {}, "Medium", m.str());
    }
    if (std::regex_search(sample, m, passenger)) hit(findings, source, "Phusion Passenger", "Runtime", m[1].matched ? m[1].str() : "", "High", m.str());
    if (std::regex_search(sample, m, python_re)) hit(findings, source, "Python", "Runtime", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, ruby_re)) hit(findings, source, "Ruby", "Runtime", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, perl_re)) hit(findings, source, "Perl", "Runtime", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, modphp)) hit(findings, source, "PHP", "Runtime", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, modwsgi)) hit(findings, source, "mod_wsgi", "Library", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, modperl)) hit(findings, source, "mod_perl", "Library", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, servlet_re)) hit(findings, source, "Java Servlet", "Runtime", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, kestrel_re)) hit(findings, source, "Kestrel", "Server", {}, "High", m.str());
    if (std::regex_search(sample, m, cowboy_re)) hit(findings, source, "Cowboy", "Server", m[1].str(), "High", m.str());
    if (std::regex_search(sample, m, arr_re)) hit(findings, source, "IIS ARR", "Server", m[1].str(), "High", m.str());
    if (source.find("x-aspnetmvc-version") != std::string::npos &&
        std::regex_match(sample, m, std::regex(R"([0-9]+\.[0-9]+(?:\.[0-9]+)?)"))) {
        hit(findings, source, "ASP.NET MVC", "Framework", sample, "High", sample);
    }
    versioned_path(sample, source, findings);
}
}
