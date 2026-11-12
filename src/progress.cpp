#include "vector/progress.hpp"
#include <algorithm>

namespace vectorapp {
const char* stage_name(ScanStage stage) {
    switch (stage) {
        case ScanStage::Resolve: return "Resolve address";
        case ScanStage::Services: return "TCP services";
        case ScanStage::Web: return "Web evidence";
        case ScanStage::Discovery: return "Device discovery";
        case ScanStage::Report: return "Assemble report";
    }
    return "Unknown stage";
}
const char* stage_key(ScanStage stage) {
    switch (stage) {
        case ScanStage::Resolve: return "resolve";
        case ScanStage::Services: return "services";
        case ScanStage::Web: return "web";
        case ScanStage::Discovery: return "discovery";
        case ScanStage::Report: return "report";
    }
    return "unknown";
}
const char* activity_state_name(ActivityState state) {
    switch (state) {
        case ActivityState::Waiting: return "Waiting";
        case ActivityState::Running: return "Running";
        case ActivityState::Complete: return "Complete";
        case ActivityState::Skipped: return "Skipped";
        case ActivityState::Cancelled: return "Cancelled";
        case ActivityState::Failed: return "Failed";
    }
    return "Unknown";
}
void InspectionActivity::accept(const ScanEvent& e) {
    if (e.item.empty()) {
        auto& s = stages.at(static_cast<size_t>(e.stage));
        if (s.state == ActivityState::Waiting) s.began_ms = e.elapsed_ms;
        s.state = e.state; s.detail = e.detail;
        s.completed = e.completed; s.total = e.total;
        s.ended_ms = e.elapsed_ms;
        return;
    }
    auto found = std::find_if(work.begin(),work.end(),[&](const WorkActivity& w) { return w.stage == e.stage && w.item == e.item; });
    if (found == work.end()) work.push_back({e.stage,e.state,e.elapsed_ms,e.elapsed_ms,e.item,e.detail});
    else { found->state = e.state; found->ended_ms = e.elapsed_ms; found->detail = e.detail; }
}
}
