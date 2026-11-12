#include "vector/model.hpp"
#include "vector/export.hpp"
#include "vector/overview.hpp"
#include "vector/endpoint_connections.hpp"
#include "vector/api_endpoints.hpp"
#include "progress_view.hpp"
#include "technology_icons.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
using namespace vectorapp;
const ImVec4 ink{0.91f,0.92f,0.94f,1}, muted{0.61f,0.64f,0.69f,1}, blue{0.49f,0.66f,0.96f,1};
const ImVec4 green{0.43f,0.78f,0.64f,1}, amber{0.88f,0.70f,0.43f,1}, red{0.96f,0.51f,0.51f,1};
const ImVec4 canvas{0.105f,0.113f,0.129f,1}, rail_color{0.080f,0.087f,0.100f,1};
const ImVec4 panel{0.13f,0.14f,0.16f,1}, line{0.22f,0.24f,0.28f,1};
ImFont *regular=nullptr,*bold=nullptr,*mono=nullptr;
float scale=1;
bool system_rounding=false;
SDL_HitTestResult SDLCALL window_hit_test(SDL_Window* window,const SDL_Point* p,void*) {
    int w=0,h=0;SDL_GetWindowSize(window,&w,&h);
    if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_MAXIMIZED)) {
        const int edge=static_cast<int>(7*scale);
        const bool l=p->x<edge,r=p->x>=w-edge,t=p->y<edge,b=p->y>=h-edge;
        if(t&&l)return SDL_HITTEST_RESIZE_TOPLEFT;if(t&&r)return SDL_HITTEST_RESIZE_TOPRIGHT;
        if(b&&l)return SDL_HITTEST_RESIZE_BOTTOMLEFT;if(b&&r)return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        if(l)return SDL_HITTEST_RESIZE_LEFT;if(r)return SDL_HITTEST_RESIZE_RIGHT;
        if(t)return SDL_HITTEST_RESIZE_TOP;if(b)return SDL_HITTEST_RESIZE_BOTTOM;
    }

    if(p->y<52*scale&&p->x<w-300*scale)return SDL_HITTEST_DRAGGABLE;
    return SDL_HITTEST_NORMAL;
}
void update_window_corners(SDL_Window* window) {
#ifdef _WIN32
    if(system_rounding)return;
    const auto hwnd=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
    if(!hwnd)return;
    if(SDL_GetWindowFlags(window)&SDL_WINDOW_MAXIMIZED){SetWindowRgn(hwnd,nullptr,TRUE);return;}
    RECT rect{};GetWindowRect(hwnd,&rect);
    const int diameter=static_cast<int>(32*scale);
    auto region=CreateRoundRectRgn(0,0,rect.right-rect.left+1,rect.bottom-rect.top+1,diameter,diameter);
    if(region&&!SetWindowRgn(hwnd,region,TRUE))DeleteObject(region);
#else
    (void)window;
#endif
}
void setup_window_chrome(SDL_Window* window) {
    if(!SDL_SetWindowHitTest(window,window_hit_test,nullptr))SDL_SetWindowBordered(window,true);
#ifdef _WIN32
    const auto hwnd=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
    auto library=LoadLibraryExW(L"dwmapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(hwnd&&library) {
        using SetAttribute=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
        auto set=reinterpret_cast<SetAttribute>(GetProcAddress(library,"DwmSetWindowAttribute"));
        if(set){const DWORD round=2;const BOOL dark=TRUE;const DWORD no_system_border=0xFFFFFFFE;system_rounding=SUCCEEDED(set(hwnd,33,&round,sizeof(round)));set(hwnd,20,&dark,sizeof(dark));set(hwnd,34,&no_system_border,sizeof(no_system_border));}
    }
    if(library)FreeLibrary(library);
#endif
    update_window_corners(window);
}

struct CaptureAction { Uint64 at=0;std::string kind,value;int x=0,y=0,key=0; };
void capture_action(SDL_Window* window,const CaptureAction& action) {
    SDL_Event event{};event.common.timestamp=SDL_GetTicksNS();
    const auto id=SDL_GetWindowID(window);
    if(action.kind=="focus") {event.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event.window.windowID=id;}
    else if(action.kind=="move") {event.type=SDL_EVENT_MOUSE_MOTION;event.motion.windowID=id;event.motion.x=static_cast<float>(action.x);event.motion.y=static_cast<float>(action.y);}
    else if(action.kind=="down"||action.kind=="up") {event.type=action.kind=="down"?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;event.button.windowID=id;event.button.button=SDL_BUTTON_LEFT;event.button.down=action.kind=="down";}
    else if(action.kind=="text") {event.type=SDL_EVENT_TEXT_INPUT;event.text.windowID=id;event.text.text=action.value.c_str();}
    else if(action.kind=="wheel") {event.type=SDL_EVENT_MOUSE_WHEEL;event.wheel.windowID=id;event.wheel.x=static_cast<float>(action.x);event.wheel.y=static_cast<float>(action.y);}
    else if(action.kind=="key-down"||action.kind=="key-up") {
        event.type=action.kind=="key-down"?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;event.key.windowID=id;
        event.key.key=static_cast<SDL_Keycode>(action.key);event.key.scancode=SDL_GetScancodeFromKey(event.key.key,nullptr);
        event.key.down=action.kind=="key-down";
    } else return;
    SDL_PushEvent(&event);
}
struct EndpointDetail {
    std::string key,title,detail;
    std::vector<Evidence> evidence;
};
struct App {
    std::array<char,4097> target{};
    ScanOptions inspection_options;
    ProxyConfig proxy;
    std::filesystem::path proxy_path=proxy_settings_path();
    bool show_settings=false,proxy_enabled=false,proxy_reveal=false,proxy_load_failed=false;
    std::array<char,1025> proxy_host{},proxy_user{},proxy_password{};
    std::array<char,8> proxy_port{};
    std::string proxy_error;
    nlohmann::json proxy_controls=nlohmann::json::array();
    void edit_proxy() {
        proxy_enabled=proxy.enabled;proxy_reveal=false;
        std::snprintf(proxy_host.data(),proxy_host.size(),"%s",proxy.host.c_str());
        std::snprintf(proxy_port.data(),proxy_port.size(),"%u",proxy.port);
        std::snprintf(proxy_user.data(),proxy_user.size(),"%s",proxy.username.c_str());
        std::snprintf(proxy_password.data(),proxy_password.size(),"%s",proxy.password.c_str());
        show_settings=true;show_help=false;
    }
    void save_proxy() {
        try {
            ProxyConfig value;bool port_given=false;
            auto text=trim(proxy_host.data());
            if(const auto scheme=text.find("://");scheme!=std::string::npos) {
                text.erase(0,scheme+3);
                while(!text.empty()&&text.back()=='/')text.pop_back();
            }
            if(!text.empty()) {
                try {value=parse_proxy(text);port_given=parse_proxy("https://"+text).port==value.port;}
                catch(const std::exception&) {if(proxy_enabled)throw;value=ProxyConfig{};}
            }
            value.scheme="auto";value.enabled=proxy_enabled;
            if(!port_given) {
                const std::string port=proxy_port.data();
                if(port.empty()||port.find_first_not_of("0123456789")!=std::string::npos||std::stoul(port)>65535||std::stoul(port)==0)
                    throw std::invalid_argument("Enter a proxy port from 1 to 65535.");
                value.port=static_cast<uint16_t>(std::stoul(port));
            }
            if(proxy_user[0])value.username=proxy_user.data();
            if(proxy_password[0])value.password=proxy_password.data();
            validate_proxy(value);save_proxy_settings(value,proxy_path);proxy=std::move(value);proxy_error.clear();proxy_load_failed=false;
            edit_proxy();notice=proxy.enabled?"Proxy saved. New inspections will use it.":"Proxy disabled. New inspections will connect directly.";
        }catch(const std::exception& e){proxy_error=e.what();notice.clear();}
    }
    std::array<char,128> filter{};
    std::array<char,128> vendor_filter{};
    int selected_vendor=-1;
    bool known_vendors_only=false;
    int selected=-1,tab=4;
    bool open_only=true,show_help=false,focus_target=true;
    std::atomic_bool cancel{false},running{false},ready{false};
    std::thread worker;
    std::mutex mutex;
    std::vector<ScanEvent> live_activity;
    ActivityView activity_view;
    bool activity_open=false;
    std::chrono::steady_clock::time_point inspection_started;
    std::string notice,error;
    std::optional<ScanReport> pending,report;
    std::vector<EndpointHost> endpoint_hosts;
    size_t endpoint_host_limit=5,endpoint_dns_limit=5,infrastructure_limit=5;
    std::string selected_endpoint_row;
    std::optional<EndpointDetail> endpoint_detail;
    bool focus_endpoint_detail=false;
    float endpoint_list_scroll=0;
    nlohmann::json endpoint_row_bounds=nlohmann::json::array();
    nlohmann::json endpoint_control_bounds=nlohmann::json::array();
    std::vector<ApiEndpoint> api_endpoints;
    bool api_endpoints_truncated=false,focus_api_detail=false,api_section_shown=false;
    size_t api_endpoint_limit=5;
    std::optional<EndpointDetail> api_detail;
    float api_list_scroll=0;
    nlohmann::json api_row_bounds=nlohmann::json::array();
    nlohmann::json api_control_bounds=nlohmann::json::array();
    void load_api_endpoints() {
        auto list=report?api_endpoint_list(*report):ApiEndpointList{};
        api_endpoints=std::move(list.rows);api_endpoints_truncated=list.truncated;
    }
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* host_favicon=nullptr;
    std::string host_favicon_key;
    void clear_host_favicon() {
        if(host_favicon){SDL_DestroyTexture(host_favicon);host_favicon=nullptr;}
        host_favicon_key.clear();
    }
    void reset_endpoint_view() {
        endpoint_host_limit=endpoint_dns_limit=infrastructure_limit=5;selected_endpoint_row.clear();endpoint_detail.reset();focus_endpoint_detail=false;
        endpoint_row_bounds=nlohmann::json::array();endpoint_control_bounds=nlohmann::json::array();
        api_endpoint_limit=5;api_detail.reset();focus_api_detail=false;
        api_row_bounds=nlohmann::json::array();api_control_bounds=nlohmann::json::array();
    }
    ~App() { cancel=true; if(worker.joinable())worker.join(); clear_host_favicon(); }
    void start() {
        if(running) return;
        try {
            const auto parsed=parse_target(target.data());
            if(proxy_load_failed)throw std::runtime_error("Open Settings and save the proxy configuration before inspecting.");
            auto options=inspection_options;options.proxy=proxy;validate_proxy(options.proxy);
            if(worker.joinable()) worker.join();
            error.clear();notice.clear();report.reset();clear_host_favicon();endpoint_hosts.clear();load_api_endpoints();reset_endpoint_view();selected=-1;selected_vendor=-1;cancel=false;ready=false;live_activity.clear();activity_view={};running=true;show_help=false;
            show_settings=false;inspection_started=std::chrono::steady_clock::now();
            worker=std::thread([this,parsed,options] {
                try {
                    auto result=scan(parsed,options,cancel,{},[this](const ScanEvent& event) { std::lock_guard lock(mutex);live_activity.push_back(event); });
                    { std::lock_guard lock(mutex); pending=std::move(result); }
                } catch(const std::exception& e) {
                    ScanReport result; result.target=parsed;result.status="error";result.error=e.what();
                    std::lock_guard lock(mutex);pending=std::move(result);
                }
                ready=true;
            });
        }catch(const std::exception& e){error=e.what();}
    }
    void collect() {
        if(!ready.exchange(false))return;
        if(worker.joinable())worker.join();
        {std::lock_guard lock(mutex);report=std::move(pending);pending.reset();}
        running=false;clear_host_favicon();
        if(report) {
            selected=-1;
            selected_vendor=-1;
            endpoint_hosts=vectorapp::endpoint_hosts(*report);
            load_api_endpoints();
            reset_endpoint_view();
        }
    }
    void demo() {
        if(running)return;
        selected_vendor=-1;clear_host_favicon();
        report=demo_report();selected=0;tab=4;error.clear();notice.clear();activity_view={};show_help=false;focus_target=false;
        endpoint_hosts=vectorapp::endpoint_hosts(*report);load_api_endpoints();reset_endpoint_view();
    }
};
void text(const std::string& s) { ImGui::TextUnformatted(s.c_str()); }
void wrapped(const std::string& s,ImVec4 color=ink) { ImGui::PushStyleColor(ImGuiCol_Text,color);ImGui::PushTextWrapPos(0);text(s);ImGui::PopTextWrapPos();ImGui::PopStyleColor(); }
void label(const char* s) { ImGui::TextColored(muted,"%s",s); }
void heading(const std::string& s,float size=23) { ImGui::PushFont(bold,size*scale);ImGui::PushTextWrapPos(0);text(s);ImGui::PopTextWrapPos();ImGui::PopFont(); }
void data_text(const std::string& s,ImVec4 color=ink) { ImGui::PushFont(mono,14*scale);wrapped(s,color);ImGui::PopFont(); }
std::string report_brand_icon(const ScanReport& r) {
    for(const auto& f:r.findings)if(!technology_icon_key(f.name).empty())return f.name;
    for(const auto& v:r.vendors)if(!technology_icon_key(v.name).empty())return v.name;
    return {};
}
SDL_Texture* ensure_host_favicon(App& app,const ScanReport& r) {
    const auto key=r.favicon.url+"|"+std::to_string(r.favicon.body.size());
    if(app.host_favicon_key==key)return app.host_favicon;
    app.clear_host_favicon();
    app.host_favicon_key=key;
    if(!app.renderer||r.favicon.body.size()<8)return nullptr;
    const auto* bytes=reinterpret_cast<const unsigned char*>(r.favicon.body.data());

    if(bytes[0]==0x89&&bytes[1]=='P'&&bytes[2]=='N'&&bytes[3]=='G') {
        if(auto* io=SDL_IOFromConstMem(r.favicon.body.data(),static_cast<size_t>(r.favicon.body.size()))) {
            if(auto* surface=SDL_LoadPNG_IO(io,true)) {
                app.host_favicon=SDL_CreateTextureFromSurface(app.renderer,surface);
                SDL_DestroySurface(surface);
                if(app.host_favicon) {
                    SDL_SetTextureBlendMode(app.host_favicon,SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(app.host_favicon,SDL_SCALEMODE_LINEAR);
                }
            }
        }
    }
    return app.host_favicon;
}
void report_host_heading(App& app,const ScanReport& r) {
    const float icon_size=36*scale;
    const auto origin=ImGui::GetCursorScreenPos();
    auto* draw=ImGui::GetWindowDrawList();
    if(auto* texture=ensure_host_favicon(app,r)) {
        draw->AddRectFilled(origin,{origin.x+icon_size,origin.y+icon_size},IM_COL32(244,245,247,255),icon_size*.19f);
        const float inset=icon_size*.12f;
        draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture)),
            {origin.x+inset,origin.y+inset},{origin.x+icon_size-inset,origin.y+icon_size-inset});
    } else {
        const auto brand=report_brand_icon(r);
        technology_icon(brand.empty()?r.target.host:brand,brand.empty()?"Provider":"Platform",origin,icon_size);
    }
    ImGui::Dummy({icon_size+12*scale,icon_size});
    ImGui::SameLine(0,0);
    const float title_y=ImGui::GetCursorPosY()+std::max(0.0f,(icon_size-ImGui::GetFontSize()*1.35f)*.5f);
    ImGui::SetCursorPosY(title_y);
    heading(r.target.host,27);
}
void gap(float n=8) { ImGui::Dummy(ImVec2(0,n*scale)); }
double flowing_clock=0;
float flowing_pace=0;
bool flowing_animated=true;
void update_flowing_motion(SDL_Window* window) {
    bool enabled=true;
#ifdef _WIN32
    static double checked=-1;
    static BOOL client_animation=TRUE;
    if(checked<0||ImGui::GetTime()-checked>=1) {
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&client_animation,0);
        checked=ImGui::GetTime();
    }
    enabled=client_animation!=FALSE;
