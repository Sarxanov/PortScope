#ifndef PORTSCOPE_CLI_HPP
#define PORTSCOPE_CLI_HPP

#include <string>

namespace portscope {

enum class Command {
    Run,
    Help,
    Version,
};

struct Options {
    Command command = Command::Run;
    std::string host;
    std::string ports_spec;
    int timeout_ms = 1000;
    int concurrency = 100;
};

struct ParseResult {
    bool ok = false;
    Options options{};
    std::string error;
};

ParseResult ParseCommandLine(int argc, char* argv[]);

}  // namespace portscope

#endif
