#ifndef PORTSCOPE_SCANNER_HPP
#define PORTSCOPE_SCANNER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace portscope {

enum class PortState {
    Open,
    Closed,
    Filtered,
};

struct PortScanResult {
    std::uint16_t port = 0;
    PortState state = PortState::Filtered;
    int error_code = 0;
};

struct ScanConfig {
    std::string host;
    std::vector<std::uint16_t> ports;
    int timeout_ms = 1000;
    int concurrency = 100;
};

struct ScanSummary {
    std::size_t open_count = 0;
    std::size_t closed_count = 0;
    std::size_t filtered_count = 0;
};

struct ScanOutput {
    std::string resolved_address;
    std::vector<PortScanResult> results;
    ScanSummary summary;
};

PortState ClassifySocketError(int socket_error);
bool ScanPorts(const ScanConfig& config, ScanOutput& output, std::string& error);

}  // namespace portscope

#endif
