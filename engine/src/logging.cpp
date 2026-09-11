#include "a2e/logging.hpp"

#include <iostream>

namespace a2e {

void log_info(const std::string& message) { std::clog << "[A2E][INFO] " << message << '\n'; }
void log_error(const std::string& message) { std::cerr << "[A2E][ERROR] " << message << '\n'; }

} // namespace a2e
