#pragma once
#include "vector/progress.hpp"
#include <imgui.h>

namespace vectorapp {
struct ActivityTheme {
    float scale;
    ImFont* heading;
    ImVec4 ink, muted, blue, green, amber, red;
};
struct ActivityView {
    int selected = -1;
    bool follow = true;
};

bool draw_activity(const std::vector<ScanEvent>& events, double elapsed_ms, bool running,
                   bool cancelling, ActivityView& view, const ActivityTheme& theme, bool detailed = true);
}