#endif
    flowing_animated=enabled&&!(SDL_GetWindowFlags(window)&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED));
    if(flowing_animated)flowing_clock+=std::min(ImGui::GetIO().DeltaTime,.05f);
}

void flowing_rule(float width=0) {
    if(width<=0)width=ImGui::GetContentRegionAvail().x;
    if(width<8*scale)return;
    const float thickness=std::max(1.5f,2.0f*scale);
    flowing_pace=0.28f*(width+220*scale);
    const ImVec2 origin=ImGui::GetCursorScreenPos();

    ImGui::Dummy({width,thickness+4*scale});
    auto* draw=ImGui::GetWindowDrawList();
    const float y=origin.y+1.5f*scale;
    const float x0=origin.x,x1=origin.x+width;
    draw->AddRectFilled({x0,y},{x1,y+thickness},ImGui::GetColorU32(ImVec4(0.28f,0.30f,0.34f,0.55f)),thickness);
    if(!flowing_animated)return;
    const float t=static_cast<float>(flowing_clock);
    for(int band=0;band<3;++band) {
        const float travel=std::fmod(t*0.28f+band*0.37f,1.0f);
        const float span=110*scale;
        const float center=x0-span+travel*(width+2*span);
        for(int step=0;step<12;++step) {
            const float u0=step/12.0f,u1=(step+1)/12.0f;
            const float sx0=center-span+u0*2*span;
            const float sx1=center-span+u1*2*span;
            if(sx1<=x0||sx0>=x1)continue;
            const float peak=1.0f-std::fabs(u0+u1-1.0f);
            const float alpha=0.08f+0.62f*peak*peak;
            draw->AddRectFilled({std::max(x0,sx0),y},{std::min(x1,sx1),y+thickness},
                ImGui::GetColorU32(ImVec4(0.78f,0.80f,0.86f,alpha)));
        }
    }
}
float window_corner_radius(SDL_Window* window) {
    if(SDL_GetWindowFlags(window)&(SDL_WINDOW_MAXIMIZED|SDL_WINDOW_FULLSCREEN))return 0;
#ifdef _WIN32
    using Arranged=BOOL(WINAPI*)(HWND);
    static const auto arranged=reinterpret_cast<Arranged>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"IsWindowArranged")));
    const auto hwnd=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
    if(arranged&&hwnd&&arranged(hwnd))return 0;
    return std::round((system_rounding?8.0f:16.0f)*scale);
#else
    return 0;
