#include "switchbox/Settings.hpp"

#include "switchbox/Json.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace switchbox {
namespace {

bool readTextIfExists(const std::string& path, std::string& text) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        text.clear();
        return true;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    text = ss.str();
    return true;
}

bool writeText(const std::string& path, const std::string& text, std::string& error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "failed to write settings: " + path;
        return false;
    }
    file << text;
    return true;
}

} // namespace

std::string urlEncodeComponent(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::ostringstream ss;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            ss << c;
        } else {
            ss << '%' << hex[c >> 4] << hex[c & 15];
        }
    }
    return ss.str();
}

std::string AppSettings::normalizeBaseUrl(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) start++;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) end--;
    std::string out = value.substr(start, end - start);
    while (!out.empty() && out.back() == '/') out.pop_back();
    return out;
}

bool AppSettings::load(const std::string& path, std::string& error) {
    std::string text;
    readTextIfExists(path, text);
    if (text.empty()) return true;

    JsonValue root;
    if (!JsonParser::parse(text, root, error)) return false;
    if (!root.isObject()) {
        error = "settings root must be object";
        return false;
    }
    setCompanionBaseUrl(root.get("companionBaseUrl").asString());
    setPlaybackMode(root.get("playbackMode").asStringOr("auto"));
    return true;
}

bool AppSettings::save(const std::string& path, std::string& error) const {
    JsonValue root = JsonValue::makeObject({
        {"companionBaseUrl", JsonValue::makeString(companionBaseUrl_)},
        {"playbackMode", JsonValue::makeString(playbackMode_)},
    });
    return writeText(path, JsonWriter::stringify(root, true), error);
}

std::string AppSettings::rewritePlaybackUrl(const std::string& url) const {
    if (url.empty()) return url;
    if (companionBaseUrl_.empty() || playbackMode_ == "direct") return url;
    return companionBaseUrl_ + "/play?mode=" + urlEncodeComponent(playbackMode_) + "&url=" + urlEncodeComponent(url);
}

} // namespace switchbox
