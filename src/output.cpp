#include "output.hpp"

#include <iomanip>
#include <sstream>

namespace portscope {
namespace {

std::string PluralizePortCount(const std::size_t count) {
    return count == 1 ? "port" : "ports";
}

std::string FormatElapsed(const std::chrono::milliseconds elapsed) {
    if (elapsed.count() <= 0) {
        return "<1 ms";
    }

    if (elapsed.count() < 1000) {
        return std::to_string(elapsed.count()) + " ms";
    }

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << (static_cast<double>(elapsed.count()) / 1000.0) << " s";
    return stream.str();
}

}  // namespace

void PrintHelp(std::ostream& output) {
    output << "PortScope v" << kVersion << "\n"
           << "Usage: portscope --host HOST --ports SPEC [OPTIONS]\n\n"
           << "Options:\n"
           << "  --host HOST         IPv4 address or hostname to scan (required)\n"
           << "  --ports SPEC        Ports to scan, e.g. 22,80-82,443 (required)\n"
           << "  --timeout MS        Connect timeout per port in milliseconds (default: 1000)\n"
           << "  --concurrency N     Maximum concurrent connection attempts (default: 100)\n"
           << "  --help              Show this help message\n"
           << "  --version           Show version information\n";
}

void PrintVersion(std::ostream& output) {
    output << "PortScope v" << kVersion << "\n";
}

std::string PortStateToString(const PortState state) {
    switch (state) {
    case PortState::Open:
        return "open";
    case PortState::Closed:
        return "closed";
    case PortState::Filtered:
        return "filtered";
    }

    return "filtered";
}

void PrintScanResults(
    std::ostream& output,
    const ScanConfig& config,
    const ScanOutput& scan_output,
    const std::chrono::milliseconds elapsed) {

    output << "Target: " << config.host << " (" << scan_output.resolved_address << ")\n";
    output << "Timeout: " << config.timeout_ms << " ms, Concurrency: " << config.concurrency << "\n\n";

    output << "PORT      STATE\n";
    output << "----------------\n";

    for (const PortScanResult& result : scan_output.results) {
        output << std::left << std::setw(10) << (std::to_string(result.port) + "/tcp")
               << PortStateToString(result.state) << "\n";
    }

    output << "\nSummary:\n";
    output << "  Scanned:   " << scan_output.results.size() << " "
           << PluralizePortCount(scan_output.results.size()) << "\n";
    output << "  Open:      " << scan_output.summary.open_count << "\n";
    output << "  Closed:    " << scan_output.summary.closed_count << "\n";
    output << "  Filtered:  " << scan_output.summary.filtered_count << "\n";
    output << "  Elapsed:   " << FormatElapsed(elapsed) << "\n";
}

}  // namespace portscope
