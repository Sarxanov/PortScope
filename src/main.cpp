
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        std::cout << "PortScope v0.2.0\n"
                  << "Usage: portscope [OPTIONS]\n\n"
                  << "Options:\n"
                  << "  --help    Show this help message\n";

        return 0;
    }

    std::cout << "PortScope v0.2.0\n";
    std::cout << "TCP scanner initialization...\n";

    return 0;
}
