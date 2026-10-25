#include "cli.hpp"
#include "output.hpp"
#include "port_parser.hpp"
#include "scanner.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class ScopedFd {
public:
    explicit ScopedFd(int fd = -1) : fd_(fd) {}

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    ~ScopedFd() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    int Get() const {
        return fd_;
    }

private:
    int fd_;
};

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

portscope::ParseResult ParseCliForTest(const std::vector<std::string>& args) {
    std::vector<std::string> values;
    values.reserve(args.size() + 1);
    values.push_back("portscope");
    values.insert(values.end(), args.begin(), args.end());

    std::vector<char*> raw_argv;
    raw_argv.reserve(values.size());
    for (std::string& value : values) {
        raw_argv.push_back(value.data());
    }

    return portscope::ParseCommandLine(static_cast<int>(raw_argv.size()), raw_argv.data());
}

void TestPortParserValidCases() {
    std::vector<std::uint16_t> ports;
    std::string error;

    Require(portscope::ParsePortSpec("22", ports, error), "Expected single port parse to succeed");
    Require(ports.size() == 1 && ports[0] == 22, "Single port result mismatch");

    Require(portscope::ParsePortSpec("80-82,22,80", ports, error), "Expected mixed parse to succeed");
    const std::vector<std::uint16_t> expected{22, 80, 81, 82};
    Require(ports == expected, "Mixed parse should be sorted and deduplicated");
}

void TestPortParserInvalidCases() {
    std::vector<std::uint16_t> ports;
    std::string error;

    Require(!portscope::ParsePortSpec("", ports, error), "Empty spec should fail");
    Require(!portscope::ParsePortSpec("100-90", ports, error), "Reversed range should fail");
    Require(!portscope::ParsePortSpec("0,10", ports, error), "Out-of-range low port should fail");
    Require(!portscope::ParsePortSpec("22,65536", ports, error), "Out-of-range high port should fail");
    Require(!portscope::ParsePortSpec("1,,2", ports, error), "Empty token should fail");
}

void TestCliValidation() {
    {
        const auto result = ParseCliForTest({"--help"});
        Require(result.ok, "--help should be valid");
        Require(result.options.command == portscope::Command::Help, "--help command mismatch");
    }

    {
        const auto result = ParseCliForTest({"--version"});
        Require(result.ok, "--version should be valid");
        Require(result.options.command == portscope::Command::Version, "--version command mismatch");
    }

    {
        const auto result = ParseCliForTest({"--host", "127.0.0.1", "--ports", "22,80-81", "--timeout", "500", "--concurrency", "5"});
        Require(result.ok, "valid CLI should parse");
        Require(result.options.command == portscope::Command::Run, "run command mismatch");
        Require(result.options.timeout_ms == 500, "timeout mismatch");
        Require(result.options.concurrency == 5, "concurrency mismatch");
    }

    Require(!ParseCliForTest({"--host"}).ok, "missing --host value should fail");
    Require(!ParseCliForTest({"--host", "--ports", "80"}).ok, "missing --host value should fail when next token is an option");
    Require(!ParseCliForTest({"--ports"}).ok, "missing --ports value should fail");
    Require(!ParseCliForTest({"--timeout", "0"}).ok, "zero timeout should fail");
    Require(!ParseCliForTest({"--timeout", "-1"}).ok, "negative timeout should fail");
    Require(!ParseCliForTest({"--concurrency", "0"}).ok, "zero concurrency should fail");
    Require(!ParseCliForTest({"--unknown"}).ok, "unknown option should fail");
    Require(!ParseCliForTest({"--host", "127.0.0.1"}).ok, "missing ports should fail");
    Require(!ParseCliForTest({"--ports", "80"}).ok, "missing host should fail");
}

void TestOutputFormatting() {
    portscope::ScanConfig config;
    config.host = "127.0.0.1";
    config.timeout_ms = 1000;
    config.concurrency = 2;

    portscope::ScanOutput output;
    output.resolved_address = "127.0.0.1";
    output.results = {{8080, portscope::PortState::Open, 0}};
    output.summary.open_count = 1;

    std::ostringstream stream;
    portscope::PrintScanResults(stream, config, output, std::chrono::milliseconds(0));
    const std::string text = stream.str();

    Require(text.find("8080/tcp  open") != std::string::npos, "port/state formatting should be readable");
    Require(text.find("Scanned:   1 port") != std::string::npos, "single-port summary grammar mismatch");
    Require(text.find("Elapsed:   <1 ms") != std::string::npos, "sub-millisecond elapsed display mismatch");

    output.results.push_back({8081, portscope::PortState::Closed, ECONNREFUSED});
    output.summary.closed_count = 1;

    std::ostringstream stream_plural;
    portscope::PrintScanResults(stream_plural, config, output, std::chrono::milliseconds(1500));
    const std::string plural_text = stream_plural.str();

    Require(plural_text.find("Scanned:   2 ports") != std::string::npos, "multi-port summary grammar mismatch");
    Require(plural_text.find("Elapsed:   1.50 s") != std::string::npos, "second-based elapsed display mismatch");
}

