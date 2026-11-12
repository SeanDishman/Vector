#pragma once
#include <imgui.h>
#include <string_view>

struct SDL_Renderer;

namespace vectorapp {

void initialize_technology_icons(SDL_Renderer* renderer);
void shutdown_technology_icons();
std::string_view technology_icon_key(std::string_view name);
void technology_icon(std::string_view name, std::string_view category, ImVec2 position, float size);
}
