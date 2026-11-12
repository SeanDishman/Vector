#pragma once
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace vectorapp {
enum class ScanStage { Resolve, Services, Web, Discovery, Report };
enum class ActivityState { Waiting, Running, Complete, Skipped, Cancelled, Failed };

struct ScanEvent {
    ScanStage stage = ScanStage::Resolve;
    ActivityState state = ActivityState::Waiting;
    double elapsed_ms = 0;
    std::string item, detail;
    size_t completed = 0, total = 0;
};
using ScanObserver = std::function<void(const ScanEvent&)>;

const char* stage_name(ScanStage stage);
const char* stage_key(ScanStage stage);
const char* activity_state_name(ActivityState state);

struct StageActivity {
    ActivityState state = ActivityState::Waiting;
    double began_ms = 0, ended_ms = 0;
    std::string detail;
    size_t completed = 0, total = 0;
};
struct WorkActivity {
    ScanStage stage;
    ActivityState state;
    double began_ms = 0, ended_ms = 0;
    std::string item, detail;
};
struct InspectionActivity {
    std::array<StageActivity,5> stages{};
    std::vector<WorkActivity> work;
    void accept(const ScanEvent& event);
};
}