void TestClassification() {
    Require(portscope::ClassifySocketError(0) == portscope::PortState::Open, "0 should be open");
    Require(portscope::ClassifySocketError(ECONNREFUSED) == portscope::PortState::Closed, "ECONNREFUSED should be closed");
    Require(portscope::ClassifySocketError(ETIMEDOUT) == portscope::PortState::Filtered, "ETIMEDOUT should be filtered");
}

std::uint16_t CreateClosedCandidatePort() {
    ScopedFd fd(socket(AF_INET, SOCK_STREAM, 0));
    if (fd.Get() < 0) {
        throw std::runtime_error("socket failed while preparing closed port");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    if (bind(fd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("bind failed while preparing closed port");
    }

    socklen_t length = sizeof(address);
    if (getsockname(fd.Get(), reinterpret_cast<sockaddr*>(&address), &length) < 0) {
        throw std::runtime_error("getsockname failed while preparing closed port");
    }

    return ntohs(address.sin_port);
}

void TestLoopbackIntegration() {
    ScopedFd listener(socket(AF_INET, SOCK_STREAM, 0));
    Require(listener.Get() >= 0, "listener socket creation failed");

    int reuse = 1;
    Require(setsockopt(listener.Get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0, "setsockopt failed");

    sockaddr_in listen_address{};
    listen_address.sin_family = AF_INET;
    listen_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listen_address.sin_port = htons(0);

    Require(bind(listener.Get(), reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address)) == 0, "bind failed");
    Require(listen(listener.Get(), 8) == 0, "listen failed");

    socklen_t length = sizeof(listen_address);
    Require(getsockname(listener.Get(), reinterpret_cast<sockaddr*>(&listen_address), &length) == 0, "getsockname failed");

    const std::uint16_t open_port = ntohs(listen_address.sin_port);
    const std::uint16_t closed_port = CreateClosedCandidatePort();

    portscope::ScanConfig config;
    config.host = "127.0.0.1";
    config.timeout_ms = 500;
    config.concurrency = 2;
    config.ports = {closed_port, open_port};

    portscope::ScanOutput output;
    std::string error;
    Require(portscope::ScanPorts(config, output, error), "ScanPorts failed: " + error);
    Require(output.results.size() == 2, "Expected two scan results");

    const auto first = output.results[0].state;
    const auto second = output.results[1].state;

    Require(first == portscope::PortState::Closed || second == portscope::PortState::Closed,
            "Expected one closed loopback port result");
    Require(first == portscope::PortState::Open || second == portscope::PortState::Open,
            "Expected one open loopback port result");
}

void TestScanConfigValidation() {
    portscope::ScanConfig config;
    portscope::ScanOutput output;
    std::string error;

    config.host.clear();
    config.ports = {80};
    config.timeout_ms = 1000;
    config.concurrency = 1;
    Require(!portscope::ScanPorts(config, output, error), "empty host should fail");

    config.host = "127.0.0.1";
    config.ports.clear();
    Require(!portscope::ScanPorts(config, output, error), "empty port list should fail");

    config.ports = {80};
    config.timeout_ms = 0;
    Require(!portscope::ScanPorts(config, output, error), "zero timeout should fail");

    config.timeout_ms = 1000;
    config.concurrency = 0;
    Require(!portscope::ScanPorts(config, output, error), "zero concurrency should fail");
}

void RunTest(const std::string& name, const std::function<void()>& fn) {
    fn();
    std::cout << "[PASS] " << name << "\n";
}

}  // namespace

int main() {
    int failures = 0;

    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"port_parser_valid", TestPortParserValidCases},
        {"port_parser_invalid", TestPortParserInvalidCases},
        {"cli_validation", TestCliValidation},
        {"output_formatting", TestOutputFormatting},
        {"classification", TestClassification},
        {"loopback_integration", TestLoopbackIntegration},
        {"scan_config_validation", TestScanConfigValidation},
    };

    for (const auto& test : tests) {
        try {
            RunTest(test.first, test.second);
        } catch (const std::exception& ex) {
            ++failures;
            std::cerr << "[FAIL] " << test.first << ": " << ex.what() << "\n";
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
