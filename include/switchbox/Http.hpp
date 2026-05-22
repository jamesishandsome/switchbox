#pragma once

#include <string>

namespace switchbox {

class HttpClient {
public:
    static bool isUrl(const std::string& location);
    static bool fetchText(const std::string& location, std::string& body, std::string& error);
};

} // namespace switchbox
