#include "web_signatures.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace vectorapp;
using Json=nlohmann::json;
int checks=0;
void check(bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);}
std::string read(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Fixture missing");return {std::istreambuf_iterator<char>(f),{}};}
bool has(const std::vector<Finding>& f,const std::string& name){for(const auto& v:f)if(v.name==name)return true;return false;}
std::vector<Finding> runtime(const Json& v){std::vector<Finding> f;web_signatures().runtime(v,"Browser runtime / https://unseen.test/p?token=PRIVATE",f);return f;}
std::vector<Finding> script(const std::string& body){std::vector<Finding> f;identify_javascript("https://unseen.test/opaque",body,f);return f;}
int main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--expression"){std::cout<<web_signatures().runtime_expression();return 0;}
    if(argc==3&&std::string(argv[1])=="--custom-expression"){std::cout<<WebSignatures(Json::parse(read(argv[2]))).runtime_expression();return 0;}
    if(argc==4&&std::string(argv[1])=="--match-custom") {
        WebSignatures engine(Json::parse(read(argv[2])));ScanReport r;r.status="offline_analysis";engine.runtime(Json::parse(read(argv[3])),"Offline runtime fixture",r.findings);std::cout<<report_json(r);return 0;
    }
    if(argc==3&&std::string(argv[1])=="--javascript") {ScanReport r;r.status="offline_analysis";r.findings=script(read(argv[2]));std::cout<<report_json(r);return 0;}
    const auto& definition=web_signatures().definition();
    for(const auto& r:definition["rules"]) {
        const auto name=r["name"].get<std::string>();
        if(r["source"]=="runtime") {
            Json observation={{"properties",Json::object()}};
            auto& p=observation["properties"];
            p[r["version"].get<std::string>()]={{"type","string"},{"value","4.5.6-RC.2+Build7"}};
            for(const auto& [key,type]:r["requires"].items())p[key]={{"type",type}};
            const auto found=runtime(observation);check(has(found,name),name+" paired API");
            for(const auto& f:found)if(f.name==name) {
                check(f.version=="4.5.6-RC.2+Build7",name+" retains advertised version case");
                check(f.evidence.size()==r["requires"].size()+1,name+" all supporting signals retained");
                for(const auto& e:f.evidence)check(e.source.find("PRIVATE")==std::string::npos&&e.source.find("rule "+r["id"].get<std::string>())!=std::string::npos,name+" rule provenance and redaction");
            }
            for(const auto& [key,type]:r["requires"].items()) {(void)type;auto missing=observation;missing["properties"].erase(key);check(!has(runtime(missing),name),name+" missing corroboration");
                missing=observation;missing["properties"][key]["type"]="string";check(!has(runtime(missing),name),name+" prose object rejected");}
            for(const auto* v:{"","4.","4", "4.5.6 trailing prose","NaN","4.5.6-"}) {auto bad=observation;bad["properties"][r["version"].get<std::string>()]["value"]=v;check(!has(runtime(bad),name),name+" invalid version");}
        } else if(r["source"]=="dom") {
            Json item={{"rule",r["id"]},{"tag","fixture-element"},{"attributes",Json::object()}};
            for(const auto& [key,kind]:r["attributes"].items())item["attributes"][key]=kind=="present"?Json(true):kind=="hex24"?Json("0123456789abcdef01234567"):Json("20.1.0");
            check(has(runtime({{"dom",Json::array({item})}}),name),name+" structured DOM");
            for(const auto& [key,kind]:r["attributes"].items()){(void)kind;auto bad=item;bad["attributes"].erase(key);check(!has(runtime({{"dom",Json::array({bad})}}),name),name+" incomplete element");}
            auto unknown=item;unknown["rule"]="unknown";check(!has(runtime({{"dom",Json::array({unknown})}}),name),"unknown rule ignored");
        }
    }
    HttpObservation page;page.url="https://unseen.test/";page.status=200;
    for(const auto& r:definition["rules"])if(r["source"]=="cookies") {
        page.headers.clear();for(const auto& name:r["names"])page.headers.emplace_back("Set-Cookie",name.get<std::string>()+"=PRIVATE; Path=/");
        if(r.contains("hex_suffix"))page.headers.emplace_back("set-cookie",r["hex_suffix"].get<std::string>()+std::string(32,'a')+"=PRIVATE");
        std::vector<Finding> f;web_signatures().cookies(page,f);check(has(f,r["name"]),"corroborated cookie names");
        for(const auto& x:f){check(x.version.empty(),"cookie names cannot infer versions");for(const auto& e:x.evidence)check(e.value.find("PRIVATE")==std::string::npos,"no cookie values exported");}
        auto incomplete=page;incomplete.headers.pop_back();f.clear();web_signatures().cookies(incomplete,f);check(f.empty(),"one cookie insufficient");
        auto rejected=page;rejected.status=403;f.clear();web_signatures().cookies(rejected,f);check(f.empty(),"rejected response cookies ignored");
        rejected=page;rejected.error="interrupted";web_signatures().cookies(rejected,f);check(f.empty(),"failed response cookies ignored");
    }
    page.headers={{"set-cookie","unrelated=laravel_session; XSRF-TOKEN=PRIVATE"}};std::vector<Finding> cookies;web_signatures().cookies(page,cookies);check(cookies.empty(),"values and attributes are not cookie names");
    for(const auto& r:definition["rules"])if(r["source"]=="script_banner") {
        const auto name=r["name"].get<std::string>(),prefix=r["prefix"].get<std::string>(),url=r["corroboration"].get<std::string>();
        const auto banner="/*! "+prefix+"4.5.6-RC.2 | "+url+" */";
        check(has(script(banner),name),"corroborated license comment");
        for(const auto& bad:{"const example='"+banner+"';","const example=`"+banner+"`;","// Example: "+banner,prefix+"4.5.6 | "+url,"/*! Example: "+prefix+"4.5.6 | "+url+" */","/*! "+prefix+"4.5.6 */","/*! "+prefix+"4.5.6 | "+url})check(!has(script(bad),name),"quoted, uncorroborated or incomplete banner");
        check(has(script("const example='"+banner+"';\n"+banner),name),"real comment after quoted example retained");
        check(has(script("// Example: "+banner+"\n"+banner),name),"real comment after line-comment example retained");
    }
    auto invalid=definition;invalid["rules"][0]["version"]="L.version);fetch('bad')";bool refused=false;try{WebSignatures e(invalid);}catch(...){refused=true;}check(refused,"executable property path rejected");
    invalid=definition;invalid["rules"].push_back(invalid["rules"][0]);refused=false;try{WebSignatures e(invalid);}catch(...){refused=true;}check(refused,"duplicate rule IDs rejected");
    std::cout<<checks<<" web signature checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
