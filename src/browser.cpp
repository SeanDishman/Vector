#include "proxy_transport.hpp"
#include "vector/model.hpp"
#include "vector/api_endpoints.hpp"
#include "web_signatures.hpp"
#include "frontend.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace vectorapp {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr size_t kScriptBytes = 2 * 1024 * 1024;
constexpr size_t kScriptFetchBytes = 4 * 1024 * 1024;
constexpr size_t kScriptCount = 160;
constexpr size_t kStylesheetBytes = 512 * 1024;
constexpr size_t kStylesheetCount = 8;
constexpr size_t kRequests = 256;
constexpr size_t kTransferBytes = 32 * 1024 * 1024;
std::string str(const Json& j, const char* key) {
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : "";
}
bool http_url(const std::string& url) {
    const auto prefix=lower(url.substr(0,8));
    return prefix.starts_with("http://") || prefix.starts_with("https://");
}
std::string clean_url(std::string url) {

    if(url.empty())return {};
    if(!http_url(url))return "Non-HTTP source";
    const auto tail = url.find_first_of("?#");
    if (tail != std::string::npos) url.resize(tail);
    const auto authority=url.find("://")+3;
    const auto end=url.find('/',authority);
    const auto credentials=url.rfind('@',end==std::string::npos?url.size():end);
    if(credentials!=std::string::npos && credentials>=authority)url.erase(authority,credentials-authority+1);
    return printable(url, 2048);
}
std::string clean_socket_url(std::string url) {
    const auto prefix=lower(url.substr(0,6));
    if(!prefix.starts_with("ws://")&&!prefix.starts_with("wss://"))return {};
    const auto tail=url.find_first_of("?#");
    if(tail!=std::string::npos)url.resize(tail);
    const auto authority=url.find("://")+3;
    const auto end=url.find('/',authority);
    const auto credentials=url.rfind('@',end==std::string::npos?url.size():end);
    if(credentials!=std::string::npos&&credentials>=authority)url.erase(authority,credentials-authority+1);
    return printable(url,2048);
}
bool script_response(const BrowserRequest& request) {
    const auto mime=lower(trim(request.mime.substr(0,request.mime.find(';'))));
    return request.type=="Script" || mime=="text/javascript" || mime=="application/javascript" ||
        mime=="application/x-javascript" || mime=="text/ecmascript" || mime=="application/ecmascript";
}
std::string base64_decode(const std::string& text,size_t limit=kScriptBytes+1) {
    std::string out; unsigned value = 0; int bits = -8;
    if(!limit)return out;
    for (unsigned char c : text) {
        const std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const auto digit = alphabet.find(c);
        if (digit == std::string_view::npos) { if (c == '=') break; continue; }
        value = (value << 6) | static_cast<unsigned>(digit); bits += 6;
        if (bits >= 0) { out += static_cast<char>((value >> bits) & 255); bits -= 8; }
        if (out.size() >= limit) break;
    }
    return out;
}
#ifdef _WIN32
std::string base64_encode(const std::string& bytes) {
    constexpr std::string_view alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;out.reserve((bytes.size()+2)/3*4);
    for(size_t i=0;i<bytes.size();i+=3) {
        const unsigned a=static_cast<unsigned char>(bytes[i]);
        const unsigned b=i+1<bytes.size()?static_cast<unsigned char>(bytes[i+1]):0;
        const unsigned c=i+2<bytes.size()?static_cast<unsigned char>(bytes[i+2]):0;
        out+=alphabet[a>>2];out+=alphabet[((a&3)<<4)|(b>>4)];
        out+=i+1<bytes.size()?alphabet[((b&15)<<2)|(c>>6)]:'=';
        out+=i+2<bytes.size()?alphabet[c&63]:'=';
    }
    return out;
}
bool usable_document(const HttpObservation* page,const Target& target,const ScanOptions& options) {
    if(!page||page->url!=target.url()||page->status!=200||!page->error.empty()||page->body.empty()||
       page->body_truncated||page->body.size()>512*1024)return false;
    if(target.scheme=="https"&&!page->tls_verified&&!options.allow_untrusted_tls)return false;
    std::string mime;
    for(const auto& [name,value]:page->headers) {

        if(value.size()>=4096)return false;
        if(lower(name)=="content-type")mime=lower(trim(value.substr(0,value.find(';'))));
    }
    return mime=="text/html"||(mime.empty()&&page->body.find('<')!=std::string::npos);
}
struct Handle {
    HANDLE value = nullptr;
    Handle() = default;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    void reset(HANDLE h = nullptr) { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = h; }
};
std::filesystem::path environment(const wchar_t* name) {
    const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (!length || length > 32768) return {};
    std::wstring value(length, L'\0');
    const auto copied = GetEnvironmentVariableW(name, value.data(), length);
    if (!copied || copied >= length) return {};
    value.resize(copied); return value;
}
std::filesystem::path browser_path() {

    if (auto override_path = environment(L"VECTOR_BROWSER"); !override_path.empty()) {
        if (std::filesystem::is_regular_file(override_path)) return override_path;
        return {};
    }
    for (const auto root : {L"ProgramFiles(x86)", L"ProgramFiles", L"LOCALAPPDATA"}) {
        const auto dir = environment(root); if (dir.empty()) continue;
        for (const auto tail : {L"Microsoft/Edge/Application/msedge.exe", L"Google/Chrome/Application/chrome.exe"}) {
            const auto path = dir / tail; if (std::filesystem::is_regular_file(path)) return path;
        }
    }
    return {};
}
std::wstring wide(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size && !text.empty()) throw std::runtime_error("Invalid UTF-8 in browser target");
    std::wstring out(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}
