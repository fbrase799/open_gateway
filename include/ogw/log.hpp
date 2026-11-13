#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ogw {

enum class LogLevel { Error, Warn, Info, Debug };

void set_log_level(LogLevel level);
LogLevel log_level();
void log_message(LogLevel level, std::string_view component, std::string_view message,
                 const std::vector<std::pair<std::string, std::string>>& fields = {});

}  // namespace ogw
