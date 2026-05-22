#pragma once

#include <string>

namespace switchbox {

bool promptText(const char* header, const char* subText, const std::string& initial, std::string& output);

} // namespace switchbox
