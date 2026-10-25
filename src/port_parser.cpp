#include "port_parser.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>

namespace portscope {
namespace {

constexpr int kMinPort = 1;
constexpr int kMaxPort = 65535;

std::string Trim(const std::string& text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(start, end - start);
}

bool ParseIntToken(const std::string& token, int& value) {
    if (token.empty()) {
        return false;
    }

    int total = 0;
    for (const char ch : token) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }

        total = total * 10 + (ch - '0');
        if (total > kMaxPort) {
            return false;
        }
    }

    value = total;
    return true;
}

bool IsValidPort(int port) {
    return port >= kMinPort && port <= kMaxPort;
}

}  // namespace

bool ParsePortSpec(const std::string& spec, std::vector<std::uint16_t>& ports, std::string& error) {
    ports.clear();
    error.clear();

    if (spec.empty()) {
        error = "Port specification cannot be empty";
        return false;
    }

    std::set<std::uint16_t> unique_ports;

    std::size_t start = 0;
    while (start <= spec.size()) {
        const std::size_t comma_position = spec.find(',', start);
        const std::string raw_token = spec.substr(start, comma_position == std::string::npos ? std::string::npos : comma_position - start);
        const std::string token = Trim(raw_token);

        if (token.empty()) {
            error = "Invalid empty token in port specification";
            return false;
        }

        const std::size_t dash_position = token.find('-');

        if (dash_position == std::string::npos) {
            int port = 0;
            if (!ParseIntToken(token, port) || !IsValidPort(port)) {
                error = "Invalid port: " + token;
                return false;
            }

            unique_ports.insert(static_cast<std::uint16_t>(port));
        } else {
            if (token.find('-', dash_position + 1) != std::string::npos) {
                error = "Invalid range token: " + token;
                return false;
            }

            const std::string start_token = Trim(token.substr(0, dash_position));
            const std::string end_token = Trim(token.substr(dash_position + 1));

            int range_start = 0;
            int range_end = 0;
            if (!ParseIntToken(start_token, range_start) || !ParseIntToken(end_token, range_end)) {
                error = "Invalid range: " + token;
                return false;
            }

            if (!IsValidPort(range_start) || !IsValidPort(range_end)) {
                error = "Port range out of bounds: " + token;
                return false;
            }

            if (range_start > range_end) {
                error = "Reversed range is not allowed: " + token;
                return false;
            }

            for (int port = range_start; port <= range_end; ++port) {
                unique_ports.insert(static_cast<std::uint16_t>(port));
            }
        }

        if (comma_position == std::string::npos) {
            break;
        }

        start = comma_position + 1;
    }

    ports.assign(unique_ports.begin(), unique_ports.end());
    return true;
}

}  // namespace portscope
