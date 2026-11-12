#include "progress_view.hpp"
#include <algorithm>
#include <cstdio>

namespace vectorapp {
namespace {
std::string duration(double ms) {
    char result[40];
    if (ms < 1000) std::snprintf(result,sizeof(result),"%.0f ms",std::max(0.0,ms));
    else if (ms < 60000) std::snprintf(result,sizeof(result),"%.1f s",ms/1000);
    else std::snprintf(result,sizeof(result),"%d m %02d s",static_cast<int>(ms/60000),static_cast<int>(ms/1000)%60);
    return result;
}
ImVec4 color(ActivityState state, const ActivityTheme& t) {
    switch (state) {
        case ActivityState::Running: return t.blue;
        case ActivityState::Complete: return t.green;
        case ActivityState::Cancelled: return t.amber;
        case ActivityState::Failed: return t.red;
        default: return t.muted;
    }
}
void wrapped(const std::string& value, ImVec4 ink) {
    ImGui::PushStyleColor(ImGuiCol_Text,ink);
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
}
void marker(ImDrawList* draw, ImVec2 center, ActivityState state, const ActivityTheme& t) {
    const float r = 7*t.scale;
    const ImU32 c = ImGui::GetColorU32(color(state,t));
    if (state == ActivityState::Complete || state == ActivityState::Running) {
        draw->AddCircleFilled(center,r,c);
        if (state == ActivityState::Complete) {
            draw->AddLine({center.x-3*t.scale,center.y},{center.x-t.scale,center.y+2*t.scale},ImGui::GetColorU32(ImGuiCol_WindowBg),1.5f*t.scale);
            draw->AddLine({center.x-t.scale,center.y+2*t.scale},{center.x+3*t.scale,center.y-2*t.scale},ImGui::GetColorU32(ImGuiCol_WindowBg),1.5f*t.scale);
        }
    } else {
        draw->AddCircle(center,r,c,0,1.3f*t.scale);
        if (state == ActivityState::Skipped || state == ActivityState::Cancelled)
            draw->AddLine({center.x-3*t.scale,center.y},{center.x+3*t.scale,center.y},c,1.3f*t.scale);
        if (state == ActivityState::Failed) {
            draw->AddLine({center.x-2*t.scale,center.y-2*t.scale},{center.x+2*t.scale,center.y+2*t.scale},c,1.3f*t.scale);
            draw->AddLine({center.x-2*t.scale,center.y+2*t.scale},{center.x+2*t.scale,center.y-2*t.scale},c,1.3f*t.scale);
        }
    }
}
}
bool draw_activity(const std::vector<ScanEvent>& events, double elapsed_ms, bool running,
                   bool cancelling, ActivityView& view, const ActivityTheme& t, bool detailed) {
    InspectionActivity activity;
    for (const auto& e : events) activity.accept(e);
    int current = 0;
    for (int i = 0; i < 5; ++i) {
        if (activity.stages[i].state != ActivityState::Waiting) current = i;
        if (activity.stages[i].state == ActivityState::Running) break;
    }
    if (view.follow || view.selected < 0) view.selected = current;
    view.selected = std::clamp(view.selected,0,4);
    ImGui::PushID("Inspection activity");
    if (running) {
        ImGui::PushFont(t.heading,23*t.scale);
        ImGui::TextUnformatted(cancelling ? "Stopping inspection" : "Inspection in progress");
        ImGui::PopFont();
    }
    ImGui::TextColored(t.muted,"%s elapsed  /  steps reflect actual work",duration(elapsed_ms).c_str());
    ImGui::Dummy({0,12*t.scale});

    const char* names[] = {"Resolve","Services","Web evidence","Discovery","Report"};
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float cell = width/5;
    const float height = 83*t.scale;
    auto* draw = ImGui::GetWindowDrawList();
    for (int i = 0; i < 4; ++i) {
        const float left = origin.x + cell*i + 22*t.scale;
        const float right = origin.x + cell*(i+1) + 6*t.scale;
        draw->AddLine({left,origin.y+12*t.scale},{right,origin.y+12*t.scale},ImGui::GetColorU32(ImGuiCol_Border),t.scale);
    }
    for (int i = 0; i < 5; ++i) {
        const auto& s = activity.stages[i];
        const ImVec2 p{origin.x+cell*i,origin.y};
        ImGui::SetCursorScreenPos(p); ImGui::PushID(i);
        if (ImGui::Selectable("##stage",false,ImGuiSelectableFlags_None,{cell-4*t.scale,height})) { view.selected=i;view.follow=false; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s",stage_name(static_cast<ScanStage>(i)),s.detail.empty() ? "Waiting for earlier steps" : s.detail.c_str());
        marker(draw,{p.x+14*t.scale,p.y+12*t.scale},s.state,t);
        draw->AddText(t.heading,17*t.scale,{p.x+6*t.scale,p.y+30*t.scale},ImGui::GetColorU32(t.ink),names[i]);
        draw->AddText({p.x+6*t.scale,p.y+53*t.scale},ImGui::GetColorU32(color(s.state,t)),activity_state_name(s.state));
        if (view.selected == i) draw->AddLine({p.x+6*t.scale,p.y+height},{p.x+cell-14*t.scale,p.y+height},ImGui::GetColorU32(t.blue),2*t.scale);
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos({origin.x,origin.y+height+18*t.scale});
    bool cancel = false;
    if (running) {
        ImGui::BeginDisabled(cancelling);
        cancel = ImGui::Button(cancelling ? "Cancelling..." : "Cancel inspection",{174*t.scale,0});
        ImGui::EndDisabled();
        if (!view.follow) {
            ImGui::SameLine();
            if (ImGui::Button("Follow current step")) view.follow=true;
        }
        ImGui::Dummy({0,8*t.scale});
    }
    const auto& selected = activity.stages[view.selected];
    ImGui::PushFont(t.heading,18*t.scale);
    ImGui::TextUnformatted(stage_name(static_cast<ScanStage>(view.selected)));
    ImGui::PopFont();
    ImGui::SameLine();
    if (selected.total) ImGui::TextColored(t.muted,view.selected==static_cast<int>(ScanStage::Web)?"%zu / %zu page requests (cap)":"%zu / %zu checked",selected.completed,selected.total);
    else ImGui::TextColored(color(selected.state,t),"%s",activity_state_name(selected.state));
    std::string detail = selected.detail.empty() ? "This step has not started." : selected.detail;
    if (cancelling) detail = "Stopping active checks. Responses already collected will remain in the report.";
    wrapped(detail,t.muted);
    ImGui::Dummy({0,5*t.scale});

    if (!detailed) {
        ImGui::Dummy({0,12*t.scale});
        ImGui::PushFont(t.heading,18*t.scale);
        ImGui::TextUnformatted("Key observations");
        ImGui::PopFont();
        ImGui::BeginChild("Live observations",{0,0});
        int shown=0;
        for (auto it=activity.work.rbegin();it!=activity.work.rend()&&shown<6;++it) {
            const bool useful=it->state==ActivityState::Running||it->state==ActivityState::Failed||
                it->detail.find("open")!=std::string::npos||it->stage==ScanStage::Resolve||it->stage==ScanStage::Web||it->stage==ScanStage::Discovery;
            if(!useful)continue;
            ImGui::TextColored(color(it->state,t),"%s",activity_state_name(it->state));
            ImGui::SameLine();wrapped(it->item,t.ink);
            wrapped(it->detail,t.muted);
            ImGui::Dummy({0,10*t.scale});++shown;
        }
        if(!shown)wrapped("Waiting for the first observations. The stages above follow the real inspection.",t.muted);
        ImGui::EndChild();
        ImGui::PopID();
        return cancel;
    }

    std::vector<const WorkActivity*> rows;
    for (const auto& item : activity.work)
        if (view.selected == 4 || static_cast<int>(item.stage) == view.selected) rows.push_back(&item);
    std::stable_sort(rows.begin(),rows.end(),[](const auto* a,const auto* b) {
        if ((a->state == ActivityState::Running) != (b->state == ActivityState::Running)) return a->state == ActivityState::Running;
        return a->began_ms < b->began_ms;
    });
    if (!rows.empty()) {
        float available = ImGui::GetContentRegionAvail().y;
        const float table_height = running ? std::max(105*t.scale,available-12*t.scale) : 180*t.scale;
        if (ImGui::BeginTable("Work observed",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,{0,table_height})) {
            ImGui::TableSetupColumn("Operation",ImGuiTableColumnFlags_WidthStretch,0.9f);
            ImGui::TableSetupColumn("State",ImGuiTableColumnFlags_WidthFixed,82*t.scale);
            ImGui::TableSetupColumn("Elapsed",ImGuiTableColumnFlags_WidthFixed,66*t.scale);
            ImGui::TableSetupColumn("Latest observation",ImGuiTableColumnFlags_WidthStretch,1.4f);
            ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
            for (const auto* item : rows) {
                ImGui::TableNextRow();ImGui::TableNextColumn();wrapped(item->item,t.ink);
                ImGui::TableNextColumn();ImGui::TextColored(color(item->state,t),"%s",activity_state_name(item->state));
                ImGui::TableNextColumn();
                const double end = item->state == ActivityState::Running && running ? elapsed_ms : item->ended_ms;
                ImGui::TextColored(t.muted,"%s",duration(end-item->began_ms).c_str());
                ImGui::TableNextColumn();wrapped(item->detail,t.muted);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Dummy({0,10*t.scale});
        switch (static_cast<ScanStage>(view.selected)) {
            case ScanStage::Resolve: wrapped("The system resolver chooses an address. HTTP Host and TLS SNI keep the hostname you entered.",t.muted); break;
            case ScanStage::Services: wrapped("Each port appears here when its connection check starts.",t.muted); break;
            case ScanStage::Web: wrapped("Static page reads and browser-loaded scripts appear here as they start. Browser execution can load dependency hosts.",t.muted); break;
            case ScanStage::Discovery: wrapped("SSDP, mDNS and SNMP replies describe this endpoint. A missing reply does not prove a service is absent.",t.muted); break;
            case ScanStage::Report: wrapped("No individual connections were started during this inspection.",t.muted); break;
        }
    }
    ImGui::PopID();
    return cancel;
}
}
