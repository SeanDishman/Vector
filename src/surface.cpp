#include "vector/model.hpp"
#include <cctype>
#include <regex>
#include <string>

namespace vectorapp {
namespace {
void hit(std::vector<Finding>& all, const std::string& source, std::string name, const char* category,
         std::string version, const char* confidence, const std::string& evidence) {
    add_finding(all, Finding{std::move(name), category, std::move(version), confidence, {{source, printable(evidence, 300)}}});
}

std::string plugin_name(const std::string& slug) {
    static const std::pair<const char*, const char*> known[] = {
        {"woocommerce", "WooCommerce"},
        {"woocommerce-gateway-stripe", "WooCommerce Stripe"},
        {"woocommerce-payments", "WooPayments"},
        {"woocommerce-services", "WooCommerce Shipping & Tax"},
        {"facebook-for-woocommerce", "Facebook for WooCommerce"},
        {"contact-form-7", "Contact Form 7"},
        {"elementor", "Elementor"},
        {"elementor-pro", "Elementor Pro"},
        {"header-footer-elementor", "Elementor Header & Footer"},
        {"wordpress-seo", "Yoast SEO"},
        {"wordpress-seo-premium", "Yoast SEO Premium"},
        {"duplicate-post", "Yoast Duplicate Post"},
        {"jetpack", "Jetpack"},
        {"wordfence", "Wordfence"},
        {"akismet", "Akismet"},
        {"wpforms-lite", "WPForms"},
        {"litespeed-cache", "LiteSpeed Cache"},
        {"really-simple-ssl", "Really Simple SSL"},
        {"updraftplus", "UpdraftPlus"},
        {"classic-editor", "Classic Editor"},
        {"classic-widgets", "Classic Widgets"},
        {"wp-super-cache", "WP Super Cache"},
        {"w3-total-cache", "W3 Total Cache"},
        {"wp-fastest-cache", "WP Fastest Cache"},
        {"google-site-kit", "Site Kit by Google"},
        {"advanced-custom-fields", "Advanced Custom Fields"},
        {"revslider", "Slider Revolution"},
        {"js_composer", "WPBakery"},
        {"mailpoet", "MailPoet"},
        {"wp-mail-smtp", "WP Mail SMTP"},
        {"seo-by-rank-math", "Rank Math"},
        {"all-in-one-seo-pack", "All in One SEO"},
        {"autoptimize", "Autoptimize"},
        {"wp-rocket", "WP Rocket"},
        {"sg-cachepress", "SiteGround Optimizer"},
        {"all-in-one-wp-migration", "All-in-One WP Migration"},
        {"redirection", "Redirection"},
        {"limit-login-attempts-reloaded", "Limit Login Attempts Reloaded"},
        {"google-analytics-for-wordpress", "MonsterInsights"},
        {"complianz-gdpr", "Complianz"},
        {"cookieyes", "CookieYes"},
        {"cookie-law-info", "CookieYes"},
        {"tablepress", "TablePress"},
        {"the-events-calendar", "The Events Calendar"},
        {"ninja-forms", "Ninja Forms"},
        {"gravityforms", "Gravity Forms"},
        {"formidable", "Formidable Forms"},
        {"fluentform", "Fluent Forms"},
        {"memberpress", "MemberPress"},
        {"learndash", "LearnDash"},
        {"bbpress", "bbPress"},
        {"buddypress", "BuddyPress"},
        {"polylang", "Polylang"},
        {"sitepress-multilingual-cms", "WPML"},
        {"translatepress-multilingual", "TranslatePress"},
        {"wp-optimize", "WP-Optimize"},
        {"ewww-image-optimizer", "EWWW Image Optimizer"},
        {"smush", "Smush"},
        {"forminator", "Forminator"},
        {"wp-statistics", "WP Statistics"},
        {"duplicator", "Duplicator"},
        {"insert-headers-and-footers", "WPCode"},
        {"query-monitor", "Query Monitor"},
        {"custom-post-type-ui", "Custom Post Type UI"},
        {"disable-comments", "Disable Comments"},
        {"optinmonster", "OptinMonster"},
        {"popup-maker", "Popup Maker"},
        {"coming-soon", "SeedProd"},
        {"hummingbird-performance", "Hummingbird"},
        {"official-facebook-pixel", "Meta pixel for WordPress"},
    };
    for (const auto& [key, name] : known) if (slug == key) return name;
    std::string name;
    bool cap = true;
    for (unsigned char c : slug) {
        if (c == '-' || c == '_') { if (!name.empty() && name.back() != ' ') name.push_back(' '); cap = true; continue; }
        name.push_back(static_cast<char>(cap ? std::toupper(c) : std::tolower(c)));
        cap = false;
    }
    return name;
}

struct Hw {
    const char* name;
    const char* pattern;
    int version_group;
};

const Hw hardware_rules[] = {
    {"MikroTik RouterOS", R"((?:MikroTik|RouterOS)(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"Cisco IOS", R"(Cisco IOS(?:[^0-9]{0,40}Version\s+([0-9][0-9.()A-Za-z]+))?)", 1},
    {"HP LaserJet", R"((?:HP|Hewlett-Packard)\s+(?:Color\s+)?LaserJet(?:\s+([A-Z0-9][A-Z0-9-]*))?)", 1},
    {"Synology", R"(Synology(?:\s+(DiskStation|DS[0-9]+))?)", 1},
    {"QNAP", R"(\bQNAP\b(?:\s+([A-Z0-9][A-Z0-9-]*))?)", 1},
    {"Ubiquiti", R"(\b(?:Ubiquiti|UniFi|EdgeOS|EdgeRouter)(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"Hikvision", R"(\b(?:Hikvision|App-webs|DVRDVS-Webs)\b)", 0},
    {"Dahua", R"(\bDahua\b)", 0},
    {"HPE iLO", R"(\biLO\s*([0-9])\b|\bIntegrated Lights-Out\b)", 1},
    {"Dell iDRAC", R"(\biDRAC\s*([0-9]+)?)", 1},
    {"Fortinet", R"(\b(?:FortiGate|FortiOS)(?:[ /v]*([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"SonicWall", R"(\bSonicWall\b)", 0},
    {"pfSense", R"(\bpfSense(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"OPNsense", R"(\bOPNsense(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"OpenWrt", R"(\bOpenWrt(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"DD-WRT", R"(\bDD-WRT\b)", 0},
    {"MiniUPnP", R"(\bMiniUPnPd?(?:/([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"UPnP Portable SDK", R"(\blibupnp(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"VMware ESXi", R"(\bESXi(?:\s+([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"Proxmox VE", R"(\bProxmox(?:\s+Virtual Environment)?(?:[ /]([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"TrueNAS", R"(\bTrueNAS(?:\s+(CORE|SCALE))?)", 1},
    {"Unraid", R"(\bUnraid\b)", 0},
    {"Raspberry Pi", R"(\bRaspberry Pi(?:\s+([0-9]+|Zero|Pico))?\b)", 1},
    {"PJL printer", R"(@PJL)", 0},
    {"Brother printer", R"(\bBrother\s+(?:MFC|DCP|HL)-?[A-Z0-9-]+)", 0},
    {"Epson printer", R"(\bEPSON\b)", 0},
    {"Allegro RomPager", R"(\bRomPager(?:/([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"GoAhead", R"(\bGoAhead(?:-Webs)?(?:/([0-9]+(?:\.[0-9]+){0,3}))?)", 1},
    {"Boa", R"(\bBoa/([0-9]+(?:\.[0-9]+){0,3}))", 1},
    {"Embedded camera httpd", R"(\buc-httpd(?:/([0-9.]+))?)", 1},
};
}

void identify_wordpress_plugins(const std::string& text, const std::string& source, std::vector<Finding>& findings) {
    if (text.empty()) return;
    static const std::regex plugin(R"(/wp-content/(?:plugins|mu-plugins)/([A-Za-z0-9][A-Za-z0-9_-]{0,60})/)", std::regex::icase);
    static const std::regex ver(R"([?&]ver=([0-9]+(?:\.[0-9A-Za-z]+){0,4}))");
    int count = 0;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), plugin); it != std::sregex_iterator() && count < 24; ++it, ++count) {
        const auto slug = lower((*it)[1].str());
        if (slug == "index" || slug == "plugins") continue;
        const auto at = static_cast<size_t>(it->position());
        const auto window = text.substr(at, std::min<size_t>(200, text.size() - at));
        std::smatch version_match;
        std::string version;
        if (std::regex_search(window, version_match, ver)) version = version_match[1].str();
        auto evidence = (*it)[0].str();
        if (!version.empty()) evidence += "?ver=" + version;
        hit(findings, source, plugin_name(slug), "Plugin", version, "High", evidence);
    }
}

void identify_hardware(const std::string& text, const std::string& source, std::vector<Finding>& findings) {
    if (text.empty()) return;
    const auto sample = text.size() > 4000 ? text.substr(0, 4000) : text;
    std::smatch match;
    for (const auto& rule : hardware_rules) {
        const std::regex re(rule.pattern, std::regex::icase | std::regex::optimize);
        if (!std::regex_search(sample, match, re)) continue;
        std::string version;
        if (rule.version_group && match.size() > static_cast<size_t>(rule.version_group) && match[rule.version_group].matched)
            version = match[rule.version_group].str();
        hit(findings, source, rule.name, "Device hint", version, "Medium", match.str());
    }
}
}