std::wstring quote(const std::wstring& arg) {
    std::wstring result = L"\""; size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'"' ? 2 : 1), L'\\'); slashes = 0;
        if (c == L'"') result += L'\\';
        result += c;
    }
    result.append(slashes * 2, L'\\'); return result + L'"';
}
class BrowserProcess {
    Handle input, output, process, job;
    std::filesystem::path profile, temp_root;
    std::string pending;
    std::atomic_bool& cancel;
    Clock::time_point stop_at;
    int next_id = 0;
public:
    std::function<void(const Json&)> on_message;
    BrowserProcess(std::atomic_bool& token,Clock::time_point deadline) : cancel(token),stop_at(deadline) {}
    ~BrowserProcess() {

        if(process.value && input.value && WaitForSingleObject(process.value,0)==WAIT_TIMEOUT) {
            stop_at=Clock::now()+std::chrono::milliseconds(200);
            try { send("Browser.close"); } catch(...) {}

            const auto close_deadline=Clock::now()+std::chrono::milliseconds(1500);
            while(Clock::now()<close_deadline&&WaitForSingleObject(process.value,0)==WAIT_TIMEOUT) {
                DWORD available=0,read=0;char discard[65536];
                if(output.value&&PeekNamedPipe(output.value,nullptr,0,nullptr,&available,nullptr)&&available)
                    ReadFile(output.value,discard,std::min<DWORD>(available,sizeof(discard)),&read,nullptr);
                else WaitForSingleObject(process.value,20);
            }
        }

        if (job.value) {
            TerminateJobObject(job.value, 0);
            const auto until=Clock::now()+std::chrono::milliseconds(1500);
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            while(Clock::now()<until && QueryInformationJobObject(job.value,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr) && accounting.ActiveProcesses)
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        if (process.value) WaitForSingleObject(process.value, 1500);
        input.reset(); output.reset(); process.reset(); job.reset();
        std::error_code error;
        if (!profile.empty() && std::filesystem::equivalent(profile.parent_path(),temp_root,error) && profile.filename().wstring().starts_with(L"vector-browser-")) {

            const auto cleanup_deadline=Clock::now()+std::chrono::seconds(3);
            do {
                std::filesystem::remove_all(profile, error);
                if (!error) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } while(Clock::now()<cleanup_deadline);
        }
        if(error)std::fprintf(stderr,"Browser profile cleanup: %s\n",error.message().c_str());
    }
    void start(const std::filesystem::path& executable, const Target& target, const std::string& address, const std::string& proxy) {
        if(Clock::now()>=stop_at)throw std::runtime_error("Web evidence time limit reached");
        temp_root = std::filesystem::temp_directory_path();
        const auto unique = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(Clock::now().time_since_epoch().count());
        profile = temp_root / (L"vector-browser-" + unique);
        if (!std::filesystem::create_directory(profile)) throw std::runtime_error("Cannot create isolated browser profile");
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        Handle child_read, child_write;

        const auto pipe_name = L"\\\\.\\pipe\\vector-cdp-" + unique;
        input.reset(CreateNamedPipeW(pipe_name.c_str(), PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, nullptr));
        if (input.value == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create browser command pipe");
        child_read.reset(CreateFileW(pipe_name.c_str(), GENERIC_READ, 0, &security, OPEN_EXISTING, 0, nullptr));
        if (child_read.value == INVALID_HANDLE_VALUE || !CreatePipe(&output.value, &child_write.value, &security, 65536))
            throw std::runtime_error("Cannot create browser event pipe");
        SetHandleInformation(output.value, HANDLE_FLAG_INHERIT, 0);
        std::wstring command = quote(executable.wstring());
        std::vector<std::wstring> args = {
            L"--headless=new", L"--remote-debugging-pipe",
            L"--remote-debugging-io-pipes=" + std::to_wstring(reinterpret_cast<uintptr_t>(child_read.value)) + L"," + std::to_wstring(reinterpret_cast<uintptr_t>(child_write.value)),
            L"--user-data-dir=" + profile.wstring(), L"--no-first-run", L"--no-default-browser-check",
            L"--disable-extensions", L"--disable-sync", L"--disable-background-networking", L"--disable-component-update",
            L"--disable-default-apps", L"--disable-domain-reliability", L"--disable-quic",
            L"--password-store=basic", L"--use-mock-keychain", L"--mute-audio", L"--window-size=1280,800",
            L"--disable-features=MediaRouter,OptimizationHints,AutofillServerCommunication"
        };
        if(!proxy.empty()) {
            args.push_back(L"--proxy-server="+wide(proxy));
            args.push_back(L"--proxy-bypass-list=<-loopback>");
            args.push_back(L"--host-resolver-rules=MAP * ~NOTFOUND, EXCLUDE 127.0.0.1");
            args.push_back(L"--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
        } else args.push_back(L"--no-proxy-server");
        if(proxy.empty()&&!address.empty())args.push_back(L"--host-resolver-rules=MAP " + wide(target.host) + L" " + wide(address));
        args.push_back(L"about:blank");
        for (const auto& arg : args) command += L" " + quote(arg);
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        std::vector<unsigned char> storage(size);
        auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) throw std::runtime_error("Browser process attributes failed");
        const HANDLE inherited[] = {child_read.value, child_write.value};
        const bool configured = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            const_cast<HANDLE*>(inherited), sizeof(inherited), nullptr, nullptr);
        STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW; startup.StartupInfo.wShowWindow = SW_HIDE;
        startup.lpAttributeList = attributes; PROCESS_INFORMATION info{};
        const bool started = configured && CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &info);
        DeleteProcThreadAttributeList(attributes);
        if (!started) throw std::runtime_error("Unable to start isolated Chromium browser");
        process.reset(info.hProcess); Handle thread(info.hThread);
        job.reset(CreateJobObjectW(nullptr, nullptr));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job.value || !SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
            !AssignProcessToJobObject(job.value, process.value)) {
            TerminateProcess(process.value, 1); throw std::runtime_error("Cannot isolate browser process lifetime");
        }
        ResumeThread(thread.value);
    }
    int send(const std::string& method, Json parameters = Json::object(), const std::string& session = "") {
        if(Clock::now()>=stop_at)throw std::runtime_error("Web evidence time limit reached");
        Json command{{"id", ++next_id}, {"method", method}, {"params", std::move(parameters)}};
        if (!session.empty()) command["sessionId"] = session;
        std::string bytes = command.dump(); bytes += '\0';
        Handle ready(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        OVERLAPPED state{}; state.hEvent = ready.value; DWORD count = 0;
        if (!WriteFile(input.value, bytes.data(), static_cast<DWORD>(bytes.size()), &count, &state)) {
            if (GetLastError() != ERROR_IO_PENDING) throw std::runtime_error("Browser command pipe closed");
            const auto until = Clock::now() + std::chrono::seconds(2);
            while (WaitForSingleObject(ready.value, 25) == WAIT_TIMEOUT) {
                if (cancel || Clock::now() >= until || Clock::now() >= stop_at) {
                    CancelIoEx(input.value, &state); GetOverlappedResult(input.value, &state, &count, TRUE);
                    throw std::runtime_error(cancel ? "Cancelled" : "Browser command timed out");
                }
            }
            if (!GetOverlappedResult(input.value, &state, &count, FALSE)) throw std::runtime_error("Browser command failed");
        }
        if (count != bytes.size()) throw std::runtime_error("Incomplete browser command");
        return next_id;
    }
    bool receive(Json& message, Clock::time_point until) {
        until=std::min(until,stop_at);
        while (!cancel && Clock::now() < until) {
            const auto end = pending.find('\0');
            if (end != std::string::npos) {
                auto frame = pending.substr(0, end); pending.erase(0, end + 1);
                message = Json::parse(frame, nullptr, false);
                if (message.is_object()) return true;
                continue;
            }
            DWORD available = 0;
            if (!PeekNamedPipe(output.value, nullptr, 0, nullptr, &available, nullptr)) throw std::runtime_error("Browser event pipe closed");
            if (available) {
                char buffer[65536]; DWORD read = 0;
                if (!ReadFile(output.value, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr)) throw std::runtime_error("Browser event read failed");
                pending.append(buffer, read);
                if (pending.size() > 20 * 1024 * 1024) throw std::runtime_error("Browser message exceeded collection limit");
            } else {
                if (WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0) throw std::runtime_error("Browser exited before collection finished");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return false;
    }
    Json call(const std::string& method, Json params = Json::object(), const std::string& session = "", int timeout = 8000) {
        const auto id = send(method, std::move(params), session);
        const auto until = Clock::now() + std::chrono::milliseconds(timeout); Json message;
        while (receive(message, until)) {
            if (message.value("id", 0) == id) {
                if (message.contains("error")) throw std::runtime_error(method + ": " + str(message["error"], "message"));
                return message.value("result", Json::object());
            }
            if (on_message) on_message(message);
        }
        throw std::runtime_error(cancel ? "Cancelled" : method + " timed out");
    }
};

const char* runtime_expression = R"JS((() => {
  const values=[];
  const own=(o,k)=>{try {const d=Object.getOwnPropertyDescriptor(o,k);return d&&'value' in d?d.value:undefined;}catch{return undefined;}};
  const read=(name,version,evidence)=>{if(typeof version==='string' && /^\d+\.\d+(?:\.[\w.+-]+)*$/.test(version) && version.length<80)values.push({name,version,evidence});};
  const jq=own(globalThis,'jQuery'); if(typeof jq==='function' && typeof own(jq,'ajax')==='function')read('jQuery',own(own(jq,'fn'),'jquery'),'jQuery.fn.jquery; jQuery.ajax function');
  const react=own(globalThis,'React'); if(react && typeof own(react,'createElement')==='function')read('React',own(react,'version'),'React.version; React.createElement function');
  const vue=own(globalThis,'Vue'); if(vue && (typeof own(vue,'createApp')==='function'||typeof own(vue,'extend')==='function'))read('Vue.js',own(vue,'version'),'Vue.version; Vue factory function');
  const polymer=own(globalThis,'Polymer'); if(polymer && (typeof polymer==='function'||own(polymer,'Element')))read('Polymer',own(polymer,'version'),'Polymer.version; Polymer element API');
  for(const [key,name] of [['litHtmlVersions','lit-html'],['litElementVersions','LitElement'],['reactiveElementVersions','Lit ReactiveElement']]) {
    const a=own(globalThis,key); if(Array.isArray(a))for(const v of a.slice(0,8))read(name,v,key+' version registry');
  }
  if(document.documentElement) {
    const walker=document.createTreeWalker(document.documentElement,NodeFilter.SHOW_ELEMENT);
    let node=walker.currentNode,remaining=2000,matched=false;
    while(node && remaining-- && !matched) {
      for(const key of Object.getOwnPropertyNames(node).slice(0,128)) {
        if(!key.startsWith('__reactFiber$'))continue;
        const suffix=key.slice('__reactFiber$'.length),fiber=own(node,key),props=own(node,'__reactProps$'+suffix);
        if(fiber && own(fiber,'stateNode')===node && Number.isInteger(own(fiber,'tag')) && props && typeof props==='object') {
          values.push({name:'React',version:'',confidence:'Medium',evidence:key+' and __reactProps$'+suffix+' on '+node.tagName+'; fiber.stateNode matches element'});
          matched=true;break;
        }
      }
      node=walker.nextNode();
    }
  }
  return {url:location.href,title:document.title,html:document.documentElement?.outerHTML.slice(0,524288)||'',
    truncated:(document.documentElement?.outerHTML.length||0)>524288,values};
})())JS";

