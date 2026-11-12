#include "vector/export.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace vectorapp {
namespace {
using nlohmann::json;

std::string csv_cell(const std::string& value) {
    std::string cell="\"";
    for(char c:value) { if(c=='"')cell+='"';cell+=c; }
    return cell+'"';
}
std::string pointer_key(const std::string& key) {
    std::string result;
    for(char c:key) {
        if(c=='~')result+="~0";
        else if(c=='/')result+="~1";
        else result+=c;
    }
    return result;
}
void csv_rows(const json& value,const std::string& path,std::string& out) {
    if(value.is_primitive()||value.empty()) {
        out+=csv_cell(path)+','+csv_cell(value.type_name())+','+csv_cell(value.dump())+"\r\n";
    } else if(value.is_array()) {
        for(size_t i=0;i<value.size();++i)csv_rows(value[i],path+'/'+std::to_string(i),out);
    } else {
        for(const auto& [key,item]:value.items())csv_rows(item,path+'/'+pointer_key(key),out);
    }
}
void text_rows(const json& value,const std::string& name,size_t depth,std::string& out) {
    out+=std::string(depth*2,' ')+name+':';
    if(value.is_primitive()||value.empty())out+=' '+value.dump()+"\n";
    else {
        out+='\n';
        if(value.is_array()) {
            for(size_t i=0;i<value.size();++i)text_rows(value[i],'['+std::to_string(i)+']',depth+1,out);
        } else {
            for(const auto& [key,item]:value.items())text_rows(item,key,depth+1,out);
        }
    }
}
}

const char* report_extension(ReportFormat format) {
    switch(format) {
        case ReportFormat::Json:return ".json";
        case ReportFormat::Csv:return ".csv";
        case ReportFormat::Txt:return ".txt";
    }
    throw std::invalid_argument("Unknown report format.");
}
std::string report_export(const ScanReport& report,ReportFormat format) {
    const auto serialized=report_json(report);
    if(format==ReportFormat::Json)return serialized+'\n';
    const auto data=json::parse(serialized);
    if(format==ReportFormat::Csv) {
        std::string out="Field,Type,Value\r\n";
        csv_rows(data,"",out);
        return out;
    }
    if(format==ReportFormat::Txt) {
        std::string out="Vector inspection report\n";
        for(const auto& [key,value]:data.items()) { out+='\n';text_rows(value,key,0,out); }
        return out;
    }
    throw std::invalid_argument("Unknown report format.");
}
void save_report(const ScanReport& report,const std::string& path,ReportFormat format) {
    const auto contents=report_export(report,format);
    std::ofstream file(std::filesystem::path(std::u8string(path.begin(),path.end())),std::ios::binary);
    if(!file)throw std::runtime_error("Cannot write report: "+path);
    file.write(contents.data(),static_cast<std::streamsize>(contents.size()));
    file.close();
    if(!file)throw std::runtime_error("Writing the report failed: "+path);
}
void save_report(const ScanReport& report,const std::string& path) {
    save_report(report,path,ReportFormat::Json);
}
}
