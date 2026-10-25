#include "cli.hpp"
#include "output.hpp"
#include "port_parser.hpp"
#include "scanner.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    const portscope::ParseResult parse_result = portscope::ParseCommandLine(argc, argv);
    if (!parse_result.ok) {
        std::cerr << "Error: " << parse_result.error << "\n\n";
        portscope::PrintHelp(std::cerr);
        return 1;
    }

    if (parse_result.options.command == portscope::Command::Help) {
        portscope::PrintHelp(std::cout);
        return 0;
    }

    if (parse_result.options.command == portscope::Command::Version) {
        portscope::PrintVersion(std::cout);
        return 0;
    }

#ifndef __linux__
    std::cerr << "Error: PortScope currently supports Linux only.\n";
    return 1;
#endif

    std::vector<std::uint16_t> ports;
    std::string port_error;
    if (!portscope::ParsePortSpec(parse_result.options.ports_spec, ports, port_error)) {
        std::cerr << "Error: " << port_error << "\n";
        return 1;
    }

    portscope::ScanConfig config;
    config.host = parse_result.options.host;
    config.ports = ports;
    config.timeout_ms = parse_result.options.timeout_ms;
    config.concurrency = parse_result.options.concurrency;

    const auto scan_start = std::chrono::steady_clock::now();

    portscope::ScanOutput scan_output;
    std::string scan_error;
    if (!portscope::ScanPorts(config, scan_output, scan_error)) {
        std::cerr << "Error: " << scan_error << "\n";
        return 2;
    }

    const auto scan_end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(scan_end - scan_start);

    portscope::PrintScanResults(std::cout, config, scan_output, elapsed);
    return 0;
}
