#include "cli.hpp"

#include <cctype>
#include <limits>
#include <string>

namespace portscope {
namespace {

bool ParseStrictPositiveInt(const std::string& value, int& parsed) {
    if (value.empty()) {
        return false;
    }

    long long total = 0;
    for (const char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }

        total = total * 10 + (ch - '0');
        if (total > std::numeric_limits<int>::max()) {
            return false;
        }
    }

    if (total <= 0) {
        return false;
    }

    parsed = static_cast<int>(total);
    return true;
}

bool RequireValue(int argc, char* argv[], int index, std::string& value) {
    if (index + 1 >= argc) {
        return false;
    }

    value = argv[index + 1];
    return true;
}

}  // namespace

ParseResult ParseCommandLine(int argc, char* argv[]) {
    ParseResult result;
    result.ok = false;

    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help") {
            options.command = Command::Help;
            continue;
        }

        if (argument == "--version") {
            options.command = Command::Version;
            continue;
        }

        if (argument == "--host") {
            std::string value;
            if (!RequireValue(argc, argv, i, value)) {
                result.error = "Missing value for --host";
                return result;
            }

            options.host = value;
            ++i;
            continue;
        }

        if (argument == "--ports") {
            std::string value;
            if (!RequireValue(argc, argv, i, value)) {
                result.error = "Missing value for --ports";
                return result;
            }

            options.ports_spec = value;
            ++i;
            continue;
        }

        if (argument == "--timeout") {
            std::string value;
            if (!RequireValue(argc, argv, i, value)) {
                result.error = "Missing value for --timeout";
                return result;
            }

            int timeout_ms = 0;
            if (!ParseStrictPositiveInt(value, timeout_ms)) {
                result.error = "Invalid --timeout value: " + value;
                return result;
            }

            options.timeout_ms = timeout_ms;
            ++i;
            continue;
        }

        if (argument == "--concurrency") {
            std::string value;
            if (!RequireValue(argc, argv, i, value)) {
                result.error = "Missing value for --concurrency";
                return result;
            }

            int concurrency = 0;
            if (!ParseStrictPositiveInt(value, concurrency)) {
                result.error = "Invalid --concurrency value: " + value;
                return result;
            }

            options.concurrency = concurrency;
            ++i;
            continue;
        }

        result.error = "Unknown option: " + argument;
        return result;
    }

    if (options.command == Command::Run) {
        if (options.host.empty()) {
            result.error = "Missing required option: --host";
            return result;
        }

        if (options.ports_spec.empty()) {
            result.error = "Missing required option: --ports";
            return result;
        }
    }

    result.ok = true;
    result.options = options;
    return result;
}

}  // namespace portscope
