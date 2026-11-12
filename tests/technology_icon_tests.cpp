#include "technology_icons.hpp"
#include <SDL3/SDL.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace vectorapp;
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Expected manifest and gallery bitmap paths");
        std::ifstream input(argv[1]);const auto manifest=nlohmann::json::parse(input);
        const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
        check(technology_icon_key("YouTube")=="youtube","YouTube logo lookup");
        check(technology_icon_key(" nginx ")=="nginx","Case/outer-space normalization");
        check(technology_icon_key("NGINX")=="nginx","Case normalization");
        check(technology_icon_key("Google Frontend")=="google","Explicit product alias");
        check(technology_icon_key("youtube.example").empty(),"Unknown domains must not claim a brand");
        check(technology_icon_key("Not React").empty(),"Substring must not claim a brand");
        check(technology_icon_key("").empty(),"Missing name uses neutral fallback");
        check(technology_icon_key("Google-fake-LLC").empty(),"Unlisted company names must not be attributed");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL init");
        auto* window=SDL_CreateWindow("Vector icon checks",1248,840,SDL_WINDOW_HIDDEN);
        auto* renderer=window?SDL_CreateRenderer(window,"software"):nullptr;
        check(renderer,"Hidden software renderer");
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
        ImGui_ImplSDL3_InitForSDLRenderer(window,renderer);ImGui_ImplSDLRenderer3_Init(renderer);
        initialize_technology_icons(renderer);
        size_t aliases=0;
        for(const auto& entry:manifest.at("icons"))for(const auto& name:entry.at("names")) {
            check(technology_icon_key(name.get<std::string>())==entry.at("key").get<std::string>(),"Conflicting or missing alias");++aliases;
        }
        const auto& icons=manifest.at("icons");
        constexpr size_t per_page=132;
        const size_t pages=(icons.size()+per_page-1)/per_page;
        const auto gallery_path=[&](size_t page) {
            const std::string path=argv[2];
            if(page==0)return path;
            const auto dot=path.rfind('.');
            const auto stem=dot==std::string::npos?path:path.substr(0,dot);
            return stem+"-"+std::to_string(page+1)+(dot==std::string::npos?std::string{}:path.substr(dot));
        };
        for(int pass=0;pass<2;++pass) {
            for(size_t page=0;page<pages;++page) {
                const size_t first=page*per_page,last=std::min(icons.size(),first+per_page);
                ImGui_ImplSDLRenderer3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
                ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1248,840});
                ImGui::Begin("Icons",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
                for(size_t i=first;i<last;++i) {
                    const size_t slot=i-first;
                    const ImVec2 p{12+static_cast<float>(slot%12)*104,12+static_cast<float>(slot/12)*72};
                    const auto& entry=icons.at(i);
                    technology_icon(entry.at("names").at(0).get<std::string>(),"Library",p,36);
                    ImGui::GetWindowDrawList()->AddText({p.x,p.y+40},IM_COL32(230,232,235,255),entry.at("key").get<std::string>().c_str());
                }
                technology_icon("unlisted.example","Provider",{12,804},24);
                technology_icon("HTTP","Service",{48,804},24);
                ImGui::End();ImGui::Render();
                SDL_SetRenderDrawColor(renderer,27,29,33,255);SDL_RenderClear(renderer);
                ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),renderer);
                auto* surface=SDL_RenderReadPixels(renderer,nullptr);check(surface,"Rendered surface");
                for(size_t i=first;i<last;++i) {
                    const size_t slot=i-first;
                    Uint8 r=0,g=0,b=0,a=0;
                    check(SDL_ReadSurfacePixel(surface,13+static_cast<int>(slot%12)*104,30+static_cast<int>(slot/12)*72,&r,&g,&b,&a),"Read icon pixel");
                    if(r!=244||g!=245||b!=247)throw std::runtime_error("Unrenderable icon: "+icons.at(i).at("key").get<std::string>());
                }
                check(SDL_SaveBMP(surface,gallery_path(page).c_str()),"Save gallery");SDL_DestroySurface(surface);
            }
            shutdown_technology_icons();initialize_technology_icons(renderer);
        }
        shutdown_technology_icons();ImGui_ImplSDLRenderer3_Shutdown();ImGui_ImplSDL3_Shutdown();ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"PASS: "<<manifest.at("icons").size()<<" icons rasterized twice; "<<aliases<<" aliases; unknown-name boundaries; renderer reset.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
