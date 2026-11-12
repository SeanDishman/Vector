#pragma once
#include "vector/model.hpp"

namespace vectorapp {
enum class ReportFormat { Json, Csv, Txt };
const char* report_extension(ReportFormat format);
std::string report_export(const ScanReport& report, ReportFormat format);
void save_report(const ScanReport& report, const std::string& path, ReportFormat format);
}
