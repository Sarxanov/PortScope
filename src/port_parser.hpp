#ifndef PORTSCOPE_PORT_PARSER_HPP
#define PORTSCOPE_PORT_PARSER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace portscope {

bool ParsePortSpec(const std::string& spec, std::vector<std::uint16_t>& ports, std::string& error);

}  // namespace portscope

#endif
