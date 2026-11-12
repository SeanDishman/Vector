#pragma once
#include "vector/model.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace vectorapp {
inline int documentation_link_score(const Target& page, const Target& link) {
    if (link.scheme != page.scheme || link.port != page.port) return 0;
    auto from = lower(page.host), to = lower(link.host);
    if (from.ends_with('.')) from.pop_back();
    if (to.ends_with('.')) to.pop_back();
    if (from.empty() || to.empty()) return 0;
    const auto site = from.starts_with("www.") ? from.substr(4) : from;
    const bool same = to == from || to == site || to == "www." + site;
    const bool below = !same && !page.is_ip && !link.is_ip && site.find('.') != std::string::npos &&
        to.size() > site.size() + 1 && to.ends_with("." + site);
    if (!same && !below) return 0;
    auto path = lower(link.path);
    path = path.substr(0, path.find_first_of("?#"));
    std::vector<std::string> segments;
    for (size_t at = 0; at < path.size();) {
        const auto next = path.find('/', at);
        const auto part = path.substr(at, next == std::string::npos ? std::string::npos : next - at);
        if (!part.empty()) segments.push_back(part);
        if (next == std::string::npos) break;
        at = next + 1;
    }
    auto own = lower(page.path);
    own = own.substr(0, own.find_first_of("?#"));
    if (same && path == own) return 0;
    const auto label = to.substr(0, to.find('.'));
    const bool named_host = below && (label == "docs" || label == "documentation");
    const bool developer_host = below && (label == "developers" || label == "developer");
    int section = -1;
    for (size_t i = 0; i < segments.size() && i < 4; ++i)
        if (segments[i] == "docs" || segments[i] == "documentation") { section = static_cast<int>(i); break; }
    if (!named_host && !developer_host && section < 0) return 0;
    int score = named_host ? 40 : section == 0 ? 30 : section > 0 ? 26 - 2 * section : 12;
    if (named_host && section >= 0) score += 4;
    if (developer_host && section >= 0) score += 6;
    return score - static_cast<int>(std::min<size_t>(segments.size(), 6));
}
}
