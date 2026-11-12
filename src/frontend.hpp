#pragma once
#include "vector/model.hpp"

namespace vectorapp {
void identify_frontend_markup(const HttpObservation& page, std::vector<Finding>& findings);
void identify_stylesheet(const std::string& url, const std::string& body, std::vector<Finding>& findings);
std::vector<std::string> page_stylesheet_references(const HttpObservation& page);
}
