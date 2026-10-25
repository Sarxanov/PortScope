#ifndef PORTSCOPE_OUTPUT_HPP
#define PORTSCOPE_OUTPUT_HPP

#include "scanner.hpp"

#include <chrono>
#include <ostream>
#include <string>

namespace portscope {

constexpr const char* kVersion = "0.3.0";

void PrintHelp(std::ostream& output);
void PrintVersion(std::ostream& output);
std::string PortStateToString(PortState state);
void PrintScanResults(
    std::ostream& output,
    const ScanConfig& config,
    const ScanOutput& scan_output,
    std::chrono::milliseconds elapsed);

}  // namespace portscope

#endif