#endif
}
void flowing_outline(ImVec2 size,SDL_Window* window) {
    auto* draw=ImGui::GetForegroundDrawList();
    const float t=std::max(2.0f,std::round(2.0f*scale));
    const float w=std::floor(size.x),h=std::floor(size.y);
    if(w<8*t||h<8*t)return;
    const float corner=std::min(window_corner_radius(window),std::min(w,h)*0.25f);
    const float r=corner>t?corner:0.0f;
    const float mid=std::max(0.0f,r-t*0.5f),turn=1.5707963f*mid;
    const float side=r>0?r:t;
    const float across=w-2*r,down=h-2*side,perim=2*across+2*down+4*turn;
    if(across<=0||down<=0)return;
    const auto piece=[&](float from,float to,ImU32 color) {
        float start=0;
        for(int part=0;part<8;++part) {
            const float length=part%2?turn:part%4?down:across;
            const float a=std::max(from,start)-start,b=std::min(to,start+length)-start;
            start+=length;
            if(b<=a)continue;
            if(part==0)draw->AddRectFilled({r+a,0},{r+b,t},color);
            else if(part==2)draw->AddRectFilled({w-t,side+a},{w,side+b},color);
            else if(part==4)draw->AddRectFilled({w-r-b,h-t},{w-r-a,h},color);
            else if(part==6)draw->AddRectFilled({0,h-side-b},{t,h-side-a},color);
            else {
                const ImVec2 center{part==1||part==3?w-r:r,part==3||part==5?h-r:r};
                const float base=(part-3)*0.5f*1.5707963f;
                draw->PathArcTo(center,mid,base+a/mid,base+b/mid,std::max(2,static_cast<int>((b-a)/1.5f)+1));
                draw->PathStroke(color,0,t);
            }
        }
    };
    const auto span=[&](float from,float length,ImU32 color) {
        from=std::fmod(from,perim);if(from<0)from+=perim;
        if(from+length<=perim)piece(from,from+length,color);
        else{piece(from,perim,color);piece(0,from+length-perim,color);}
    };
    piece(0,perim,ImGui::GetColorU32(ImVec4(0.28f,0.30f,0.34f,0.55f)));
    if(!flowing_animated)return;
    const float reach=110*scale;
    const float pace=flowing_pace>0?flowing_pace:0.28f*(0.72f*w+2*reach);
    static double travelled=0;
    travelled=std::fmod(travelled+std::min(ImGui::GetIO().DeltaTime,.05f)*pace,static_cast<double>(perim)*3.0);
    for(int band=0;band<3;++band) {
        const float head=static_cast<float>(travelled)+band*perim/3.0f;
        for(int step=0;step<12;++step) {
            const float u0=step/12.0f,u1=(step+1)/12.0f;
            const float peak=1.0f-std::fabs(u0+u1-1.0f);
            span(head-reach+u0*2*reach,(u1-u0)*2*reach,ImGui::GetColorU32(ImVec4(0.78f,0.80f,0.86f,0.08f+0.62f*peak*peak)));
        }
    }
}
bool button(const char* title,ImVec2 size={},bool primary=false) {
    if(primary) {
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.22f,0.39f,0.69f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.27f,0.46f,0.78f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.18f,0.33f,0.60f,1));
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));
    }
    bool hit=ImGui::Button(title,size);
    if(primary)ImGui::PopStyleColor(4);
    return hit;
}
void theme() {
    ImGui::StyleColorsDark();
    auto& s=ImGui::GetStyle();
    s.WindowPadding=ImVec2(24,22);s.FramePadding=ImVec2(12,9);s.CellPadding=ImVec2(12,11);
    s.ItemSpacing=ImVec2(10,8);s.ItemInnerSpacing=ImVec2(8,6);
    s.FontSizeBase=16*scale;
    s.WindowRounding=16;s.ChildRounding=16;s.FrameRounding=12;s.PopupRounding=12;s.ScrollbarRounding=8;s.GrabRounding=8;
    s.WindowBorderSize=0;s.ChildBorderSize=0;s.FrameBorderSize=1;s.TabRounding=3;
    s.Colors[ImGuiCol_Text]=ink;s.Colors[ImGuiCol_TextDisabled]=muted;
    s.Colors[ImGuiCol_WindowBg]=canvas;
    s.Colors[ImGuiCol_ChildBg]=ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_FrameBg]=panel;
    s.Colors[ImGuiCol_FrameBgHovered]=ImVec4(0.17f,0.19f,0.22f,1);
    s.Colors[ImGuiCol_FrameBgActive]=ImVec4(0.18f,0.21f,0.26f,1);
    s.Colors[ImGuiCol_PopupBg]=panel;
    s.Colors[ImGuiCol_Border]=line;s.Colors[ImGuiCol_Separator]=line;
    s.Colors[ImGuiCol_Button]=ImVec4(0.18f,0.20f,0.23f,1);
    s.Colors[ImGuiCol_ButtonHovered]=ImVec4(0.24f,0.27f,0.31f,1);
    s.Colors[ImGuiCol_ButtonActive]=ImVec4(0.28f,0.31f,0.36f,1);
    s.Colors[ImGuiCol_Header]=ImVec4(0.15f,0.20f,0.28f,1);
    s.Colors[ImGuiCol_HeaderHovered]=ImVec4(0.18f,0.21f,0.25f,1);
    s.Colors[ImGuiCol_HeaderActive]=ImVec4(0.16f,0.21f,0.28f,1);
    s.Colors[ImGuiCol_TableHeaderBg]=panel;
    s.Colors[ImGuiCol_TableRowBg]=ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_TableRowBgAlt]=ImVec4(1,1,1,0.018f);
    s.Colors[ImGuiCol_TableBorderLight]=line;s.Colors[ImGuiCol_TableBorderStrong]=line;
    s.Colors[ImGuiCol_CheckMark]=blue;
    s.Colors[ImGuiCol_ScrollbarBg]=ImVec4(0,0,0,0);
    s.Colors[ImGuiCol_ScrollbarGrab]=ImVec4(0.28f,0.30f,0.34f,1);
    s.Colors[ImGuiCol_ScrollbarGrabHovered]=ImVec4(0.38f,0.40f,0.45f,1);
    s.Colors[ImGuiCol_ScrollbarGrabActive]=muted;
    s.Colors[ImGuiCol_PlotHistogram]=blue;
    s.Colors[ImGuiCol_TextSelectedBg]=ImVec4(0.28f,0.42f,0.65f,0.8f);
    s.Colors[ImGuiCol_NavCursor]=blue;
    s.ScaleAllSizes(scale);

    s.CircleTessellationMaxError=0.05f;
}
enum class Icon { Overview, Services, Software, Vendors, Evidence, Activity, Plus, Search, Help, Export, Settings };
void icon(Icon kind,ImVec2 p,float size,ImVec4 color) {
    auto* d=ImGui::GetWindowDrawList();const auto c=ImGui::GetColorU32(color);const float w=1.5f*scale;
    auto pt=[&](float x,float y){return ImVec2(p.x+x*size,p.y+y*size);};
    auto ln=[&](float x,float y,float xx,float yy){d->AddLine(pt(x,y),pt(xx,yy),c,w);};
    auto box=[&](float x,float y,float xx,float yy){d->AddRect(pt(x,y),pt(xx,yy),c,1.5f*scale,0,w);};
    switch(kind) {
        case Icon::Overview: box(.08f,.08f,.43f,.43f);box(.57f,.08f,.92f,.43f);box(.08f,.57f,.43f,.92f);box(.57f,.57f,.92f,.92f);break;
        case Icon::Services: box(.08f,.12f,.92f,.42f);box(.08f,.58f,.92f,.88f);ln(.20f,.27f,.28f,.27f);ln(.20f,.73f,.28f,.73f);break;
        case Icon::Software: ln(.1f,.28f,.5f,.08f);ln(.5f,.08f,.9f,.28f);ln(.9f,.28f,.5f,.48f);ln(.5f,.48f,.1f,.28f);ln(.1f,.50f,.5f,.70f);ln(.5f,.70f,.9f,.50f);ln(.1f,.72f,.5f,.92f);ln(.5f,.92f,.9f,.72f);break;
        case Icon::Vendors: d->AddCircle(pt(.22f,.5f),.15f*size,c,0,w);d->AddCircle(pt(.78f,.2f),.13f*size,c,0,w);d->AddCircle(pt(.78f,.8f),.13f*size,c,0,w);ln(.36f,.45f,.65f,.25f);ln(.36f,.55f,.65f,.75f);break;
        case Icon::Evidence: box(.18f,.06f,.82f,.94f);ln(.33f,.32f,.67f,.32f);ln(.33f,.50f,.67f,.50f);ln(.33f,.68f,.55f,.68f);break;
        case Icon::Activity: ln(0,.55f,.2f,.55f);ln(.2f,.55f,.36f,.15f);ln(.36f,.15f,.56f,.88f);ln(.56f,.88f,.74f,.4f);ln(.74f,.4f,.85f,.55f);ln(.85f,.55f,1,.55f);break;
        case Icon::Plus: ln(.5f,.15f,.5f,.85f);ln(.15f,.5f,.85f,.5f);break;
        case Icon::Search: d->AddCircle(pt(.42f,.42f),.28f*size,c,0,w);ln(.64f,.64f,.92f,.92f);break;
        case Icon::Help: d->AddCircle(pt(.5f,.5f),.43f*size,c,0,w);ln(.5f,.4f,.5f,.65f);d->AddCircleFilled(pt(.5f,.25f),w*.7f,c);break;
        case Icon::Export: ln(.18f,.58f,.18f,.86f);ln(.18f,.86f,.82f,.86f);ln(.82f,.86f,.82f,.58f);ln(.50f,.70f,.50f,.14f);ln(.32f,.32f,.50f,.14f);ln(.68f,.32f,.50f,.14f);break;
        case Icon::Settings: {
            const ImVec2 raw=pt(.5f,.5f);
            const ImVec2 ctr{std::floor(raw.x)+.5f,std::floor(raw.y)+.5f};
            constexpr int teeth=8,segments=48;
            const float tip=.475f*size,root=.355f*size,hole=.195f*size;
            constexpr float root_half=.26f,tip_half=.16f,turn=6.28318530718f;
            auto at=[&](float a,float r){return ImVec2(ctr.x+std::cos(a)*r,ctr.y+std::sin(a)*r);};
            d->PathClear();
            d->PathArcTo(ctr,(root+hole)*.5f,0,turn*float(segments-1)/float(segments),segments-1);
            d->PathStroke(c,root-hole,ImDrawFlags_Closed);
            for(int i=0;i<teeth;++i) {
                const float mid=-1.57079632679f+i*turn/float(teeth);
                const float base=root-.75f*scale;
                const ImVec2 tooth[4]={at(mid-root_half,base),at(mid-tip_half,tip),at(mid+tip_half,tip),at(mid+root_half,base)};
                d->AddConvexPolyFilled(tooth,4,c);
            }
            break;
        }
    }
}
bool navigation(const char* title,Icon glyph,bool selected,int count=-1) {
    const auto p=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
    const bool hit=ImGui::Selectable((std::string("##")+title).c_str(),selected,0,{width,38*scale});
    icon(glyph,{p.x+10*scale,p.y+11*scale},16*scale,selected?blue:muted);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddText({p.x+36*scale,p.y+9*scale},ImGui::GetColorU32(selected?ink:muted),title);
    if(count>=0) {
        const auto value=std::to_string(count);const auto tw=ImGui::CalcTextSize(value.c_str()).x;
        draw->AddText({p.x+width-tw-10*scale,p.y+9*scale},ImGui::GetColorU32(muted),value.c_str());
    }
    return hit;
}
void sidebar(App& app,float width,float height) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg,rail_color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12*scale,22*scale});
    ImGui::BeginChild("Navigation",ImVec2(width,height),ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPosX(22*scale);
    heading("Information",18);gap(20);
    const auto* r=app.report?&*app.report:nullptr;
    const int count_services=r?static_cast<int>(std::count_if(r->services.begin(),r->services.end(),[](const Service& v){return v.state=="open";})):-1;
    struct View { const char* name;Icon glyph;int tab,count; };
    const View views[]={{"Overview",Icon::Overview,4,-1},{"Software",Icon::Software,1,r?static_cast<int>(r->findings.size()):-1},
        {"Services",Icon::Services,0,count_services},{"Vendors",Icon::Vendors,3,r?static_cast<int>(r->vendors.size()):-1},
        {"Evidence",Icon::Evidence,2,-1},{"Activity",Icon::Activity,5,-1}};
    ImGui::BeginDisabled(!r||app.running);
    for(const auto& v:views)if(navigation(v.name,v.glyph,!app.show_help&&app.tab==v.tab,v.count)){app.tab=v.tab;app.show_help=false;}
    ImGui::EndDisabled();
    ImGui::EndChild();ImGui::PopStyleVar();ImGui::PopStyleColor();
}
void help() {
    heading("From response to identification");gap();
    wrapped("Vector inspects one IP address at a time. A hostname is resolved first; HTTP requests keep the hostname so virtual hosts and TLS SNI can work.");gap(18);
    heading("What gets inspected",18);gap();
    wrapped("TCP connections identify open ports. Server-first banners can reveal SSH, FTP, mail and VNC software. A port that stays silent gets one identification probe: a protocol handshake such as SMB, RDP, TLS, Redis or PostgreSQL, or a one-line HTTP request. The reply is kept as evidence. HTTP responses reveal web servers, framework headers, generator tags and library asset paths. HTTPS also exposes certificate metadata when the certificate verifies.");gap(18);
    heading("Reachability and location",18);gap();
    wrapped("Windows sends one ICMP echo to the selected IP with a 750 ms timeout. The report distinguishes an echo reply, TCP connections or refusals, and no response. A silent endpoint remains unknown, not offline. Cancellation can wait up to 750 ms for that one echo. Public IPs also get one HTTPS geolocation lookup from ipwho.is, capped at three seconds and 32 KiB; local/reserved addresses are not sent. The result is an approximate city, region and country, with source and date. Registration country comes from RDAP and is not device location. These metadata checks precede the separate 30-second web collection window.");gap(18);
    heading("Domain DNS",18);gap();
    wrapped("Public hostnames get four HTTPS lookups through Google Public DNS for CNAME, MX, NS and TXT, sharing a three-second limit. Each response is capped at 64 KiB and 64 records. Mail and authoritative-DNS records use the name you entered without a leading www label; other subdomains are not replaced with a guessed parent. The report retains record owners, values, TTLs, resolver and collection time. DNS roles describe domain configuration, not software proven to run on the selected IP. SPF only shows sending authorization; verification tokens do not establish product use. IP inputs and names resolving to local/reserved addresses skip this layer. These metadata queries are separate from the web budget.");gap(18);
    heading("Third party / subdomains",18);gap();
    wrapped("Third party / subdomains and DNS records under Endpoint each show five compact rows. Load more reveals the next five. Double-click a row, or focus it and press Enter, to read its full evidence; Back to connections returns to the lists. Browser responses, attempted requests, captured documents, page references and DNS configuration keep distinct labels. A/AAAA values come from system address resolution; returned CNAME, MX, NS and TXT records retain their TTL, resolver and date. Only descendants of the entered hostname (without an exact leading www) are called subdomains; other hosts do not establish company ownership. Dependencies of separately rendered disclosure pages are excluded. Opening details or loading more makes no requests or subdomain sweep.");gap(18);
    heading("API endpoints",18);gap();
    wrapped("API endpoints under Infrastructure lists URLs the site's front end calls or names. It starts with five rows and Load more reveals ten more. The line under the heading counts rows by source: seen in network requests, found in code, or named by API documents, page markup or robots.txt. Observed: the page's own code made the request in Vector's isolated browser (XHR, fetch, event stream, beacon or WebSocket) and a response arrived. Attempted: it tried and nothing answered. Answered: one of Vector's own page reads was answered with JSON. Described: listed in an OpenAPI or Swagger document the inspection already collected. Declared: named by page markup, such as a REST link, an API-shaped form action or an htmx request attribute. In code: a string literal in a collected script that is a request URL (fetch, axios, an API client's get or post, an ajax or request options object, $fetch, useSWR or new Request), a value under an endpoint or API path name, or an API-shaped path. robots.txt: an API-shaped rule in that file. Vector never calls a listed endpoint, so only Observed and Answered rows are confirmed to respond; a code reference can be inactive code or a bundled library's own endpoint. Query strings are left out. Identifier-like parts of observed URLs appear as {id}, and {} marks a value the code fills in at run time. A path shown without a host means the code did not state one. Double-click a row, or focus it and press Enter, for its evidence; Back to API endpoints returns to the list. Fetches answered with a page, static files, scripts, styles, images and fonts are not listed, and neither are literals naming an IP address or localhost. Limits: 256 rows, 64 references per script, the first 2 MiB of each script, and whatever the existing page, script and browser caps collected.");gap(18);
    heading("Reading confidence",18);gap();
    wrapped("High: a specific protocol or product signature was observed. Medium: a likely product, device or OS hint. Low: a weaker vendor marker. These labels describe rule strength, not statistical probability. Banners and headers can be changed or spoofed.");gap(18);
    heading("Following an inspection",18);gap();
    wrapped("The process line shows real scan stages. Select a step to inspect its operations, or choose Follow current step. Complete means that check finished; read its observation to see whether a response arrived. Skipped steps explain why they did not run. Cancellation preserves partial results and the activity timeline. Open Activity on a report to revisit it. On a completed report, New search in the header (or Ctrl+L) clears the report and returns to address entry so you can inspect another target. Settings stays available during an inspection; opening it does not cancel the scan, and Back to inspection returns to the live progress.");gap(18);
    heading("Exporting a report",18);gap();
    wrapped("Choose the export icon, then JSON, CSV or TXT. Each format saves the complete report, including evidence, endpoint details, providers, activity and collection limits. Copy to clipboard puts that same JSON on the clipboard and does not write a file. Filters do not affect exports. Files go in the reports folder beside Vector; the saved path appears after a file export. Partial and example reports keep their status labels.");gap(18);
    heading("What a target cannot tell you",18);gap();
    wrapped("Infrastructure shows named technologies and integrations. A supported source means the page explicitly accepts that service's URLs; a page reference names a resource, and observed contact means the browser received a response; requested, no response means the page asked for it and the connection failed on this machine. Service software, package versions and ports appear in Info. Use the Information sidebar to read the full report, including evidence and weaker policy or code hints.");gap();
    wrapped("A port number does not identify the service behind it. An IP can belong to a proxy, load balancer or many virtual machines. Physical model, hidden software and raw-packet OS fingerprints are outside this version. A full host inspection also sends SSDP, mDNS and SNMP probes. Web evidence has a shared cap of 12 page requests, 40 static script reads, eight stylesheet reads and 30 seconds across all ports. Redirects and failures count; ordinary page bodies are limited to 1 MiB; scripts and stylesheets to 512 KiB. Collected evidence is kept when a cap stops further collection. When the entered page links to documentation on another host of the same site (a docs or developers subdomain, or a docs link that redirects to one), that one page is read and run in the browser after the entered site: at most three requests of its own, inside the same 30 seconds. A name first seen there says which host it came from. One RDAP query retrieves public allocation metadata, not a subscriber mailbox.");gap(18);
    wrapped("Device discovery retains up to eight distinct SSDP replies from the inspected address, within 1.5 seconds at the default timeout, plus one InternetGatewayDevice search. LOCATION headers that point back at the inspected address are fetched (at most two documents, 64 KiB each) for manufacturer, model and friendly-name fields. A private LOCATION advertised on a public SSDP reply is fetched by pinning TCP to that same public address (still at most two documents). Vector also sends one unsolicited SNMPv3 discovery packet to learn the engineID enterprise and MAC when present, without credentials. When SNMP answers with community public, Vector also reads sysObjectID and sysName, then walks up to 24 ARP table rows the gateway has cached. Those neighbors are labeled as SNMP ARP hints, not a complete subnet map. When a same-host SSDP LOCATION publishes a TR-064 Hosts service, Vector reads at most 32 host-table rows. A private address also tries tr64desc.xml on port 49000. That list is the router's published table, not a subnet sweep.");gap(18);
    wrapped("After entry pages, linked documentation landing pages precede robots.txt and ordinary page links; guessed sitemaps are fallbacks. Source discovery recognizes document labels and relevant paths, and skips unrelated showcase slogans. The existing request and time limits still apply.");gap(18);
    heading("JavaScript execution",18);gap();
    wrapped("Website redirects can add or remove the exact www hostname label. Vector resolves that destination separately, retains its URL and address on the web evidence, and executes its page. The original endpoint and port results stay attached to the address you entered. Unrelated hostname redirects, disallowed ports and HTTPS downgrades remain outside this inspection; the same page and time caps apply.");gap();
    wrapped("Vector automatically opens one entry page per web endpoint in installed Edge or Chrome with a fresh temporary profile. It executes JavaScript and observes dynamically loaded scripts, including external hosts. Up to 256 browser requests, 160 scripts with 2 MiB analyzed each and 32 MiB transferred are collected per page. Streaming retains a 2 MiB prefix even from larger bundles. Up to eight already-loaded CSS responses per page are also analyzed, retaining at most 512 KiB each. Rendered resource elements supply page-reference evidence even when their network request fails. Reference parsing accepts up to 3 MiB of retained markup and 8,192 tags per document, with 256 relationships per report. If streaming is unavailable, the fallback retrieves bodies up to 4 MiB and records larger bodies as unavailable. A rejected static read also gets a normal browser attempt. The default observation window is 12 seconds, shortened by the shared 30-second web budget. Browser shutdown and report analysis may add a little time afterward. No clicks, scrolling or login are performed; interaction-gated or later loads may be missed. Browser coverage, loaded-by URLs and failures appear in Evidence. If no supported browser is available, static results remain and the missing runtime coverage is shown.");gap(18);
    wrapped("Additional web signatures combine advertised runtime versions with matching API properties, or inspect structured page markers. Each DOM selector checks up to 64 elements; reaching the limit is recorded. Rules retain their ID, revision and supporting evidence. Paired Set-Cookie names can suggest a framework; cookie values are never used or exported by these rules. Complete library license comments can supply an advertised version. These checks use the evidence already collected and share the existing browser time limit.");gap(18);
    wrapped("The browser can send requests and receive temporary cookies as page code runs. It uses no personal browser profile or saved credentials. Runtime reports omit query values, request bodies, cookie values and request headers. Account permissions remain unverified.");gap(18);
    wrapped("If browser navigation fails after a complete HTTP 200 HTML response was collected, Vector can execute that collected document in a separate fresh profile. Security headers are preserved; static cookies are not replayed. Evidence keeps the rejected request and labels the captured-document attempt separately. Both share the existing time limit and eight-attempt browser cap. Repeated static page URLs are collected once across ports and redirects.");gap(18);
    heading("Published providers",18);gap();
    wrapped("Published providers come from documents collected during this inspection. Vector follows subprocessor, privacy, legal, compliance and trust links, including explicitly linked HTTPS trust centers. Tables and structured provider cards retain their fields, source, date and discovery chain. One linked JavaScript source can be rendered. Limits: four disclosure transfers, five seconds of transfer time and 3 MiB per document, inside the shared page and time caps. There is no bundled company/provider list. Failed or unsupported sources supply no fallback names. These relationships do not prove software execution or account access. Select a provider for its source; Evidence lists collection outcomes.");gap(18);
    wrapped("Automatic coverage: a bare IPv4 or IPv6 address gets TCP ports 1-1024 plus common higher service ports, with up to 32 connections at once. Bare hostnames use 26 common ports with up to eight connections. An explicit URL or host:port stays on that endpoint; custom CLI lists stay limited to 256 ports. Settings contains only the optional proxy connection: host or IP, port and an optional username and password. There is no proxy type to choose. When an inspection starts, Vector asks the proxy how it answers and uses it as an HTTP, HTTPS or SOCKS5 proxy accordingly; if it answers as none of them the inspection stops with that message and nothing is sent directly. TCP and web collection use the saved proxy; ICMP and UDP discovery are skipped. Save applies to new inspections. The selected ports and completed count are exported. Standard web ports and an explicitly entered URL get a full HTTP(S) read. Other silent ports get one short identification probe. Printer port 9100 receives a connection check only.");gap();
    wrapped("Certificate verification stays on automatically. A failed verification is reported as an observation error. Reports omit page bodies and redact Set-Cookie values.",muted);
}
void empty_state(App& app) {
    const float width=std::min(720*scale,ImGui::GetContentRegionAvail().x-32*scale);
    const float x=ImGui::GetCursorPosX()+std::max(0.0f,(ImGui::GetContentRegionAvail().x-width)*.5f);
    const float card=(app.error.empty()?176.0f:228.0f)*scale;
    const float top=std::max(24*scale,(ImGui::GetContentRegionAvail().y-card)*.42f);
    gap(top/scale);
    ImGui::SetCursorPosX(x);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,panel);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{30*scale,28*scale});
    ImGui::BeginChild("Start inspection",{width,card},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar);
    heading("Inspect an address",30);gap(18);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{16*scale,16*scale});
    ImGui::PushStyleColor(ImGuiCol_FrameBg,canvas);
    ImGui::PushFont(regular,18*scale);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-134*scale);
    if(app.focus_target){ImGui::SetKeyboardFocusHere();app.focus_target=false;}
    const bool enter=ImGui::InputTextWithHint("##target","Paste a URL or IP address",app.target.data(),app.target.size(),ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();const bool start=button("Inspect",{124*scale,54*scale},true);
    ImGui::PopFont();ImGui::PopStyleColor();ImGui::PopStyleVar();
    if(!app.error.empty()){gap(7);wrapped(app.error,red);}
    if(enter||start){app.tab=4;app.show_help=false;app.start();}
    ImGui::EndChild();
    ImGui::PopStyleVar();ImGui::PopStyleColor();
}
void services(App& app,const ScanReport& r) {
    ImGui::Checkbox("Open ports only",&app.open_only);ImGui::SameLine();label("Names require a response match.");gap(4);
    int shown=0;
    const auto flags=ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable;
    const float height=std::max(140.0f*scale,ImGui::GetContentRegionAvail().y-64*scale);
    if(ImGui::BeginTable("Services",5,flags,ImVec2(0,height))) {
        ImGui::TableSetupColumn("Port",ImGuiTableColumnFlags_WidthFixed,65*scale);
        ImGui::TableSetupColumn("State",ImGuiTableColumnFlags_WidthFixed,110*scale);
        ImGui::TableSetupColumn("Identified service",ImGuiTableColumnFlags_WidthFixed,155*scale);
        ImGui::TableSetupColumn("Connect",ImGuiTableColumnFlags_WidthFixed,85*scale);
        ImGui::TableSetupColumn("Response / context");ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        for(const auto& s:r.services) {
            if(app.open_only && s.state!="open")continue;
            ++shown;ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%u",s.port);ImGui::TableNextColumn();
            ImGui::TextColored(s.state=="open"?green:s.state=="closed"?muted:amber,"%s",s.state.c_str());
            ImGui::TableNextColumn();text(s.name);ImGui::TableNextColumn();
            if(s.latency_ms>0)ImGui::Text("%.1f ms",s.latency_ms);else text("--");
            ImGui::TableNextColumn();
            auto detail=!s.banner.empty()?printable(s.banner):!s.error.empty()?s.error:s.name!="Unknown"?"Response observed; see Raw evidence.":"Convention: "+s.expected+" (unconfirmed)";
            wrapped(detail,muted);
        }
        ImGui::EndTable();
    }
    if(!shown)wrapped(r.status=="cancelled"?"No ports match this view before cancellation. The inspection did not finish.":app.open_only?"No open ports observed. Turn off the filter to see all connection results.":"No port observations were collected.",muted);
    else wrapped("A conventional port label is a hint about port usage. Unknown means no protocol signature was observed.",muted);
}
bool technology_selectable(const std::string& name,const std::string& category,bool selected) {
    const auto p=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x,height=26*scale;

    ImGui::PushStyleColor(ImGuiCol_Text,{0,0,0,0});
    const bool chosen=ImGui::Selectable(name.c_str(),selected,ImGuiSelectableFlags_SpanAllColumns,{0,height});
    ImGui::PopStyleColor();
    technology_icon(name,category,{p.x,p.y+scale},24*scale);
    const ImVec4 clip{p.x+32*scale,p.y,p.x+width,p.y+height};
    ImGui::GetWindowDrawList()->AddText(regular,17*scale,{clip.x,p.y+4*scale},ImGui::GetColorU32(ink),name.c_str(),nullptr,0,&clip);
    return chosen;
}
void technology_heading(const std::string& name,const std::string& category,float font_size=21) {
    const auto p=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x,wrap=std::max(40*scale,width-46*scale);
    ImGui::PushFont(bold,font_size*scale);
    const float height=std::max(36*scale,ImGui::CalcTextSize(name.c_str(),nullptr,false,wrap).y+6*scale);
    ImGui::PopFont();
    ImGui::Dummy({width,height});
    technology_icon(name,category,p,36*scale);
    ImGui::GetWindowDrawList()->AddText(bold,font_size*scale,{p.x+46*scale,p.y+3*scale},ImGui::GetColorU32(ink),name.c_str(),nullptr,wrap);
}
void finding_detail(const Finding& f) {
    technology_heading(f.name,f.category);gap(3);
    wrapped(f.category,muted);gap(12);
    label(f.category=="Fingerprint"?"Comparison value":"Version");data_text(f.version.empty()?"Not advertised":f.version);gap(10);
    if(f.category=="Fingerprint")wrapped("A comparison signal, not a named software identification.",muted);
    label("Signature confidence");ImGui::TextColored(f.confidence=="High"?green:amber,"%s",f.confidence.c_str());
    gap(20);heading("Observed evidence",17);gap(6);
    for(const auto& e:f.evidence) {wrapped(e.source,muted);gap(2);data_text(e.value);gap(14);}
    if(f.category.find("hint")!=std::string::npos)wrapped("An advertised hint does not verify the physical device or operating system.",amber);
}
void findings(App& app,const ScanReport& r) {
    ImGui::SetNextItemWidth(std::min(360*scale,ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##filter","Find software, version or category",app.filter.data(),app.filter.size());gap(10);
    const auto wanted=lower(app.filter.data());
    std::vector<int> visible;
    for(size_t i=0;i<r.findings.size();++i){
        const auto& f=r.findings[i];
        if(wanted.empty()||lower(f.name+" "+f.category+" "+f.version).find(wanted)!=std::string::npos)visible.push_back(static_cast<int>(i));
    }
    std::stable_partition(visible.begin(),visible.end(),[&](int i){return r.findings[i].category!="Fingerprint";});
    if(std::find(visible.begin(),visible.end(),app.selected)==visible.end())app.selected=visible.empty()?-1:visible.front();
    if(visible.empty()) {
        gap(20);
        wrapped(!wanted.empty()?"No matching software. Clear the filter to see every finding.":r.status=="cancelled"?"No software was identified before cancellation. This report is incomplete.":"No software signature matched the collected responses.",muted);
        if(!wanted.empty()&&button("Clear filter"))app.filter[0]=0;
        return;
    }
    const auto origin=ImGui::GetCursorScreenPos();
    const float total=ImGui::GetContentRegionAvail().x;
    const bool side=total>=920*scale;
    const float list_width=side?total*.58f:total;
    const float height=side?ImGui::GetContentRegionAvail().y:std::clamp(ImGui::GetContentRegionAvail().y*.45f,130*scale,240*scale);
    ImGui::BeginChild("Software list",{list_width,height});
    if(ImGui::BeginTable("Software",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,0})) {
        ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthStretch,1.35f);
        ImGui::TableSetupColumn("Category",ImGuiTableColumnFlags_WidthStretch,.9f);
        ImGui::TableSetupColumn("Version / value",ImGuiTableColumnFlags_WidthStretch,.75f);
        ImGui::TableSetupColumn("Confidence",ImGuiTableColumnFlags_WidthFixed,92*scale);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        for(int i:visible) {
            const auto& f=r.findings[i];ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();
            if(technology_selectable(f.name,f.category,app.selected==i))app.selected=i;
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",f.name.c_str());
            ImGui::TableNextColumn();wrapped(f.category,muted);ImGui::TableNextColumn();text(f.version.empty()?"--":f.version);
            ImGui::TableNextColumn();ImGui::TextColored(f.confidence=="High"?green:amber,"%s",f.confidence.c_str());ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    if(side)ImGui::SetCursorScreenPos({origin.x+list_width+20*scale,origin.y});else gap(16);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,panel);
    ImGui::BeginChild("Selected evidence",{side?total-list_width-20*scale:0,side?height:0},ImGuiChildFlags_AlwaysUseWindowPadding);
    finding_detail(r.findings[app.selected]);
    ImGui::EndChild();ImGui::PopStyleColor();
}
void vendor_detail(const VendorRelationship& v,bool compact=false) {
    const bool published=!v.published_by.empty();
    const auto source_label=published?"Published source":"Observed evidence";
    const auto origin=published?"Published by "+v.published_by:v.domain;
    const auto confidence=v.evidence_level+" / "+(published?"Document match: ":"Provider match: ")+v.confidence;
    if(compact) {

        technology_heading(v.name,"Provider",20);
        ImGui::TextColored(amber,"Account access unverified");
        wrapped(v.capability);gap(4);
        heading(source_label,17);
        for(const auto& e:v.evidence){wrapped(e.source,muted);data_text(e.value);gap(4);}
        gap(6);data_text(origin,muted);
        wrapped(v.relationship+" / "+v.category,muted);
        wrapped(confidence,muted);
        if(v.confidence=="Unknown")wrapped("A different origin does not prove a different owner. No provider is attributed to this domain.",amber);
        if(!v.attribution_source.empty()){gap(6);label("Provider reference");wrapped(v.attribution_source,muted);}
        return;
    }
    technology_heading(v.name,"Provider");gap(3);data_text(origin,muted);gap(14);
    label(v.relationship.c_str());wrapped(v.category,muted);
    gap(10);ImGui::TextColored(amber,"Account access unverified");
    gap(10);wrapped(v.capability);
    if(v.confidence=="Unknown"){gap(8);wrapped("A different origin does not prove a different owner. No provider is attributed to this domain.",amber);}
    gap(18);heading(source_label,17);gap(5);
    wrapped(confidence,muted);gap(5);
    for(const auto& e:v.evidence){wrapped(e.source,muted);data_text(e.value);gap(10);}
    if(!v.attribution_source.empty()){label("Provider reference");wrapped(v.attribution_source,muted);}
}
void vendors(App& app,const ScanReport& r) {
    ImGui::SetNextItemWidth(std::min(360*scale,ImGui::GetContentRegionAvail().x*.56f));
    ImGui::InputTextWithHint("##vendor-filter","Find a provider, domain or relationship",app.vendor_filter.data(),app.vendor_filter.size());
    ImGui::SameLine();ImGui::Checkbox("Known providers only",&app.known_vendors_only);
    gap(10);
    const auto wanted=lower(app.vendor_filter.data());
    std::vector<int> visible;
    for(size_t i=0;i<r.vendors.size();++i) {
        const auto& v=r.vendors[i];
        if(app.known_vendors_only&&v.confidence=="Unknown")continue;
        if(wanted.empty()||lower(v.name+" "+v.category+" "+v.domain+" "+v.relationship+" "+v.evidence_level).find(wanted)!=std::string::npos)visible.push_back(static_cast<int>(i));
    }
    const auto rank=[](const VendorRelationship& v){return v.evidence_level=="Observed response"?-1:v.evidence_level=="Page reference"||v.evidence_level=="Request attempt"?0:v.evidence_level=="Response header"?1:v.evidence_level=="Code hint"?2:3;};
    std::stable_sort(visible.begin(),visible.end(),[&](int a,int b){return rank(r.vendors[a])<rank(r.vendors[b]);});
    if(std::find(visible.begin(),visible.end(),app.selected_vendor)==visible.end())app.selected_vendor=visible.empty()?-1:visible.front();
    if(r.vendors_truncated)wrapped("Some references were omitted because a collection limit was reached.",amber);
    if(visible.empty()) {
        gap(20);
        wrapped(!wanted.empty()||app.known_vendors_only?"No matching relationships. Clear the filters to see every reference.":r.status=="cancelled"?"No vendor evidence was collected before cancellation. This inspection is incomplete.":"No vendor references found in the collected responses. Private account grants are not exposed by this inspection.",muted);
        if((!wanted.empty()||app.known_vendors_only)&&button("Clear filters")){app.vendor_filter[0]=0;app.known_vendors_only=false;}
        return;
    }
    const auto origin=ImGui::GetCursorScreenPos();
    const float total=ImGui::GetContentRegionAvail().x;
    const bool compact=total<920*scale;
    const bool side=total>=620*scale;
    const float list_width=side?total*(compact?.42f:.55f):total;
    const float height=side?ImGui::GetContentRegionAvail().y:std::clamp(ImGui::GetContentRegionAvail().y*.38f,100*scale,205*scale);
    ImGui::BeginChild("Vendor list",{list_width,height});
    if(ImGui::BeginTable("Vendor relationships",compact?1:3,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,0})) {
        ImGui::TableSetupColumn("Provider / origin",ImGuiTableColumnFlags_WidthStretch,1.45f);
        if(!compact){ImGui::TableSetupColumn("Relationship",ImGuiTableColumnFlags_WidthStretch,1.0f);ImGui::TableSetupColumn("Evidence basis",ImGuiTableColumnFlags_WidthStretch,1.0f);}
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        for(int i:visible) {
            const auto& v=r.vendors[i];ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();
            if(technology_selectable(v.name,"Provider",app.selected_vendor==i))app.selected_vendor=i;
            const auto description=v.published_by.empty()?v.domain:v.evidence_level+" / "+v.recorded_at.substr(0,10);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",v.name.c_str(),description.c_str());
            ImGui::PushFont(regular,13*scale);wrapped(description,muted);ImGui::PopFont();
            if(!compact){ImGui::TableNextColumn();wrapped(v.relationship);ImGui::TableNextColumn();wrapped(v.evidence_level,muted);}
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    if(side)ImGui::SetCursorScreenPos({origin.x+list_width+20*scale,origin.y});else gap(16);
    ImGui::PushStyleColor(ImGuiCol_ChildBg,panel);
    if(compact)ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16*scale,16*scale});
    ImGui::BeginChild("Vendor detail",{side?total-list_width-20*scale:0,side?height:0},ImGuiChildFlags_AlwaysUseWindowPadding);
    vendor_detail(r.vendors[app.selected_vendor],compact);ImGui::EndChild();
    if(compact)ImGui::PopStyleVar();ImGui::PopStyleColor();
}
std::string location_text(const Geolocation& g);
std::string icmp_text(const IcmpObservation& i);
void observations(const ScanReport& r) {
    ImGui::BeginChild("Observation scroll",ImVec2(0,0));
    if(ImGui::CollapsingHeader("Domain DNS",ImGuiTreeNodeFlags_DefaultOpen)) {
        wrapped(r.dns.state,r.dns.state=="complete"?green:muted);
        wrapped(r.dns.reason.empty()?"DNS records were not collected in this report.":r.dns.reason,muted);
        for(size_t i=0;i<r.dns.queries.size();++i) {
            const auto& q=r.dns.queries[i];ImGui::PushID(static_cast<int>(i));
            if(ImGui::TreeNode("dns_query","%s / %s / %zu %s",q.type.c_str(),q.state.c_str(),q.records.size(),q.records.size()==1?"record":"records")) {
                wrapped("Name: "+q.name);
                for(const auto& record:q.records) {
                    wrapped(record.type+" / "+record.name);data_text(record.value);
                    wrapped("TTL "+std::to_string(record.ttl)+" seconds",muted);gap(4);
                }
                if(!q.error.empty())wrapped(q.error,amber);
                if(!q.source.empty())wrapped("Resolver: "+q.source,muted);
                if(!q.checked_at.empty())wrapped("Checked: "+q.checked_at,muted);
                if(q.authenticated)wrapped("Resolver reported authenticated DNS data.",muted);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        gap(12);
    }
    if(ImGui::CollapsingHeader("Reachability and location",ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto reach=summarize_reachability(r);
        wrapped(reach.summary);wrapped("ICMP: "+icmp_text(r.icmp));
        if(!r.icmp.detail.empty())wrapped(r.icmp.detail,muted);
        if(!r.icmp.reply_address.empty())wrapped("Reply address: "+r.icmp.reply_address);
        if(r.icmp.ttl)wrapped("Reply TTL: "+std::to_string(r.icmp.ttl)+" (not an OS identification)",muted);
        wrapped("TCP: "+std::to_string(reach.tcp_open)+" open, "+std::to_string(reach.tcp_refused)+" refused, "+std::to_string(reach.tcp_no_response)+" no response, "+std::to_string(reach.tcp_other)+" incomplete/error.");
        wrapped("A TCP refusal can come from the endpoint or a firewall. No reply does not prove the endpoint is offline.",muted);
        gap(8);const auto& g=r.geolocation;
        if(g.state=="available") {
            wrapped("Approximate location: "+location_text(g));
            if(!g.timezone.empty())wrapped("Time zone: "+g.timezone);
            if(!g.isp.empty())wrapped("Provider ISP: "+g.isp);
            if(!g.asn.empty())wrapped("Provider ASN: "+g.asn);
            wrapped("IP-based estimate; not a device or street location.",muted);
        } else wrapped(g.error.empty()?"Geolocation was not collected in this report.":g.error,muted);
        if(!g.source.empty())wrapped("Source: "+g.source,muted);
        if(!g.checked_at.empty())wrapped("Checked: "+g.checked_at,muted);
        gap(12);
    }
    if(r.web_coverage.started) {
        const auto& c=r.web_coverage;
        wrapped("Web evidence: "+std::to_string(c.page_requests)+" / 12 page requests; "+std::to_string(c.script_requests)+" / 40 static scripts.",c.limited?amber:muted);
        wrapped("30-second collection budget shared by all ports and browser sessions.",muted);
        for(const auto& reason:c.reasons)wrapped(reason+". Collected evidence retained.",amber);
        gap(12);
    }
    if(!r.disclosures.empty()&&ImGui::CollapsingHeader("Published provider sources",ImGuiTreeNodeFlags_DefaultOpen)) {
        for(const auto& d:r.disclosures) {
            data_text(d.url);wrapped(d.state+" / "+std::to_string(d.providers)+" providers / "+d.checked_at,muted);
            wrapped(d.reason,d.state=="failed"?amber:muted);gap(8);
        }
    }
    if(ImGui::CollapsingHeader("Browser execution",ImGuiTreeNodeFlags_DefaultOpen)) {
        wrapped(r.browser.state+(r.browser.engine.empty()?"":" / "+r.browser.engine),r.browser.state=="complete"?green:amber);
        wrapped(r.browser.reason.empty()?"No eligible page was opened in a browser.":r.browser.reason,muted);
        for(const auto& p:r.browser.pages) {
            data_text(p.url);wrapped(std::to_string(p.scripts_analyzed)+" loaded scripts analyzed / "+p.state,muted);
            if(p.document_source=="captured_http")wrapped("Source: collected HTTP response",muted);
            if(!p.reason.empty())wrapped(p.reason,muted);
        }
        wrapped("Fresh browser profile. No clicks or login. Later loads can be missed.",muted);
        if(!r.browser.requests.empty()&&ImGui::CollapsingHeader("Browser requests")) {
            if(ImGui::BeginTable("Runtime network",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,210*scale})) {
                ImGui::TableSetupColumn("Request",ImGuiTableColumnFlags_WidthStretch,1.3f);
                ImGui::TableSetupColumn("Observed result",ImGuiTableColumnFlags_WidthStretch,1.f);
                ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
                for(const auto& q:r.browser.requests) {
                    ImGui::TableNextRow();ImGui::TableNextColumn();wrapped(q.method+" "+q.url);
                    if(!q.initiator.empty())wrapped("Loaded by "+q.initiator,muted);
                    ImGui::TableNextColumn();wrapped(q.type+" / "+q.state+(q.status?" / HTTP "+std::to_string(q.status):""));
                    if(q.collection=="captured_http")wrapped("Supplied from collected HTML; no new response",muted);
                    if(!q.error.empty())wrapped(q.error,amber);
                }
                ImGui::EndTable();
            }
        }
        if(r.browser.requests_truncated)wrapped("The browser request list reached its limit.",amber);
        gap(12);
    }
    if(!r.scripts.empty()&&ImGui::CollapsingHeader("JavaScript coverage",ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto analyzed=std::count_if(r.scripts.begin(),r.scripts.end(),[](const auto& s){return s.analyzed;});
        wrapped(std::to_string(analyzed)+" of "+std::to_string(r.scripts.size())+" script observations analyzed. Static reads and browser loads are labeled separately. Static reads take the page host's own scripts first, then scripts it declares on other public hosts.",muted);
        if(r.script_references_truncated)wrapped("The reference list reached its 512-entry limit.",amber);
        if(ImGui::BeginTable("Script coverage",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,220*scale})) {
            ImGui::TableSetupColumn("Script",ImGuiTableColumnFlags_WidthStretch,1.15f);
            ImGui::TableSetupColumn("Collection result",ImGuiTableColumnFlags_WidthStretch,1.0f);
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            for(const auto& s:r.scripts) {
                ImGui::TableNextRow();ImGui::TableNextColumn();wrapped(s.url);ImGui::TableNextColumn();
                wrapped(s.collection+" / "+s.state+(s.status?" / HTTP "+std::to_string(s.status):""),s.analyzed&&!s.body_truncated?green:amber);
                wrapped(s.reason,muted);
            }
            ImGui::EndTable();
        }
        gap(12);
    }
    if(!r.stylesheets.empty()&&ImGui::CollapsingHeader("Stylesheet coverage",ImGuiTreeNodeFlags_DefaultOpen)) {
        wrapped("Static: eight declared stylesheets, the page host's own first, then other public hosts. Browser: eight loaded stylesheets per page. Both analyze up to 512 KiB each within the web time limit.",muted);
        if(ImGui::BeginTable("Stylesheet results",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,160*scale})) {
            ImGui::TableSetupColumn("Stylesheet");ImGui::TableSetupColumn("Collection result");
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            for(const auto& s:r.stylesheets) {
                ImGui::TableNextRow();ImGui::TableNextColumn();wrapped(s.url);ImGui::TableNextColumn();
                wrapped((s.collection=="browser"?"Browser / ":"Static / ")+s.state+(s.status?" / HTTP "+std::to_string(s.status):""),s.analyzed&&!s.body_truncated?green:amber);
                wrapped(s.reason,muted);
            }
            ImGui::EndTable();
        }
        gap(12);
    }
    heading("Addresses",18);gap(3);
    for(const auto& a:r.addresses)wrapped(a+(a==r.address?"  / inspected":"  / DNS only"));
    gap(12);
    heading("Registry",18);gap(3);
    if(r.registry.scope.empty() && r.registry.error.empty()) wrapped("No registry query was made for this report.",muted);
    else if(r.registry.scope=="private") wrapped(r.registry.description,muted);
    else if(!r.registry.error.empty()) wrapped(r.registry.error,amber);
    else {
        if(!r.registry.source.empty()) wrapped("Source  "+r.registry.source);
        if(!r.registry.org.empty()) wrapped("Organization  "+r.registry.org);
        if(!r.registry.name.empty()) wrapped("Network  "+r.registry.name);
        if(!r.registry.cidr.empty()) wrapped("Prefix  "+r.registry.cidr);
        if(!r.registry.range.empty()) wrapped("Range  "+r.registry.range);
        if(!r.registry.asn.empty()) wrapped("ASN  "+r.registry.asn);
        if(!r.registry.country.empty()) wrapped("Registration country  "+r.registry.country);
        if(!r.registry.type.empty()) wrapped("Type  "+r.registry.type);
        if(!r.registry.status.empty()) wrapped("Status  "+r.registry.status);
        if(!r.registry.abuse_email.empty()) wrapped("Abuse email  "+r.registry.abuse_email);
        if(!r.registry.description.empty()) wrapped(r.registry.description,muted);
        for(const auto& c:r.registry.contacts) {
            std::string line=c.role.empty()?"contact":c.role;
            if(!c.name.empty()) line += "  "+c.name;
            if(!c.email.empty()) line += "  "+c.email;
            if(!c.phone.empty()) line += "  "+c.phone;
            wrapped(line);
        }
    }
    gap(12);
    for(size_t i=0;i<r.web.size();++i) {
        const auto& h=r.web[i];ImGui::PushID(static_cast<int>(i));
        auto title=h.url+"   "+(h.status?std::to_string(h.status):"No HTTP response");
        if(ImGui::CollapsingHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
            if(!h.title.empty())wrapped(h.title);
            if(!h.error.empty())wrapped(h.error,red);
            if(!h.redirect.empty())wrapped("Redirect: "+h.redirect,muted);
            if(h.url.starts_with("https:"))wrapped(h.tls_verified?"TLS certificate verified":"TLS certificate not verified",h.tls_verified?green:amber);
            if(h.body_truncated)wrapped("Page body limited to 1 MiB.",amber);
            if(ImGui::BeginTable("Headers",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthFixed,180*scale);ImGui::TableSetupColumn("Value");
                for(const auto& [k,v]:h.headers) {ImGui::TableNextRow();ImGui::TableNextColumn();text(k);ImGui::TableNextColumn();wrapped(v);}
                ImGui::EndTable();
            }
            if(!h.certificate.empty()&&ImGui::TreeNode("TLS certificate")) {
                for(const auto& [k,v]:h.certificate) {label(k.c_str());wrapped(v);gap(3);}ImGui::TreePop();
            }
        }ImGui::PopID();gap(8);
    }
    heading("Inspection notes",18);gap(4);
    for(const auto& n:r.notes){wrapped(n,muted);gap(7);}
    ImGui::EndChild();
}
void export_report(App& app,ReportFormat format) {
    if(!app.report)return;
    try {
        const std::string base=SDL_GetBasePath();
        auto directory=std::filesystem::path(std::u8string(base.begin(),base.end()))/"reports";
        std::error_code directory_error;
        std::filesystem::create_directories(directory,directory_error);
        if(directory_error)throw std::runtime_error("Could not create the reports folder beside Vector. Move Vector to a writable folder and try again.");
        auto stamp=utc_now();std::replace(stamp.begin(),stamp.end(),':','-');
        const auto extension=report_extension(format);
        auto path=directory/("vector-"+stamp+extension);
        for(size_t suffix=2;std::filesystem::exists(path);++suffix)
            path=directory/("vector-"+stamp+"-"+std::to_string(suffix)+extension);
        const auto utf8=path.u8string();const std::string saved_path(utf8.begin(),utf8.end());
        save_report(*app.report,saved_path,format);app.notice="Saved: "+saved_path;app.error.clear();
    }catch(const std::exception& e){app.notice.clear();app.error="Export failed. "+std::string(e.what());}
}
void copy_report(App& app) {
    if(!app.report)return;
    try {
        const auto text=report_export(*app.report,ReportFormat::Json);
        ImGui::SetClipboardText(text.c_str());
        app.notice="Copied JSON to the clipboard.";app.error.clear();
    }catch(const std::exception& e){app.notice.clear();app.error="Copy failed. "+std::string(e.what());}
}
void show_detail(App& app,int tab) { app.tab=tab;app.show_help=false; }
std::string location_text(const Geolocation& g) {
    std::string text;
    for(const auto* value:{&g.city,&g.region,&g.country})if(!value->empty()) {
        if(!text.empty())text+=", ";text+=*value;
    }
    return text;
}
std::string icmp_text(const IcmpObservation& i) {
    if(i.state=="reply")return "Echo reply / "+std::to_string(static_cast<int>(i.round_trip_ms))+" ms";
    if(i.state=="no_reply")return "No echo reply within 750 ms";
    if(i.state=="unreachable")return "Unreachable or TTL error reported";
    if(i.state=="cancelled")return "Cancelled";
    if(i.state=="not_checked")return "Not checked in this report";
    return "Check unavailable or failed";
}
std::string joined(const std::vector<std::string>& values) {
    std::string result;for(const auto& value:values){if(!result.empty())result+=" / ";result+=value;}return result;
}
void endpoint_evidence_row(App& app,const std::string& key,const std::string& title,const std::string& detail,const std::vector<Evidence>& evidence,
                           const std::string& tag={},bool api=false,float row_width=0,const std::string& label={}) {
    ImGui::PushID(key.c_str());
    const auto pos=ImGui::GetCursorScreenPos();
    const float width=row_width>0?row_width:ImGui::GetContentRegionAvail().x,height=24*scale;
    const float tag_width=tag.empty()?0.0f:regular->CalcTextSizeA(13*scale,FLT_MAX,0,tag.c_str()).x;
    const float text_width=width-(tag.empty()?0.0f:tag_width+14*scale);
    ImGui::PushFont(mono,13*scale);
    const bool clicked=ImGui::Selectable("##endpoint_row",app.selected_endpoint_row==key,ImGuiSelectableFlags_AllowDoubleClick,{width,height});
    if(clicked)app.selected_endpoint_row=key;
    if((clicked&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))||
       (ImGui::IsItemFocused()&&(ImGui::IsKeyPressed(ImGuiKey_Enter)||ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))) {
        if(api){app.api_detail=EndpointDetail{key,title,detail,evidence};app.focus_api_detail=true;app.api_list_scroll=ImGui::GetScrollY();}
        else {
            app.endpoint_detail=EndpointDetail{key,title,detail,evidence};app.focus_endpoint_detail=true;
            app.endpoint_list_scroll=ImGui::GetScrollY();
        }
    }
    std::string shown=label.empty()?title:label;
    std::replace(shown.begin(),shown.end(),'\n',' ');std::replace(shown.begin(),shown.end(),'\r',' ');std::replace(shown.begin(),shown.end(),'\t',' ');
    if(ImGui::CalcTextSize(shown.c_str()).x>text_width-8*scale) {
        const float available=std::max(0.0f,text_width-8*scale-ImGui::CalcTextSize("...").x);

        const char* begin=shown.c_str();const char* end=begin;
        mono->CalcTextSizeA(13*scale,available,0,begin,nullptr,&end);
        shown.resize(static_cast<size_t>(end-begin));shown+="...";
    }
    nlohmann::json bounds={{"key",key},{"title",title},{"detail",detail},{"shown",shown},{"x",pos.x},{"y",pos.y},{"width",width},{"height",height}};
    if(api){bounds["tag"]=tag;app.api_row_bounds.push_back(std::move(bounds));}
    else app.endpoint_row_bounds.push_back(std::move(bounds));
    const ImVec4 clip{pos.x,pos.y,pos.x+text_width,pos.y+height};
    ImGui::GetWindowDrawList()->AddText(mono,13*scale,{pos.x+4*scale,pos.y+5*scale},ImGui::GetColorU32(ink),shown.c_str(),nullptr,0,&clip);
    if(!tag.empty())ImGui::GetWindowDrawList()->AddText(regular,13*scale,{pos.x+width-tag_width-4*scale,pos.y+4*scale},ImGui::GetColorU32(muted),tag.c_str());
    ImGui::PopFont();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*32);
        text(title);wrapped(detail,muted);gap(3);wrapped("Double-click or press Enter for details.",muted);
        ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
    ImGui::PopID();
}
void endpoint_more(App& app,const char* key,size_t& limit,size_t total,nlohmann::json* bounds=nullptr,size_t step=5) {
    if(limit>=total)return;
    ImGui::PushID(key);gap(4);
    const std::string caption="Load more ("+std::to_string(total-limit)+" remaining)";
    const auto pos=ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{6*scale,3*scale});
    if(button(caption.c_str()))limit=std::min(total,limit+step);
    const auto size=ImGui::GetItemRectSize();
    (bounds?*bounds:app.endpoint_control_bounds).push_back({{"key",key},{"x",pos.x},{"y",pos.y},{"width",size.x},{"height",size.y}});
    ImGui::PopStyleVar();ImGui::PopID();
}
void api_endpoint_section(App& app,const ScanReport& r) {
    if(app.api_endpoints.empty()&&!r.web_coverage.started)return;
    app.api_section_shown=true;
    gap(16);heading(app.api_endpoints.empty()?std::string("API endpoints"):"API endpoints / "+std::to_string(app.api_endpoints.size()),17);
    ImGui::PushFont(regular,13*scale);
    if(app.api_detail) {
        const auto pos=ImGui::GetCursorScreenPos();
        const bool back=button("Back to API endpoints");const auto size=ImGui::GetItemRectSize();
        if(app.focus_api_detail){ImGui::SetScrollHereY(.15f);app.focus_api_detail=false;}
        app.api_control_bounds.push_back({{"key","api:back"},{"x",pos.x},{"y",pos.y},{"width",size.x},{"height",size.y}});
        if(back){app.api_detail.reset();ImGui::SetScrollY(app.api_list_scroll);}
        else {
            const auto& item=*app.api_detail;gap(8);data_text(item.title);wrapped(item.detail,muted);gap(12);
            for(const auto& e:item.evidence){wrapped(e.source,muted);data_text(e.value);gap(10);}
            ImGui::PopFont();return;
        }
    }
    if(app.api_endpoints.empty()) {
        wrapped("None found in the collected pages, scripts and browser requests.",muted);
        ImGui::PopFont();return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
    size_t network=0,coded=0,named=0;
    for(const auto& endpoint:app.api_endpoints) {
        if(endpoint.basis=="Code reference")++coded;
        else if(endpoint.basis=="Observed request"||endpoint.basis=="Collected response"||endpoint.basis=="Request attempt")++network;
        else ++named;
    }
    std::string sources;
    const auto count=[&](size_t n,const char* what){if(!n)return;if(!sources.empty())sources+=", ";sources+=std::to_string(n)+" "+what;};
    count(network,"seen in network requests");count(coded,"found in code");count(named,"named by API documents, page markup or robots.txt");
    wrapped(sources+". Vector calls none of them.",muted);gap(8);
    const float width=std::max(120*scale,ImGui::GetContentRegionAvail().x-24*scale);
    for(size_t i=0;i<std::min(app.api_endpoint_limit,app.api_endpoints.size());++i) {
        const auto& endpoint=app.api_endpoints[i];
        std::string detail=endpoint.scope;
        if(!endpoint.provider.empty())detail+=" / "+endpoint.provider;
        detail+=" / "+endpoint.basis;
        if(endpoint.status>0)detail+=" / HTTP "+std::to_string(endpoint.status);
        auto evidence=endpoint.evidence;
        evidence.push_back({"Meaning",api_endpoint_meaning(endpoint)});
        endpoint_evidence_row(app,"api:"+endpoint.method+" "+endpoint.url,trim(endpoint.method+" "+endpoint.url),detail,evidence,
            api_endpoint_tag(endpoint),true,width,endpoint.label);
    }
    endpoint_more(app,"api:more",app.api_endpoint_limit,app.api_endpoints.size(),&app.api_control_bounds,10);
    ImGui::PopStyleVar();
    if(app.api_endpoints_truncated){gap(4);wrapped("A collection limit was reached; more endpoints can exist.",amber);}
    ImGui::PopFont();
}
void endpoint_connections(App& app,const ScanReport& r) {
    gap(16);
    if(app.endpoint_detail) {
        ImGui::PushFont(regular,13*scale);
        const auto pos=ImGui::GetCursorScreenPos();
        const bool back=button("Back to connections");const auto size=ImGui::GetItemRectSize();
        if(app.focus_endpoint_detail){ImGui::SetScrollHereY(.15f);app.focus_endpoint_detail=false;}
        app.endpoint_control_bounds.push_back({{"key","detail:back"},{"x",pos.x},{"y",pos.y},{"width",size.x},{"height",size.y}});
        ImGui::PopFont();
        if(back){app.endpoint_detail.reset();ImGui::SetScrollY(app.endpoint_list_scroll);}
        else {
            const auto& item=*app.endpoint_detail;gap(8);data_text(item.title);wrapped(item.detail,muted);gap(12);
            ImGui::PushFont(regular,13*scale);
            for(const auto& e:item.evidence){wrapped(e.source,muted);data_text(e.value);gap(10);}
            ImGui::PopFont();return;
        }
    }
    heading("Third party / subdomains",16);gap(4);
    ImGui::PushFont(regular,13*scale);ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{0,0});
    if(app.endpoint_hosts.empty())wrapped("No additional hosts found in the collected evidence.",muted);
    for(size_t i=0;i<std::min(app.endpoint_host_limit,app.endpoint_hosts.size());++i) {
        const auto& host=app.endpoint_hosts[i];
        std::string detail=host.relation;
        if(!host.providers.empty())detail+=" / "+joined(host.providers);
        detail+=" / "+joined(host.roles);
        endpoint_evidence_row(app,"host:"+host.host,host.host,detail,host.evidence);
    }
    endpoint_more(app,"hosts:more",app.endpoint_host_limit,app.endpoint_hosts.size());
    gap(16);heading("DNS records",16);gap(4);
    std::vector<EndpointDetail> dns;
    if(!r.target.is_ip)for(size_t i=0;i<r.addresses.size();++i) {
        const auto& address=r.addresses[i];const std::string type=address.find(':')==std::string::npos?"A":"AAAA";
        dns.push_back({"resolved:"+address,type+" / "+address,"System resolver / "+r.target.host,
            {{"Name",r.target.host},{"Resolved address",address},{"Source","System address resolution; DNS TTL and authoritative status are not available."}}});
    }
    for(size_t i=0;i<r.dns.queries.size();++i) {
        const auto& q=r.dns.queries[i];
        if(q.records.empty())dns.push_back({"query:"+std::to_string(i),q.type+" / "+q.name,q.state,
            {{"Result",q.error.empty()?(q.state=="no_records"?"No records returned.":q.state):q.error},{"Resolver",q.source},{"Collected",q.checked_at}}});
        for(size_t k=0;k<q.records.size();++k) {
            const auto& record=q.records[k];const auto providers=dns_record_providers(r,record);
            const auto role=record.type=="MX"?"Inbound mail":record.type=="NS"?"Authoritative DNS":record.type=="CNAME"?"Alias":"DNS text";
            const auto detail=(providers.empty()?std::string(role):joined(providers))+" / TTL "+std::to_string(record.ttl)+"s";
            dns.push_back({"dns:"+std::to_string(i)+":"+std::to_string(k),record.type+" / "+record.value,detail,
                {{"Owner",record.name},{"Record",record.value},{"Resolver",q.source},{"Collected",q.checked_at},{"Query state",q.state},
                 {"Meaning","Published DNS configuration; service availability and account activity are unverified."}}});
        }
    }
    for(size_t i=0;i<std::min(app.endpoint_dns_limit,dns.size());++i) {
        const auto& item=dns[i];endpoint_evidence_row(app,item.key,item.title,item.detail,item.evidence);
    }
    endpoint_more(app,"dns:more",app.endpoint_dns_limit,dns.size());
    if(r.dns.queries.empty())wrapped(r.dns.reason.empty()?"No DNS records collected.":r.dns.reason,muted);
    ImGui::PopStyleVar();ImGui::PopFont();
}
void overview(App& app,const ScanReport& r) {
    app.endpoint_row_bounds=nlohmann::json::array();
    app.endpoint_control_bounds=nlohmann::json::array();
    app.api_row_bounds=nlohmann::json::array();
    app.api_control_bounds=nlohmann::json::array();
    app.api_section_shown=false;
    ImGui::BeginChild("Key information",{0,0});
    if(ImGui::BeginTable("Key columns",2,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Infrastructure",ImGuiTableColumnFlags_WidthStretch,1.35f);
        ImGui::TableSetupColumn("Endpoint",ImGuiTableColumnFlags_WidthStretch,1.0f);
        ImGui::TableNextRow();ImGui::TableNextColumn();
        heading("Infrastructure",20);gap(12);
        const auto row=[](int id,const std::string& name,const std::string& detail) {
            ImGui::PushID(id);
            const auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x-24*scale;
            const float wrap=std::max(80*scale,w-60*scale);
            ImGui::PushFont(bold,16*scale);
            const auto title_size=ImGui::CalcTextSize(name.c_str(),nullptr,false,wrap);
            ImGui::PopFont();ImGui::PushFont(regular,13*scale);
            const auto detail_size=ImGui::CalcTextSize(detail.c_str(),nullptr,false,wrap);
            ImGui::PopFont();
            const float height=std::max(44*scale,title_size.y+detail_size.y+14*scale);
            const bool selected=ImGui::Selectable("##infrastructure",false,0,{w,height});
            auto* draw=ImGui::GetWindowDrawList();
            technology_icon(name,detail,{p.x+8*scale,p.y+6*scale},32*scale);
            draw->AddText(bold,16*scale,{p.x+52*scale,p.y+4*scale},ImGui::GetColorU32(ink),name.c_str(),nullptr,wrap);
            draw->AddText(regular,13*scale,{p.x+52*scale,p.y+title_size.y+8*scale},ImGui::GetColorU32(muted),detail.c_str(),nullptr,wrap);
            ImGui::PopID();return selected;
        };
        const auto infrastructure=infrastructure_entries(r);

        for(size_t i=0;i<std::min(app.infrastructure_limit,infrastructure.size());++i) {
            const auto& entry=infrastructure[i];
            if(row(entry.vendor>=0?5000+entry.vendor:entry.finding,entry.name,entry.detail)) {
                if(entry.vendor>=0) {
                    app.vendor_filter[0]=0;app.known_vendors_only=false;app.selected_vendor=entry.vendor;show_detail(app,3);
                } else {
                    app.filter[0]=0;app.selected=entry.finding;show_detail(app,1);
                }
            }
        }
        ImGui::PushFont(regular,13*scale);
        endpoint_more(app,"infrastructure:more",app.infrastructure_limit,infrastructure.size());
        ImGui::PopFont();
        if(infrastructure.empty())wrapped("No technologies or integrations identified from the collected evidence.",muted);
        api_endpoint_section(app,r);
        const auto published=std::count_if(r.vendors.begin(),r.vendors.end(),[](const auto& v){return !v.published_by.empty();});
        if(published) {
            gap(16);heading("Published providers / "+std::to_string(published),17);gap(6);
            wrapped("Company relationships extracted from documents collected in this inspection.",muted);gap(6);
            for(size_t i=0;i<r.vendors.size();++i) {
                const auto& v=r.vendors[i];if(v.published_by.empty())continue;
                if(row(10000+static_cast<int>(i),v.name,v.evidence_level+" / "+v.recorded_at.substr(0,10))) {
                    app.vendor_filter[0]=0;app.known_vendors_only=false;app.selected_vendor=static_cast<int>(i);show_detail(app,3);
                }
            }
        }
        gap(24);heading("Info",20);gap(12);
        bool technical_findings=false;
        for(size_t i=0;i<r.findings.size();++i) {
            const auto& finding=r.findings[i];
            if(finding.category!="Service"&&finding.category!="Package")continue;
            technical_findings=true;
            if(row(static_cast<int>(i),finding.name,finding.category+(finding.version.empty()?"":" / "+finding.version))) {
                app.filter[0]=0;app.selected=static_cast<int>(i);show_detail(app,1);
            }
        }
        struct Protocol { std::string name, transport, ports; };
        std::vector<Protocol> protocols;
        for(const auto& service:r.services) {
            if(service.state!="open")continue;
            auto found=std::find_if(protocols.begin(),protocols.end(),[&](const auto& p){return p.name==service.name&&p.transport==service.protocol;});
            if(found==protocols.end())protocols.push_back({service.name,service.protocol,std::to_string(service.port)});
            else found->ports+=", "+std::to_string(service.port);
        }
        for(size_t i=0;i<protocols.size();++i) {
            const auto& p=protocols[i];
            if(row(-static_cast<int>(i)-1,p.name=="Unknown"?"Unidentified service":p.name,"Service / "+p.transport+" / "+p.ports))show_detail(app,0);
        }
        if(protocols.empty()&&!technical_findings)wrapped("No service details recorded.",muted);
        ImGui::TableNextColumn();
        heading("Endpoint",20);gap(12);
        ImGui::PushFont(regular,14*scale);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{0,4*scale});
        if(ImGui::BeginTable("Endpoint facts",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX)) {
            ImGui::TableSetupColumn("Fact",ImGuiTableColumnFlags_WidthFixed,112*scale);
            ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
            const auto fact=[](const char* name) {
                ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);wrapped(name,muted);ImGui::TableSetColumnIndex(1);
            };
            fact("Address");data_text(r.address.empty()?"Unresolved":r.address);
            const auto reach=summarize_reachability(r);
            fact("Reachability");wrapped(reach.summary,reach.state=="responding"?green:muted);
            if(reach.state=="unknown"&&ImGui::IsItemHovered())ImGui::SetTooltip("Silence does not establish that the endpoint is offline.");
            fact("Ping");wrapped(icmp_text(r.icmp),muted);
            if(reach.tcp_open+reach.tcp_refused+reach.tcp_no_response+reach.tcp_other) {
                fact("TCP checks");wrapped(std::to_string(reach.tcp_open)+" open / "+std::to_string(reach.tcp_refused)+" refused / "+std::to_string(reach.tcp_no_response)+" no reply"+(reach.tcp_other?" / "+std::to_string(reach.tcp_other)+" incomplete":""),muted);
            }
            if(reach.udp_replies){fact("UDP replies");wrapped(std::to_string(reach.udp_replies),muted);}
            fact("Approx. location");
            const auto& g=r.geolocation;
            if(g.state=="available") {
                wrapped(location_text(g)+" / "+(g.source=="Illustrative data"?g.source:std::string("ipwho.is")));
            } else {
                wrapped(g.state=="skipped"?"Not applicable to a local or reserved IP":g.state=="not_checked"?"Not collected in this report":"Location unavailable",muted);
                if(!g.error.empty()&&g.state!="skipped"&&ImGui::IsItemHovered())ImGui::SetTooltip("%s",g.error.c_str());
            }
            fact("Device / OS hints");
            bool identity=false;
            for(size_t i=0;i<r.findings.size();++i) {
                const auto& f=r.findings[i];if(f.category!="Device hint"&&f.category!="OS hint")continue;
                identity=true;ImGui::PushID(static_cast<int>(i)+20000);
                const std::string value=f.name+(f.version.empty()?"":" / "+f.version);
                const auto pos=ImGui::GetCursorScreenPos();
                const float width=ImGui::GetContentRegionAvail().x;
                const float wrap=std::max(40*scale,width-8*scale);
                const float height=std::max(22*scale,ImGui::CalcTextSize(value.c_str(),nullptr,false,wrap).y+6*scale);
                if(ImGui::Selectable("##endpoint_identity",false,0,{width,height})) {app.filter[0]=0;app.selected=static_cast<int>(i);show_detail(app,1);}
                ImGui::GetWindowDrawList()->AddText(regular,14*scale,{pos.x+4*scale,pos.y+3*scale},ImGui::GetColorU32(ink),value.c_str(),nullptr,wrap);
                if(ImGui::IsItemHovered())ImGui::SetTooltip("Advertised hint. Select to inspect its source.");
                ImGui::PopID();
            }
            if(!identity) {
                wrapped("Not identified",muted);
                if(ImGui::IsItemHovered())ImGui::SetTooltip("No device or OS signature was observed.");
            }
            if(!r.registry.org.empty()||!r.registry.name.empty()) {
                fact("Network owner");wrapped(r.registry.org.empty()?r.registry.name:r.registry.org);
            } else if(r.registry.scope=="private") {
                fact("Scope");wrapped("Private or local address");
            }
            if(!r.registry.cidr.empty()){fact("Network");data_text(r.registry.cidr);}
            if(!r.registry.asn.empty()){fact("ASN");data_text(r.registry.asn);}
            else if(!g.asn.empty()){fact("ASN / ipwho.is");data_text(g.asn);}
            if(!r.registry.country.empty()){fact("Registered in");wrapped(r.registry.country);}
            if(!r.registry.description.empty()&&r.registry.scope!="private") {fact("Registry details");wrapped(r.registry.description,muted);}
            if(!r.reverse_dns.empty()){fact("Reverse DNS");data_text(r.reverse_dns);}
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();ImGui::PopFont();
        endpoint_connections(app,r);
        ImGui::EndTable();
    }
    ImGui::EndChild();
}
void proxy_control(App& app,const char* key) {
    const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    app.proxy_controls.push_back({{"key",key},{"x",(a.x+b.x)*.5f},{"y",(a.y+b.y)*.5f},{"width",b.x-a.x},{"height",b.y-a.y}});
}
void proxy_settings(App& app) {
    const float panel_width=std::min(600*scale,ImGui::GetContentRegionAvail().x);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.0f,(ImGui::GetContentRegionAvail().x-panel_width)*.5f));
    ImGui::BeginChild("Proxy settings",{panel_width,0});
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{12*scale,6*scale});
    heading("Settings",26);ImGui::SameLine(width-190*scale);
    if(button("Back to inspection",{190*scale,34*scale})){app.show_settings=false;app.notice.clear();app.focus_target=!app.report;}
    proxy_control(app,"back");gap(12);
    wrapped("Send new inspections through a proxy. Enter its address; Vector works out whether it is an HTTP, HTTPS or SOCKS5 proxy when an inspection starts.",muted);gap(16);
    ImGui::Checkbox("Use a proxy",&app.proxy_enabled);proxy_control(app,"enabled");gap(12);
    ImGui::BeginDisabled(!app.proxy_enabled);
    const float host_width=width-140*scale;
    label("Host or IP");ImGui::SameLine(host_width+16*scale);label("Port");
    ImGui::SetNextItemWidth(host_width);
    ImGui::InputTextWithHint("##Proxy host","Hostname or IP, or paste host:port:user:pass",app.proxy_host.data(),app.proxy_host.size());proxy_control(app,"host");
    ImGui::SameLine(0,16*scale);ImGui::SetNextItemWidth(124*scale);
    ImGui::InputText("##Proxy port",app.proxy_port.data(),app.proxy_port.size(),ImGuiInputTextFlags_CharsDecimal);proxy_control(app,"port");gap(14);
    label("Username (optional)");ImGui::SetNextItemWidth(width);
    ImGui::InputText("##Proxy username",app.proxy_user.data(),app.proxy_user.size());proxy_control(app,"username");gap(10);
    label("Password");ImGui::SetNextItemWidth(width-90*scale);
    ImGui::InputText("##Proxy password",app.proxy_password.data(),app.proxy_password.size(),app.proxy_reveal?0:ImGuiInputTextFlags_Password);proxy_control(app,"password");
    ImGui::SameLine(0,12*scale);ImGui::Checkbox("Show",&app.proxy_reveal);proxy_control(app,"show");
    ImGui::EndDisabled();gap(16);
    wrapped("Credentials are protected by your Windows account. ICMP and UDP checks are skipped while a proxy is enabled.",muted);gap(16);
    if(!app.proxy_error.empty()){wrapped(app.proxy_error,red);gap(10);}
    if(button("Save proxy",{132*scale,38*scale},true))app.save_proxy();proxy_control(app,"save");
    if(!app.notice.empty()){gap(6);wrapped(app.notice,green);}
    ImGui::PopStyleVar();ImGui::EndChild();
}
void app_header(App& app,SDL_Window* window,bool& quit) {
    const float width=ImGui::GetIO().DisplaySize.x;
    ImGui::SetCursorPos({24*scale,18*scale});
    icon(Icon::Vendors,ImGui::GetCursorScreenPos(),20*scale,blue);
    ImGui::SetCursorPos({54*scale,16*scale});heading("Vector",19);

    const float cap=30*scale,cap_step=34*scale,cap_y=12*scale;
    const float gear_x=width-(108*scale+cap_step);

    if(app.report&&!app.running) {
        const float new_w=128*scale,new_h=cap;
        ImGui::SetCursorPos({gear_x-8*scale-new_w,cap_y});
        const bool new_hit=ImGui::InvisibleButton("##New search",{new_w,new_h});
        const ImVec2 mn=ImGui::GetItemRectMin(), mx=ImGui::GetItemRectMax();
        auto* dl=ImGui::GetWindowDrawList();
        const bool hovered=ImGui::IsItemHovered(), active=ImGui::IsItemActive();
        const ImVec4 fill_col=active?ImVec4(0.18f,0.33f,0.60f,1):hovered?ImVec4(0.27f,0.46f,0.78f,1):ImVec4(0.22f,0.39f,0.69f,1);
        dl->AddRectFilled(mn,mx,ImGui::GetColorU32(fill_col),6.f*scale);
        ImFont* font=ImGui::GetFont();
        const float fs=ImGui::GetFontSize();
        const char* ns_label="New search";
        const ImVec2 ts=font->CalcTextSizeA(fs,FLT_MAX,0.f,ns_label);
        dl->AddText(font,fs,{mn.x+(mx.x-mn.x-ts.x)*.5f,mn.y+(mx.y-mn.y-ts.y)*.5f},IM_COL32(255,255,255,255),ns_label);
        if(new_hit) {
            app.report.reset();app.clear_host_favicon();app.target[0]=0;app.error.clear();app.notice.clear();
            app.show_help=false;app.show_settings=false;app.focus_target=true;
            app.inspection_options={};app.reset_endpoint_view();
        }
    }

    auto caption_hit=[&](const char* id,ImVec4 hover)->bool {
        const auto p=ImGui::GetCursorScreenPos();
        const bool pressed=ImGui::InvisibleButton(id,{cap,cap});
        auto* d=ImGui::GetWindowDrawList();
        if(ImGui::IsItemHovered()) {
            const float a=ImGui::IsItemActive()?hover.w:hover.w*.7f;
            d->AddRectFilled(p,{p.x+cap,p.y+cap},ImGui::GetColorU32(ImVec4(hover.x,hover.y,hover.z,a)),6*scale);
        }
        return pressed;
    };
    ImGui::SetCursorPos({gear_x,cap_y});
    const auto gear_at=ImGui::GetCursorScreenPos();
    if(caption_hit("##Settings",{1,1,1,.12f})){app.proxy_error.clear();app.notice.clear();app.edit_proxy();}
    icon(Icon::Settings,{gear_at.x+5*scale,gear_at.y+5*scale},20*scale,ImVec4(1,1,1,1));
    proxy_control(app,"settings");

    for(int i=0;i<3;++i) {
        ImGui::SetCursorPos({width-(108*scale-i*cap_step),cap_y});
        const char* ids[]={"##Minimize","##Maximize or restore","##Close"};
        const ImVec4 hover=i==2?ImVec4(.65f,.21f,.25f,1):ImVec4(1,1,1,.12f);
        const auto p=ImGui::GetCursorScreenPos();
        const bool pressed=caption_hit(ids[i],hover);
        auto* d=ImGui::GetWindowDrawList();const auto c=ImGui::GetColorU32(muted);
        const ImVec2 center{p.x+cap*.5f,p.y+cap*.5f};
        if(i==0)d->AddLine({center.x-4*scale,center.y},{center.x+4*scale,center.y},c,scale);
        if(i==1) {
            if(SDL_GetWindowFlags(window)&SDL_WINDOW_MAXIMIZED)d->AddRect({center.x-1.5f*scale,center.y-5*scale},{center.x+5*scale,center.y+1.5f*scale},c,scale);
            d->AddRect({center.x-4*scale,center.y-3.5f*scale},{center.x+3.5f*scale,center.y+4*scale},c,scale);
        }
        if(i==2){

            const float arm=4.0f*scale,xw=1.55f*scale;
            const float cx=std::floor(center.x)+.5f,cy=std::floor(center.y)+.5f;
            d->AddLine({cx-arm,cy-arm},{cx+arm,cy+arm},c,xw);
            d->AddLine({cx+arm,cy-arm},{cx-arm,cy+arm},c,xw);
        }
        if(pressed) {
            if(i==0)SDL_MinimizeWindow(window);
            else if(i==1){if(SDL_GetWindowFlags(window)&SDL_WINDOW_MAXIMIZED)SDL_RestoreWindow(window);else SDL_MaximizeWindow(window);}
            else quit=true;
        }
    }
}
void render(App& app,SDL_Window* window,bool& quit) {
    const auto size=ImGui::GetIO().DisplaySize;
    update_flowing_motion(window);
    ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("Vector",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    app.proxy_controls=nlohmann::json::array();
    app_header(app,window,quit);
    flowing_outline(size,window);
    ImGui::SetCursorPos({16*scale,64*scale});
    const bool rail=app.report&&!app.running&&!app.show_settings;
    if(rail){sidebar(app,192*scale,size.y-80*scale);ImGui::SameLine(0,12*scale);}
    const bool filled=app.running||app.report||app.show_help||app.show_settings;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,filled?ImVec4(.12f,.13f,.15f,1):ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{(rail?24.0f:32.0f)*scale,24*scale});
    ImGui::BeginChild("Work area",{size.x-ImGui::GetCursorPosX()-16*scale,size.y-80*scale},ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();ImGui::PopStyleColor();
    if(!app.error.empty()&&app.report){wrapped(app.error,red);gap(12);}
    if(!app.notice.empty()&&!app.show_settings){wrapped(app.notice,green);gap(12);}

    if(app.show_settings) {
        proxy_settings(app);
    } else if(app.running) {
        data_text(app.target.data(),muted);gap(10);
        std::vector<ScanEvent> activity;{std::lock_guard lock(app.mutex);activity=app.live_activity;}
        const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-app.inspection_started).count();
        if(draw_activity(activity,elapsed,true,app.cancel,app.activity_view,{scale,bold,ink,muted,blue,green,amber,red}))app.cancel=true;
    } else if(app.show_help) {
        const float help_width=std::min(760*scale,ImGui::GetContentRegionAvail().x);
        const float help_x=ImGui::GetCursorPosX()+std::max(0.0f,(ImGui::GetContentRegionAvail().x-help_width)*.5f);
        ImGui::SetCursorPosX(help_x);
        ImGui::BeginChild("Help panel",{help_width,0},ImGuiChildFlags_None,ImGuiWindowFlags_None);
        if(button("Back to inspection"))app.show_help=false;gap(16);help();
        ImGui::EndChild();
    } else if(!app.report) {
        empty_state(app);
    } else {
        const auto& r=*app.report;
        if(ImGui::BeginTable("Report identity",2,ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Identity",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Actions",ImGuiTableColumnFlags_WidthFixed,32*scale);
            ImGui::TableNextRow();ImGui::TableNextColumn();
            report_host_heading(app,r);
            ImGui::TableNextColumn();
            const float box=28*scale;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.0f,ImGui::GetContentRegionAvail().x-box));
            const ImVec2 export_at=ImGui::GetCursorScreenPos();
            if(button("##Export report",{box,box}))ImGui::OpenPopup("Export format");
            icon(Icon::Export,{export_at.x+6*scale,export_at.y+6*scale},16*scale,ink);
            ImGui::SetNextWindowPos({ImGui::GetItemRectMax().x,ImGui::GetItemRectMax().y+4*scale},ImGuiCond_Always,{1,0});
            ImGui::SetNextWindowSizeConstraints({210*scale,0},{360*scale,FLT_MAX});
            if(ImGui::BeginPopup("Export format")) {
                if(ImGui::MenuItem("JSON"))export_report(app,ReportFormat::Json);
                if(ImGui::MenuItem("CSV"))export_report(app,ReportFormat::Csv);
                if(ImGui::MenuItem("TXT"))export_report(app,ReportFormat::Txt);
                ImGui::Separator();
                if(ImGui::MenuItem("Copy to clipboard"))copy_report(app);
                ImGui::EndPopup();
            }
            ImGui::EndTable();
        }

        if(app.report) {
            if(r.status=="cancelled"){gap(5);wrapped("Collected observations are partial. Remaining checks did not finish.",amber);}
            if(!r.error.empty()){gap(5);wrapped(r.error,red);}
            if(app.tab!=4)gap(2);
            flowing_rule();
            gap(8);
            if(app.tab==4)overview(app,r);
            else {
                const char* titles[]={"Services","Software & identity","Raw evidence","Vendor relationships","Overview","Inspection activity"};
                heading(titles[std::clamp(app.tab,0,5)],20);gap(16);
                if(app.tab==0)services(app,r);
                else if(app.tab==1)findings(app,r);
                else if(app.tab==2)observations(r);
                else if(app.tab==3)vendors(app,r);
                else if(app.tab==5) {
                    if(r.activity.empty())wrapped("This report has no recorded activity. Example data makes no network requests.",muted);
                    else draw_activity(r.activity,r.duration_ms,false,false,app.activity_view,{scale,bold,ink,muted,blue,green,amber,red});
                }
            }
        }
    }
    ImGui::EndChild();ImGui::End();
}

}
int main(int argc,char** argv) {
    App app;std::string screenshot,report_path,capture_stage,ui_state,ui_actions_path;bool demo=false,autorun=false,visible_capture=false;int width=1280,height=800,view=4,cancel_after=0,activity_stage=-1,capture_delay=0;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];auto next=[&](){return i+1<argc?std::string(argv[++i]):std::string{};};
        if(arg=="--demo")demo=true;
        else if(arg=="--target"){auto t=next();std::snprintf(app.target.data(),app.target.size(),"%s",t.c_str());}
        else if(arg=="--autorun")autorun=true;
        else if(arg=="--no-browser")app.inspection_options.browser_runtime=false;
        else if(arg=="--screenshot")screenshot=next();
        else if(arg=="--visible-capture")visible_capture=true;
        else if(arg=="--capture-delay")capture_delay=std::max(0,std::atoi(next().c_str()));
        else if(arg=="--ui-state")ui_state=next();
        else if(arg=="--ui-actions")ui_actions_path=next();
        else if(arg=="--report-json")report_path=next();
        else if(arg=="--capture-stage")capture_stage=next();
        else if(arg=="--activity-open")app.activity_open=true;
        else if(arg=="--activity-stage")activity_stage=std::atoi(next().c_str());
        else if(arg=="--ports"){try{app.inspection_options.ports=parse_ports(next());}catch(const std::exception& e){app.error=e.what();return 2;}}
        else if(arg=="--cancel-after")cancel_after=std::atoi(next().c_str());
        else if(arg=="--help-view")app.show_help=true;
        else if(arg=="--settings-view")app.show_settings=true;
        else if(arg=="--settings-path")app.proxy_path=std::filesystem::path(next());
        else if(arg=="--filter"){view=1;auto f=next();std::snprintf(app.filter.data(),app.filter.size(),"%s",f.c_str());}
        else if(arg=="--vendor-filter"){auto f=next();std::snprintf(app.vendor_filter.data(),app.vendor_filter.size(),"%s",f.c_str());}
        else if(arg=="--known-vendors-only")app.known_vendors_only=true;
        else if(arg=="--width")width=std::atoi(next().c_str());
        else if(arg=="--height")height=std::atoi(next().c_str());
        else if(arg=="--view")view=std::atoi(next().c_str());
    }
    try {app.proxy=load_proxy_settings(app.proxy_path);if(app.proxy.enabled)app.proxy.scheme="auto";if(app.show_settings)app.edit_proxy();}
    catch(const std::exception& e){app.proxy_error=e.what();app.proxy_load_failed=true;app.edit_proxy();}
    std::vector<CaptureAction> ui_actions;
    if(!ui_actions_path.empty()) {
        if(screenshot.empty())return 2;
        try {
            std::ifstream file(ui_actions_path);const auto data=nlohmann::json::parse(file);
            for(const auto& a:data)ui_actions.push_back({a.at("at").get<Uint64>(),a.at("kind").get<std::string>(),a.value("value",std::string{}),a.value("x",0),a.value("y",0),a.value("key",0)});
            std::stable_sort(ui_actions.begin(),ui_actions.end(),[](const auto& a,const auto& b){return a.at<b.at;});
        }catch(const std::exception&){return 2;}
    }
    if(!SDL_Init(SDL_INIT_VIDEO)){SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Vector",SDL_GetError(),nullptr);return 1;}
    scale=screenshot.empty()?std::clamp(SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()),1.0f,2.0f):1;
    SDL_WindowFlags flags=SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_BORDERLESS;
    if(!screenshot.empty()&&!visible_capture)flags|=SDL_WINDOW_HIDDEN;
    auto* window=SDL_CreateWindow("Vector - Host fingerprinting",static_cast<int>(width*scale),static_cast<int>(height*scale),flags);
    auto* renderer=window?SDL_CreateRenderer(window,nullptr):nullptr;
    if(!window||!renderer){SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Vector",SDL_GetError(),nullptr);SDL_Quit();return 1;}
    SDL_SetWindowMinimumSize(window,1000,700);SDL_SetRenderVSync(renderer,1);
    setup_window_chrome(window);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
    io.Fonts->Flags|=ImFontAtlasFlags_NoBakedRoundCorners;
    const std::array<const char*,3> fonts={"C:/Windows/Fonts/segoeui.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/System/Library/Fonts/SFNS.ttf"};
    for(const auto* path:fonts)if(std::filesystem::exists(path)){regular=io.Fonts->AddFontFromFileTTF(path,17*scale);break;}
    if(!regular)regular=io.Fonts->AddFontDefault();bold=regular;mono=regular;
    if(std::filesystem::exists("C:/Windows/Fonts/seguisb.ttf"))bold=io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisb.ttf",17*scale);
    if(std::filesystem::exists("C:/Windows/Fonts/consola.ttf"))mono=io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf",14*scale);
    theme();ImGui_ImplSDL3_InitForSDLRenderer(window,renderer);ImGui_ImplSDLRenderer3_Init(renderer);
    app.renderer=renderer;
    initialize_technology_icons(renderer);
    if(demo)app.demo();app.tab=std::clamp(view,0,5);if(app.activity_open)app.tab=5;if(autorun)app.start();
    if(activity_stage>=0){app.activity_view.selected=std::clamp(activity_stage,0,4);app.activity_view.follow=false;}
    bool quit=false;int frames=0,settled=0,exit_code=0;size_t next_action=0;
    const auto began=SDL_GetTicks();
    std::string quit_reason;
    auto note_quit=[&](const char* why){ if(quit_reason.empty())quit_reason=why; };
    while(!quit){
        while(next_action<ui_actions.size()&&SDL_GetTicks()-began>=ui_actions[next_action].at)capture_action(window,ui_actions[next_action++]);
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            ImGui_ImplSDL3_ProcessEvent(&e);
            if(e.type==SDL_EVENT_QUIT){note_quit("SDL_EVENT_QUIT");quit=true;}
            if(e.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED){note_quit("SDL_EVENT_WINDOW_CLOSE_REQUESTED");quit=true;}
            if(e.type==SDL_EVENT_KEY_DOWN && e.key.key==SDLK_ESCAPE && app.running)app.cancel=true;
            else if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key==SDLK_ESCAPE&&app.show_settings){app.show_settings=false;app.notice.clear();app.focus_target=!app.report;}
            if(e.type==SDL_EVENT_WINDOW_RESIZED||e.type==SDL_EVENT_WINDOW_MAXIMIZED||e.type==SDL_EVENT_WINDOW_RESTORED)update_window_corners(window);
            if(e.type==SDL_EVENT_KEY_DOWN && e.key.key==SDLK_L && (e.key.mod&SDL_KMOD_CTRL) && !app.running) {
                app.report.reset();app.clear_host_favicon();app.target[0]=0;app.error.clear();app.notice.clear();
                app.show_help=false;app.show_settings=false;app.focus_target=true;
                app.inspection_options={};app.reset_endpoint_view();
            }
        }
        app.collect();
        if(cancel_after>0&&app.running&&SDL_GetTicks()-began>=static_cast<Uint64>(cancel_after))app.cancel=true;
        ImGui_ImplSDLRenderer3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
        const bool quit_before_render=quit;
        render(app,window,quit);ImGui::Render();
        if(quit&&!quit_before_render)note_quit("header Close button");
        SDL_SetRenderDrawColor(renderer,27,29,33,255);SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),renderer);
        if(!app.running)++settled;else settled=0;
        bool capture_now=capture_stage.empty()&&settled>8;
        if(!capture_stage.empty()&&app.running) {
            InspectionActivity activity;
            {std::lock_guard lock(app.mutex);for(const auto& event:app.live_activity)activity.accept(event);}
            const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-app.inspection_started).count();
            for(size_t i=0;i<activity.stages.size();++i)if(capture_stage==stage_key(static_cast<ScanStage>(i)))
                capture_now=activity.stages[i].state==ActivityState::Running&&elapsed-activity.stages[i].began_ms>=160;
        }
        if(!screenshot.empty()&&capture_now&&SDL_GetTicks()-began>=static_cast<Uint64>(capture_delay)){
            auto* surface=SDL_RenderReadPixels(renderer,nullptr);
            if(!surface||!SDL_SaveBMP(surface,screenshot.c_str()))exit_code=2;
            if(surface)SDL_DestroySurface(surface);
            note_quit("screenshot capture finished");
            quit=true;
            if(!ui_state.empty()) {
                std::ofstream state(ui_state);
                auto infrastructure=nlohmann::json::array();
                if(app.report)for(const auto& entry:infrastructure_entries(*app.report))
                    infrastructure.push_back({{"name",entry.name},{"detail",entry.detail},{"finding",entry.finding},{"vendor",entry.vendor}});
                nlohmann::json endpoint_detail=nullptr;
                if(app.endpoint_detail) {
                    auto evidence=nlohmann::json::array();
                    for(const auto& e:app.endpoint_detail->evidence)evidence.push_back({{"source",e.source},{"value",e.value}});
                    endpoint_detail={{"key",app.endpoint_detail->key},{"title",app.endpoint_detail->title},{"detail",app.endpoint_detail->detail},{"evidence",evidence}};
                }
                nlohmann::json api_detail=nullptr;
                if(app.api_detail) {
                    auto evidence=nlohmann::json::array();
                    for(const auto& e:app.api_detail->evidence)evidence.push_back({{"source",e.source},{"value",e.value}});
                    api_detail={{"key",app.api_detail->key},{"title",app.api_detail->title},{"detail",app.api_detail->detail},{"evidence",evidence}};
                }
                state << nlohmann::json({{"show_all",app.report.has_value()||app.running},{"tab",app.tab},
                    {"api_rows",app.api_row_bounds},{"api_controls",app.api_control_bounds},{"api_detail",api_detail},
                    {"api_endpoint_limit",app.api_endpoint_limit},{"api_endpoint_count",app.report?app.api_endpoints.size():0},
                    {"api_section_shown",app.report.has_value()&&app.tab==4&&app.api_section_shown},
                    {"navigation_visible",app.report.has_value()&&!app.running&&!app.show_settings},
                    {"settings_open",app.show_settings},{"proxy_controls",app.proxy_controls},
                    {"proxy_enabled",app.proxy.enabled},{"proxy_endpoint",app.proxy.enabled?proxy_endpoint(app.proxy):std::string{}},
                    {"proxy_has_credentials",!app.proxy.username.empty()},{"proxy_error",app.proxy_error},
                    {"infrastructure",infrastructure},
                    {"endpoint_rows",app.endpoint_row_bounds},{"endpoint_controls",app.endpoint_control_bounds},{"endpoint_detail",endpoint_detail},
                    {"endpoint_host_limit",app.endpoint_host_limit},{"endpoint_dns_limit",app.endpoint_dns_limit},{"infrastructure_limit",app.infrastructure_limit},
                    {"target",app.target.data()},{"history_count",0},{"running",static_cast<bool>(app.running)},
                    {"status",app.report?app.report->status:"empty"},{"error",app.error},{"notice",app.notice},
                    {"selected_finding",app.selected},{"selected_vendor",app.selected_vendor},
                    {"mouse",{io.MousePos.x,io.MousePos.y}},{"display",{io.DisplaySize.x,io.DisplaySize.y}},
                    {"window_flags",SDL_GetWindowFlags(window)}}).dump(2);
                if(!state)exit_code=4;
            }
            if(!report_path.empty()){
                try {
                    if(app.report)save_report(*app.report,report_path);
                    else if(app.running) {
                        ScanReport snapshot;snapshot.target=parse_target(app.target.data());snapshot.status="running";
                        {std::lock_guard lock(app.mutex);snapshot.activity=app.live_activity;}
                        save_report(snapshot,report_path);
                    }
                }catch(const std::exception&){exit_code=4;}
            }
        }
        SDL_RenderPresent(renderer);++frames;
        if(!screenshot.empty()&&SDL_GetTicks()-began>90000){app.cancel=true;exit_code=3;note_quit("screenshot timeout");quit=true;}
        SDL_Delay(!screenshot.empty()?5:10);
    }
    try {
        std::ofstream log("vector-quit.log",std::ios::app);
        if(log) log << "exit=" << exit_code
                    << " reason=" << (quit_reason.empty()?"unknown":quit_reason)
                    << " frames=" << frames
                    << " ticks=" << (SDL_GetTicks()-began) << "\n";
    } catch(...) {}
    app.cancel=true;if(app.worker.joinable())app.worker.join();
    shutdown_technology_icons();
    ImGui_ImplSDLRenderer3_Shutdown();ImGui_ImplSDL3_Shutdown();ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return exit_code;
}
