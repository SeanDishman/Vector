#pragma once
#include "vector/model.hpp"
#include <nlohmann/json.hpp>
#include <span>

namespace vectorapp {

class WebSignatures {
public:
    explicit WebSignatures(nlohmann::json definition);
    const nlohmann::json& definition() const { return rules_; }
    std::string runtime_expression() const;
    void runtime(const nlohmann::json& observed,const std::string& source,std::vector<Finding>& out) const;
    void cookies(const HttpObservation& page,std::vector<Finding>& out) const;
    void javascript(const std::string& source,const std::string& body,std::span<const unsigned char> regions,std::vector<Finding>& out) const;
private:
    nlohmann::json rules_;
    void emit(const nlohmann::json& rule,const std::string& version,const std::string& source,std::vector<std::string> values,std::vector<Finding>& out) const;
};
const WebSignatures& web_signatures();
}
