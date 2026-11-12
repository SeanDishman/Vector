#include "technology_icons.hpp"
#include "technology_icon_data.hpp"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

namespace vectorapp {
namespace {
std::string folded(std::string_view value) {
    const auto first=value.find_first_not_of(" \t\r\n");
    if(first==std::string_view::npos)return {};
    const auto last=value.find_last_not_of(" \t\r\n");
    std::string key(value.substr(first,last-first+1));
    for(auto& c:key)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return key;
}
struct Icon {
    const icon_data::Source* source=nullptr;
    SDL_Texture* texture=nullptr;
    bool attempted=false;
};
struct Catalog {
    SDL_Renderer* renderer=nullptr;
    std::vector<Icon> icons;
    std::unordered_map<std::string,size_t> aliases;
    Catalog() {
        std::unordered_map<std::string,size_t> keys;
        for(const auto& source:icon_data::sources) {
            keys.emplace(source.key,icons.size());icons.push_back({&source});
        }
        const auto data=nlohmann::json::parse(icon_data::manifest);
        for(const auto& entry:data.at("icons")) {
            const auto found=keys.find(entry.at("key").get<std::string>());
            if(found==keys.end())continue;
            for(const auto& name:entry.at("names"))aliases.emplace(folded(name.get<std::string>()),found->second);
        }
    }
    Icon* find(std::string_view name) {
        const auto found=aliases.find(folded(name));
        return found==aliases.end()?nullptr:&icons[found->second];
    }
    SDL_Texture* texture(Icon& icon) {
        if(icon.attempted||!renderer)return icon.texture;
        icon.attempted=true;
        std::string svg=icon.source->svg;
        const std::unique_ptr<NSVGimage,decltype(&nsvgDelete)> image(nsvgParse(svg.data(),"px",96),nsvgDelete);
        if(!image||image->width<=0||image->height<=0||!std::isfinite(image->width)||!std::isfinite(image->height))return nullptr;
        const std::unique_ptr<NSVGrasterizer,decltype(&nsvgDeleteRasterizer)> raster(nsvgCreateRasterizer(),nsvgDeleteRasterizer);
        if(!raster)return nullptr;
        constexpr int resolution=96;
        std::vector<unsigned char> pixels(resolution*resolution*4,0);
        const float ratio=(resolution-4)/std::max(image->width,image->height);
        nsvgRasterize(raster.get(),image.get(),(resolution-image->width*ratio)*.5f,
            (resolution-image->height*ratio)*.5f,ratio,pixels.data(),resolution,resolution,resolution*4);
        bool painted=false;
        for(size_t i=3;i<pixels.size();i+=4)if(pixels[i]){painted=true;break;}
        if(!painted)return nullptr;
        auto* surface=SDL_CreateSurfaceFrom(resolution,resolution,SDL_PIXELFORMAT_RGBA32,pixels.data(),resolution*4);
        if(!surface)return nullptr;
        icon.texture=SDL_CreateTextureFromSurface(renderer,surface);SDL_DestroySurface(surface);
        if(icon.texture) {
            SDL_SetTextureBlendMode(icon.texture,SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(icon.texture,SDL_SCALEMODE_LINEAR);
        }
        return icon.texture;
    }
};
Catalog& catalog(){static Catalog value;return value;}

void fallback(std::string_view category,ImVec2 p,float size) {
    auto* draw=ImGui::GetWindowDrawList();
    const auto color=IM_COL32(175,185,200,255);
    const float stroke=std::max(1.0f,size*.045f);
    const auto point=[&](float x,float y){return ImVec2(p.x+x*size,p.y+y*size);};
    const auto line=[&](float a,float b,float c,float d){draw->AddLine(point(a,b),point(c,d),color,stroke);};
    const auto kind=folded(category);
    if(kind.find("server")!=std::string::npos||kind.find("service")!=std::string::npos||kind.find("protocol")!=std::string::npos) {
        draw->AddRect(point(.23f,.24f),point(.77f,.45f),color,stroke,0,stroke);
        draw->AddRect(point(.23f,.55f),point(.77f,.76f),color,stroke,0,stroke);
        draw->AddCircleFilled(point(.33f,.345f),stroke,color);draw->AddCircleFilled(point(.33f,.655f),stroke,color);
    } else if(kind.find("provider")!=std::string::npos||kind.find("integration")!=std::string::npos||kind.find("relationship")!=std::string::npos) {
        draw->AddCircle(point(.31f,.5f),size*.11f,color,16,stroke);
        draw->AddCircle(point(.7f,.3f),size*.09f,color,16,stroke);
        draw->AddCircle(point(.7f,.7f),size*.09f,color,16,stroke);
        line(.41f,.45f,.61f,.34f);line(.41f,.55f,.61f,.66f);
    } else {
        line(.25f,.35f,.5f,.2f);line(.5f,.2f,.75f,.35f);line(.75f,.35f,.5f,.5f);line(.5f,.5f,.25f,.35f);
        line(.25f,.35f,.25f,.65f);line(.25f,.65f,.5f,.8f);line(.5f,.8f,.75f,.65f);line(.75f,.65f,.75f,.35f);line(.5f,.5f,.5f,.8f);
    }
}
}

void initialize_technology_icons(SDL_Renderer* renderer) {
    shutdown_technology_icons();catalog().renderer=renderer;
}
void shutdown_technology_icons() {
    auto& value=catalog();
    for(auto& icon:value.icons){if(icon.texture)SDL_DestroyTexture(icon.texture);icon.texture=nullptr;icon.attempted=false;}
    value.renderer=nullptr;
}
std::string_view technology_icon_key(std::string_view name) {
    const auto* icon=catalog().find(name);return icon?icon->source->key:std::string_view{};
}
void technology_icon(std::string_view name,std::string_view category,ImVec2 p,float size) {
    auto& value=catalog();auto* icon=value.find(name);
    auto* texture=icon?value.texture(*icon):nullptr;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,{p.x+size,p.y+size},texture?IM_COL32(244,245,247,255):IM_COL32(43,47,54,255),size*.19f);
    if(texture) {
        const float inset=size*.17f;
        draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(texture)),
            {p.x+inset,p.y+inset},{p.x+size-inset,p.y+size-inset});
    } else fallback(category,p,size);
}
}