void run_page(const Target& target, ScanReport& report, BrowserPage& page, const ScanOptions& options,
              std::atomic_bool& cancel, const ScanObserver& observer, const std::filesystem::path& executable,Clock::time_point web_deadline,
              const HttpObservation* collected_page,const std::string& address,const std::function<void(const HttpObservation&)>& document_observer) {
    std::unique_ptr<BrowserProxy> proxy;
    if(options.proxy.enabled)proxy=std::make_unique<BrowserProxy>(options.proxy,cancel,web_deadline);
    BrowserProcess browser(cancel,web_deadline); browser.start(executable, target, address,proxy?proxy->endpoint():std::string{});
    auto version = browser.call("Browser.getVersion", Json::object(), "", 15000);
    report.browser.engine = str(version, "product");
    browser.call("Browser.setDownloadBehavior", {{"behavior", "deny"}});

    const auto tab = browser.call("Target.createTarget", {{"url", "about:blank"}});
    const auto target_id = str(tab, "targetId");
    const auto session = str(browser.call("Target.attachToTarget", {{"targetId", target_id}, {"flatten", true}}), "sessionId");
    if (session.empty()) throw std::runtime_error("Browser returned no page session");
    browser.call("Page.enable", Json::object(), session);
    browser.call("Network.enable", {{"maxTotalBufferSize", 32 * 1024 * 1024}, {"maxResourceBufferSize", 4 * 1024 * 1024}}, session);
    browser.call("Network.setCacheDisabled", {{"cacheDisabled", true}}, session);
    browser.call("Runtime.enable", Json::object(), session);
    browser.call("Fetch.enable", {{"patterns", Json::array({{{"urlPattern", "*"}}})}, {"handleAuthRequests", true}}, session);
    browser.call("Emulation.setDeviceMetricsOverride", {{"width",1280},{"height",800},{"deviceScaleFactor",1},{"mobile",false}}, session);
    if (options.allow_untrusted_tls) browser.call("Security.setIgnoreCertificateErrors", {{"ignore",true}}, session);
    const auto tree = browser.call("Page.getFrameTree", Json::object(), session);
    const auto main_frame = str(tree.at("frameTree").at("frame"), "id");
    const auto began = Clock::now(); auto last_event = began;
    const auto first_request=report.browser.requests.size();
    size_t request_count = 0, script_count = 0, stylesheet_count = 0, transferred = 0;
    std::map<std::string, size_t> requests;
    std::map<std::string, std::string> raw_urls, blocked;
    std::map<int, size_t> bodies;
    struct ScriptStream {
        std::string prefix,early,session,request_id;
        bool ready=false,unsupported=false,finished=false,failed=false;
        size_t missed_bytes=0;
    };
    std::map<size_t,ScriptStream> streams;
    std::map<int,size_t> stream_replies;
    std::set<std::string> live, analyzed_urls;
    bool loaded = false, limited = false, capped = false, document_supplied = false;
    std::set<std::string> supplied_requests;
    const std::string origin_label=collected_page?" (captured HTTP page)":"";
    auto notify = [&](ActivityState state, const std::string& item, const std::string& detail) {
        if (observer) observer({ScanStage::Web, state, 0, item, detail});
    };
    auto key_of = [](const Json& m, const Json& p) { return str(m,"sessionId") + ":" + str(p,"requestId"); };
    const auto body_limit=[&](size_t index){return report.browser.requests[index].type=="Stylesheet"?kStylesheetBytes:kScriptBytes;};
    auto resource_result = [&](size_t index, const Json& response) {
        const auto& request = report.browser.requests[index];
        const bool stylesheet=request.type=="Stylesheet";
        const std::string kind=stylesheet?"stylesheet":"script";
        const auto limit=body_limit(index);
        ScriptObservation result; result.url = request.url; result.referenced_by = request.initiator.empty() ? request.page : request.initiator;
        result.collection = "browser"; result.status = request.status;
        if(cancel) {
            result.state="cancelled";result.reason="Browser inspection cancelled before "+kind+" analysis";
        } else if (response.contains("error")) {
            result.state = "unavailable"; result.reason = "Browser response body unavailable: " + str(response["error"],"message");
        } else {
            const auto& data = response.at("result");
            auto body = str(data,"body");
            if (data.value("base64Encoded",false)) body = base64_decode(body,limit+1);
            const bool incomplete=response.value("incomplete",false);
            result.body_truncated = body.size() > limit || incomplete; body.resize(std::min(body.size(),limit));
            result.body_bytes = body.size(); result.analyzed = true; result.state = result.body_truncated ? "partial" : "analyzed";
            result.reason = incomplete ? "Browser response interrupted; analyzed the retained prefix" :
                result.body_truncated ? "Browser-loaded "+kind+"; analyzed first "+(stylesheet?"512 KiB":"2 MiB") : "Analyzed "+kind+" loaded by the browser";
            if(response.value("streamed",false))result.reason+=" (streamed response)";
            if(stylesheet)identify_stylesheet("Browser stylesheet"+origin_label+" "+request.url,body,report.findings);
            else {
                identify_javascript("Browser script"+origin_label+" "+request.url,body,report.findings);++page.scripts_analyzed;
                collect_api_references(request.url,body,report,"Browser script"+origin_label);
            }
        }
        notify(result.analyzed ? ActivityState::Complete : ActivityState::Failed,"Browser "+kind+" "+request.url,result.reason);
        (stylesheet?report.stylesheets:report.scripts).push_back(std::move(result));
    };
    auto finish_stream = [&](size_t index,bool interrupted=false) {
        const auto it=streams.find(index);if(it==streams.end())return;
        auto stream=std::move(it->second);streams.erase(it);
        if(stream.ready && !stream.prefix.empty())
            resource_result(index,{{"result",{{"body",std::move(stream.prefix)}}},{"streamed",true},{"incomplete",interrupted||stream.failed}});
        else if(!interrupted && !stream.failed) {

            const auto& r=report.browser.requests[index];
            if(r.transferred_bytes>kScriptFetchBytes)
                resource_result(index,{{"error",{{"message","Streaming unavailable; resource exceeds the 4 MiB body-retrieval limit"}}}});
            else bodies[browser.send("Network.getResponseBody",{{"requestId",stream.request_id}},stream.session)]=index;
        } else resource_result(index,{{"error",{{"message","Response ended before a contiguous resource prefix was available"}}}});
    };
    auto start_stream = [&](size_t index,const std::string& sid,const std::string& request_id,const std::string& raw_url) {
        if(!analyzed_urls.insert(raw_url).second)return;
        const auto& r=report.browser.requests[index];
        const bool stylesheet=r.type=="Stylesheet";
        if((stylesheet?stylesheet_count++:script_count++)>=(stylesheet?kStylesheetCount:kScriptCount)) {
            ScriptObservation skip;skip.url=r.url;skip.referenced_by=page.url;skip.collection="browser";skip.state="skipped";skip.status=r.status;
            skip.reason=stylesheet?"Browser stylesheet analysis limit reached":"Browser script analysis limit reached";
            (stylesheet?report.stylesheets:report.scripts).push_back(std::move(skip));
        } else {
            auto& stream=streams[index];stream.session=sid;stream.request_id=request_id;
            stream_replies[browser.send("Network.streamResourceContent",{{"requestId",request_id}},sid)]=index;
        }
    };
    browser.on_message = [&](const Json& message) {
        const auto id = message.value("id",0);
        if (auto b = bodies.find(id); b != bodies.end()) { resource_result(b->second,message); bodies.erase(b); return; }
        if(auto reply=stream_replies.find(id);reply!=stream_replies.end()) {
            const auto index=reply->second;stream_replies.erase(reply);
            if(auto it=streams.find(index);it!=streams.end()) {
                auto& stream=it->second;
                stream.unsupported=message.contains("error");
                stream.ready=!stream.unsupported;
                if(stream.ready) {
                    const auto limit=body_limit(index)+1;
                    stream.prefix=base64_decode(str(message.value("result",Json::object()),"bufferedData"),limit);

                    if(stream.prefix.size()<std::min(stream.missed_bytes,limit)) {
                        stream.ready=false;stream.unsupported=true;stream.prefix.clear();
                    }
                    stream.prefix.append(stream.early,0,limit-stream.prefix.size());
                }
                stream.early.clear();
                if(stream.finished)finish_stream(index,stream.failed);
            }
            return;
        }
        const auto method = str(message,"method"), sid = str(message,"sessionId");
        const auto p = message.value("params",Json::object());
        if (method == "Target.attachedToTarget") {
            const auto child = str(p,"sessionId"); const auto info = p.value("targetInfo",Json::object());
            if (str(info,"type") == "page") { browser.send("Target.closeTarget",{{"targetId",str(info,"targetId")}}); return; }
            browser.send("Network.enable",{{"maxTotalBufferSize",32*1024*1024},{"maxResourceBufferSize",4*1024*1024}},child);
            browser.send("Fetch.enable",{{"patterns",Json::array({{{"urlPattern","*"}}})},{"handleAuthRequests",true}},child);
            browser.send("Target.setAutoAttach",{{"autoAttach",true},{"waitForDebuggerOnStart",true},{"flatten",true}},child);
            browser.send("Runtime.runIfWaitingForDebugger",Json::object(),child);
        } else if (method == "Fetch.authRequired") {
            browser.send("Fetch.continueWithAuth",{{"requestId",str(p,"requestId")},{"authChallengeResponse",{{"response","CancelAuth"}}}},sid);
        } else if (method == "Fetch.requestPaused") {
            ++request_count;
            const auto request = p.value("request",Json::object()); const auto url = str(request,"url");
            std::string reason;
            const bool main_navigation = sid == session && str(p,"frameId") == main_frame && str(p,"resourceType") == "Document";
            bool same_document=false;
            try {
                const auto endpoint = parse_target(url);
                if (endpoint.port == 9100) reason = "Printer port excluded";
                same_document=endpoint.url()==target.url();
                if (main_navigation && (!same_website_host(target,endpoint) || (endpoint.port != target.port && !(target.scheme=="http" && endpoint.scheme=="https" && endpoint.port==443))))
                    reason = "Navigation outside the inspected endpoint";
                if (main_navigation && target.scheme=="https" && endpoint.scheme!="https") reason = "HTTPS downgrade not followed";
            } catch (const std::exception&) { reason = "Only HTTP(S) browser requests are collected"; }
            if (request_count > kRequests) { reason = "Browser request or transfer budget reached"; capped = true; }
            if (transferred > kTransferBytes) { reason = "Browser request or transfer budget reached"; limited = true; }
            if(reason.empty() && collected_page && !document_supplied && main_navigation && same_document && str(request,"method")=="GET") {
                Json headers=Json::array();
                for(const auto& [name,value]:collected_page->headers) {
                    const auto key=lower(name);

                    if(key=="content-length"||key=="content-encoding"||key=="transfer-encoding"||key=="connection"||
                       key=="keep-alive"||key=="te"||key=="trailer"||key=="upgrade"||key=="set-cookie"||key=="alt-svc")continue;
                    headers.push_back({{"name",name},{"value",value}});
                }
                if(std::none_of(collected_page->headers.begin(),collected_page->headers.end(),[](const auto& h){return lower(h.first)=="content-type";}))
                    headers.push_back({{"name","Content-Type"},{"value","text/html"}});
                const auto key=sid+":"+str(p,"networkId");supplied_requests.insert(key);
                if(auto r=requests.find(key);r!=requests.end())report.browser.requests[r->second].collection="captured_http";
                document_supplied=true;
                browser.send("Fetch.fulfillRequest",{{"requestId",str(p,"requestId")},{"responseCode",collected_page->status},
                    {"responseHeaders",headers},{"body",base64_encode(collected_page->body)}},sid);
            }
            else if (reason.empty()) browser.send("Fetch.continueRequest",{{"requestId",str(p,"requestId")}},sid);
            else {
                browser.send("Fetch.failRequest",{{"requestId",str(p,"requestId")},{"errorReason","BlockedByClient"}},sid);
                const auto key = sid + ":" + str(p,"networkId");
                blocked[key]=reason;
                if (auto r=requests.find(key); r!=requests.end()) {
                    report.browser.requests[r->second].state="blocked"; report.browser.requests[r->second].error=reason;
                }
            }
        } else if (method == "Network.requestWillBeSent") {
            const auto key = key_of(message,p);const auto request=p.value("request",Json::object());
            if(!http_url(str(request,"url"))) { requests.erase(key);raw_urls.erase(key);live.erase(key);return; }
            last_event = Clock::now();
            if (auto old=requests.find(key); old!=requests.end() && p.contains("redirectResponse")) {
                auto& r=report.browser.requests[old->second];
                if(r.state!="blocked") {
                    const auto& response=p["redirectResponse"];
                    r.status=response.value("status",0);r.state="redirect";
                    r.address=options.proxy.enabled?std::string{}:str(response,"remoteIPAddress");r.mime=str(response,"mimeType");

                    if(r.collection=="network")identify_browser_vendor(r,report);
                }
                streams.erase(old->second);
                supplied_requests.erase(key);
            }
            if (report.browser.requests.size() >= 1024 || report.browser.requests.size()-first_request >= kRequests || request_count > kRequests) {
                report.browser.requests_truncated=true; capped=true; return;
            }
            BrowserRequest r;
            raw_urls[key]=str(request,"url");r.url=clean_url(raw_urls[key]);r.page=page.url;r.type=str(p,"type");r.method=str(request,"method");
            if(supplied_requests.contains(key))r.collection="captured_http";
            if(auto b=blocked.find(key);b!=blocked.end()){r.state="blocked";r.error=b->second;}
            const auto initiator=p.value("initiator",Json::object()); r.initiator=clean_url(str(initiator,"url"));
            if (initiator.contains("stack")) {
                const auto frames=initiator["stack"].value("callFrames",Json::array());
                if (!frames.empty()) r.initiator=clean_url(str(frames[0],"url"));
            }
            requests[key]=report.browser.requests.size(); report.browser.requests.push_back(std::move(r));live.insert(key);

            if(report.browser.requests.back().type=="Script")start_stream(requests[key],sid,str(p,"requestId"),raw_urls[key]);
        } else if (method == "Network.responseReceived") {
            const auto it=requests.find(key_of(message,p)); if(it==requests.end())return;
            auto& r=report.browser.requests[it->second]; const auto response=p.value("response",Json::object());
            if(r.state=="blocked")return;
            r.status=response.value("status",0);r.address=options.proxy.enabled?std::string{}:str(response,"remoteIPAddress");r.mime=str(response,"mimeType");r.state="response";
            if(sid==session && r.type=="Document" && str(p,"frameId")==main_frame) {
                page.status=r.status;
                HttpObservation metadata;metadata.url=r.url;metadata.status=r.status;
                const auto headers=response.value("headers",Json::object());
                for(auto it=headers.begin();it!=headers.end();++it) {
                    const auto name=lower(it.key());
                    if(!it.value().is_string())continue;
                    if(name=="server"||name=="x-powered-by"||name=="x-generator"||name=="x-aspnet-version"||name=="via"||
                       name=="cf-ray"||name=="x-vercel-id"||name=="x-served-by")
                        metadata.headers.emplace_back(name,printable(it.value().get<std::string>(),4096));
                }
                std::vector<Finding> found;identify_http(metadata,found);
                for(auto& finding:found)if(finding.category!="Fingerprint"&&r.collection=="network") {
                    for(auto& evidence:finding.evidence)evidence.source="Browser response / "+evidence.source;
                    add_finding(report.findings,std::move(finding));
                }
            }

            if(script_response(r)) identify_script_reference(r.url,"Browser request" + origin_label + " from " + page.url,report.findings);
            if(r.collection=="network")identify_browser_vendor(r,report);
            if(r.type=="Stylesheet"&&r.status>=200&&r.status<300&&lower(r.mime)!="text/css") {
                ScriptObservation skipped;skipped.url=r.url;skipped.referenced_by=page.url;skipped.collection="browser";
                skipped.status=r.status;skipped.state="skipped";skipped.reason="Browser stylesheet response is not text/css";
                report.stylesheets.push_back(std::move(skipped));
            } else if((r.type=="Stylesheet"?lower(r.mime)=="text/css":script_response(r)) && r.status>=200 && r.status<300)
                start_stream(it->second,sid,str(p,"requestId"),raw_urls[key_of(message,p)]);
            else {streams.erase(it->second);analyzed_urls.erase(raw_urls[key_of(message,p)]);}
        } else if (method == "Network.dataReceived") {
            const auto it=requests.find(key_of(message,p)); if(it==requests.end())return;
            if(report.browser.requests[it->second].collection!="network")return;
            const auto bytes=static_cast<size_t>(std::max(0.0,p.value("dataLength",0.0)));
            report.browser.requests[it->second].transferred_bytes+=bytes;transferred+=bytes;
            if(auto found=streams.find(it->second);found!=streams.end() && !found->second.unsupported) {
                auto& stream=found->second;auto& prefix=stream.ready?stream.prefix:stream.early;

                const auto data=str(p,"data");
                if(data.empty()&&bytes) {
                    stream.missed_bytes+=bytes;
                    if(stream.ready){stream.ready=false;stream.unsupported=true;stream.prefix.clear();}
                } else prefix+=base64_decode(data,body_limit(it->second)+1-prefix.size());
            }
            if(transferred>kTransferBytes)limited=true;
        } else if (method == "Network.loadingFinished" || method == "Network.loadingFailed") {
            last_event=Clock::now();const auto key=key_of(message,p);live.erase(key);
            const auto it=requests.find(key);if(it==requests.end())return;
            auto& r=report.browser.requests[it->second];
            if(r.state=="blocked")return;
            const bool failed=method=="Network.loadingFailed";
            r.state=failed?"failed":"complete";
            if(failed)r.error=printable(str(p,"errorText"),240);
            if(failed&&r.status<=0)identify_browser_vendor(r,report);
            if(auto stream=streams.find(it->second);stream!=streams.end()) {
                stream->second.finished=true;stream->second.failed=failed;
                if(stream->second.ready||stream->second.unsupported)finish_stream(it->second,failed);
            }
        } else if (method == "Network.webSocketCreated") {
            const auto key=key_of(message,p);const auto address=clean_socket_url(str(p,"url"));
            if(address.empty()||requests.contains(key))return;
            if(report.browser.requests.size()>=1024||report.browser.requests.size()-first_request>=kRequests){report.browser.requests_truncated=true;return;}
            last_event=Clock::now();
            BrowserRequest r;r.url=address;r.page=page.url;r.type="WebSocket";r.method="GET";
            const auto initiator=p.value("initiator",Json::object());r.initiator=clean_url(str(initiator,"url"));
            if(initiator.contains("stack")) {
                const auto frames=initiator["stack"].value("callFrames",Json::array());
                if(!frames.empty())r.initiator=clean_url(str(frames[0],"url"));
            }
            requests[key]=report.browser.requests.size();report.browser.requests.push_back(std::move(r));
        } else if (method == "Network.webSocketHandshakeResponseReceived" || method == "Network.webSocketFrameError" || method == "Network.webSocketClosed") {
            const auto it=requests.find(key_of(message,p));if(it==requests.end())return;
            auto& r=report.browser.requests[it->second];
            if(r.type!="WebSocket")return;
            if(method=="Network.webSocketHandshakeResponseReceived") {
                r.status=p.value("response",Json::object()).value("status",0);r.state=r.status==101?"open":"failed";
                if(r.status!=101)r.error="WebSocket handshake answered HTTP "+std::to_string(r.status);
            } else if(method=="Network.webSocketFrameError") {
                if(r.state=="pending"){r.state="failed";r.error=printable(str(p,"errorMessage"),240);}
            } else if(r.state=="pending"){r.state="failed";r.error="WebSocket closed before a handshake response was observed";}
            else if(r.state=="open")r.state="closed";
        } else if (method=="Page.frameNavigated" && sid==session && !p.at("frame").contains("parentId")) {
            page.final_url=clean_url(str(p.at("frame"),"url"));
            page.javascript_enabled=page.javascript_enabled||http_url(page.final_url);
        } else if (method=="Page.loadEventFired" && sid==session) {
            loaded=true;page.javascript_enabled=page.javascript_enabled||http_url(page.final_url);
        }
        else if(method=="Page.javascriptDialogOpening") browser.send("Page.handleJavaScriptDialog",{{"accept",false}},sid);
    };
    browser.call("Target.setAutoAttach",{{"autoAttach",true},{"waitForDebuggerOnStart",true},{"flatten",true}},session);
    notify(ActivityState::Running,"Browser " + page.url,collected_page?"Executing the collected HTTP response; observing live script requests":"Executing the page and observing scripts loaded by JavaScript");
    const auto navigated=browser.call("Page.navigate",{{"url",target.url()}},session,10000);
    if(!str(navigated,"errorText").empty())throw std::runtime_error(str(navigated,"errorText"));
    const auto deadline=std::min(web_deadline,began+std::chrono::milliseconds(options.browser_timeout_ms));
    Json message;
    while(!cancel && Clock::now()<deadline && !limited) {
        if(browser.receive(message,std::min(deadline,Clock::now()+std::chrono::milliseconds(100))))browser.on_message(message);

    }
    const bool settled=loaded && live.empty() && bodies.empty() && streams.empty() && Clock::now()-last_event>=std::chrono::milliseconds(1500);
    if(cancel){
        page.state="cancelled";page.reason="Browser inspection cancelled; partial observations retained";
        for(const auto& key:live)if(auto i=requests.find(key);i!=requests.end()) {
            auto& r=report.browser.requests[i->second];r.state="cancelled";r.error="Cancelled during browser observation";
        }
        while(!streams.empty())finish_stream(streams.begin()->first,true);
        for(const auto& [id,index]:bodies){(void)id;resource_result(index,Json::object());}
        return;
    }
    const bool web_expired=Clock::now()>=web_deadline;
    page.state=web_expired||limited||capped?"limited":settled?"complete":"timed_out";
    page.reason=web_expired?"Web evidence time limit reached":limited||capped?"Browser request or transfer limit reached":settled?"Page loaded; network settled within the observation window":"Observation window ended; later loads may be missing";
    if(!web_expired)try {
        std::string expression=runtime_expression;
        if(document_observer)for(size_t at=0;(at=expression.find("524288",at))!=std::string::npos;at+=7)expression.replace(at,6,"3145728");
        expression="(() => {const page="+expression+";page.signatures="+web_signatures().runtime_expression()+";return page;})()";
        const auto evaluated=browser.call("Runtime.evaluate",{{"expression",expression},{"returnByValue",true},{"timeout",3000}},session,4000);
        if(evaluated.contains("exceptionDetails"))throw std::runtime_error("Rendered page inspection raised an exception");
        const auto value=evaluated.at("result").value("value",Json::object());
        page.final_url=clean_url(str(value,"url"));page.title=printable(str(value,"title"),300);
        if(!page.final_url.starts_with("http://")&&!page.final_url.starts_with("https://"))throw std::runtime_error("No HTTP page was rendered");
        page.javascript_enabled=true;page.dom_truncated=value.value("truncated",false);
        HttpObservation dom;dom.url=str(value,"url");dom.title=page.title;dom.status=page.status;dom.body=str(value,"html");
        const size_t dom_limit=document_observer?3*1024*1024:512*1024;
        if(dom.body.size()>dom_limit){dom.body.resize(dom_limit);page.dom_truncated=true;}
        dom.body_truncated=page.dom_truncated;
        dom.headers.emplace_back("content-type","text/html");std::vector<Finding> found;
        if(document_observer&&dom.status>=200&&dom.status<300)document_observer(dom);
        dom.url=page.final_url;
        identify_http(dom,found);
        identify_vendors(dom,report,"Rendered DOM"+origin_label+" / ");
        collect_api_references(dom,report,"Rendered DOM"+origin_label+" / ");
        for(auto& f:found){for(auto& e:f.evidence)e.source="Rendered DOM"+origin_label+" / "+e.source;add_finding(report.findings,std::move(f));}
        if(value.contains("values")&&value["values"].is_array())for(const auto& v:value["values"]) {
            const auto version=str(v,"version");
            add_finding(report.findings,{str(v,"name"),"Library",version,v.value("confidence",std::string("High")),
                {{"Browser runtime"+origin_label+" / "+page.final_url,str(v,"evidence")+(version.empty()?"":" = "+version)}}});
        }
        if(value.contains("signatures")) {
            web_signatures().runtime(value["signatures"],"Browser runtime"+origin_label+" / "+page.final_url,report.findings);
            if(value["signatures"].value("limited",false)) {
                page.reason+="; additional DOM checks reached their per-selector limit or a selector failed";
                if(page.state=="complete")page.state="partial";
            }
        }
    } catch(const std::exception& e) {page.reason += "; " + std::string(e.what()); if(page.state=="complete")page.state="partial";}

    if(page.state=="complete" && (!live.empty()||!bodies.empty()||!streams.empty())) {
        page.state="partial";page.reason="Late requests remained active when the browser observation window ended";
    }
    if(page.status>=400) {
        if(page.state=="complete")page.state="http_error";
        page.reason+="; main document returned HTTP "+std::to_string(page.status);
    }
    while(!streams.empty())finish_stream(streams.begin()->first,true);
    for(const auto& [id,index]:bodies) {
        (void)id;const auto& r=report.browser.requests[index];ScriptObservation s;s.url=r.url;s.referenced_by=page.url;s.collection="browser";
        s.state="unavailable";s.reason="Browser response body not received before collection ended";s.status=r.status;
        (r.type=="Stylesheet"?report.stylesheets:report.scripts).push_back(std::move(s));
    }
    for(const auto& key:live)if(auto i=requests.find(key);i!=requests.end()) {
        auto& r=report.browser.requests[i->second];if(r.state=="pending"||r.state=="response"){r.state="interrupted";r.error="Browser observation ended before request completion was observed";}
    }
}
#endif
}

void inspect_browser(const Target& target, ScanReport& report, const ScanOptions& options, std::atomic_bool& cancel, const ScanObserver& observer,Clock::time_point deadline,const HttpObservation* collected_page,const std::function<void(const HttpObservation&)>& document_observer) {
    if(!options.browser_runtime){report.browser.state="disabled";report.browser.reason="Browser execution disabled by CLI option";return;}
    if(cancel)return;
#ifdef _WIN32
    if(report.browser.state=="unavailable")return;
    const auto executable=browser_path();
    if(executable.empty()) {
        report.browser.state="unavailable";report.browser.reason="No installed Edge or Chrome found; only static evidence was collected";
        if(observer)observer({ScanStage::Web,ActivityState::Skipped,0,"Browser runtime",report.browser.reason});
        return;
    }
    double elapsed=0;for(const auto& p:report.browser.pages)elapsed+=p.elapsed_ms;
    if(elapsed>=45000 || report.browser.pages.size()>=8) {
        report.browser.state="limited";report.browser.reason="Browser inspection budget reached (45 seconds / eight entry pages)";return;
    }
    const auto started=Clock::now();
    const auto page_url=clean_url(target.url());

    const auto address=target.host==report.target.host?report.address:
        (collected_page&&collected_page->url==target.url()?collected_page->address:std::string{});
    if(observer)observer({ScanStage::Web,ActivityState::Running,0,"Browser "+page_url,"Starting an isolated browser for page JavaScript"});
    auto bounded=options;bounded.browser_timeout_ms=std::min(options.browser_timeout_ms,std::max(1000,static_cast<int>(45000-elapsed)));
    const HttpObservation* supplied=nullptr;

    for(int attempt=0;attempt<2;++attempt) {
        const auto attempt_started=Clock::now();
        BrowserPage page;page.url=page_url;
        page.document_source=supplied?"captured_http":"network";
        try {run_page(target,report,page,bounded,cancel,observer,executable,deadline,supplied,address,document_observer);}
        catch(const std::exception& e) {
            const bool expired=Clock::now()>=deadline;
            page.state=cancel?"cancelled":expired?"limited":page.status>=400?"http_error":"failed";
            page.reason=expired&&!cancel?"Web evidence time limit reached":printable(e.what(),400);
            if(page.status>=400)page.reason+="; main document returned HTTP "+std::to_string(page.status);
        }
        for(auto& request:report.browser.requests)if(request.page==page.url && (request.state=="pending"||request.state=="response")) {
            request.state=cancel?"cancelled":"interrupted";
            request.error=cancel?"Cancelled during browser observation":"Browser collection ended before request completion was observed";
        }
        page.elapsed_ms=std::chrono::duration<double,std::milli>(Clock::now()-attempt_started).count();
        if(supplied)page.reason=std::string(page.javascript_enabled?"Executed collected HTTP 200 HTML. ":"Attempted execution of collected HTTP 200 HTML. ")+"Static cookies are not replayed. "+page.reason;
        report.browser.state=page.state;report.browser.reason=page.reason;
        if(observer)observer({ScanStage::Web,cancel?ActivityState::Cancelled:page.state=="failed"||page.state=="http_error"?ActivityState::Failed:ActivityState::Complete,0,
            "Browser "+page.url,std::to_string(page.scripts_analyzed)+" loaded scripts analyzed / "+page.reason});
        const bool retry=!attempt&&!cancel&&(page.status>=400||(page.state=="failed"&&!page.javascript_enabled))&&
            Clock::now()<deadline&&report.browser.pages.size()+1<8&&usable_document(collected_page,target,options);
        report.browser.pages.push_back(std::move(page));
        if(!retry)break;
        const auto spent=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
        const int left=static_cast<int>(45000-elapsed-spent);
        if(left<1000)break;
        bounded.browser_timeout_ms=std::min(options.browser_timeout_ms,left);
        supplied=collected_page;
    }
    if(!cancel && std::any_of(report.browser.pages.begin(),report.browser.pages.end(),[&](const auto& p){return p.state!=report.browser.state;})) {
        report.browser.state="partial";report.browser.reason="Coverage varies across the inspected browser pages; see individual page results";
    }
#else
    (void)target; (void)observer; (void)deadline; (void)collected_page; (void)document_observer;
    report.browser.state="unavailable";report.browser.reason="Browser execution currently requires Windows with Edge or Chrome";
#endif
}
}
