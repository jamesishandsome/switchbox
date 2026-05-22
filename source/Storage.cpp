#include "switchbox/Storage.hpp"

#include "switchbox/Json.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace switchbox {
namespace {

bool readTextIfExists(const std::string& path, std::string& text, std::string& error) {
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
        error = "failed to write file: " + path;
        return false;
    }
    file << text;
    return true;
}

LibraryEntry entryFromJson(const JsonValue& object) {
    LibraryEntry e;
    e.kind = object.get("kind").asString();
    e.title = object.get("title").asString();
    e.url = object.get("url").asString();
    e.sourceKey = object.get("sourceKey").asString();
    e.sourceName = object.get("sourceName").asString();
    e.extra = object.get("extra").asString();
    e.timestamp = object.get("timestamp").asString();
    return e;
}

JsonValue entryToJson(const LibraryEntry& e) {
    return JsonValue::makeObject({
        {"kind", JsonValue::makeString(e.kind)},
        {"title", JsonValue::makeString(e.title)},
        {"url", JsonValue::makeString(e.url)},
        {"sourceKey", JsonValue::makeString(e.sourceKey)},
        {"sourceName", JsonValue::makeString(e.sourceName)},
        {"extra", JsonValue::makeString(e.extra)},
        {"timestamp", JsonValue::makeString(e.timestamp)},
    });
}

bool loadEntries(const std::string& path, std::vector<LibraryEntry>& out, std::string& error) {
    out.clear();
    std::string text;
    if (!readTextIfExists(path, text, error)) return false;
    if (text.empty()) return true;
    JsonValue root;
    if (!JsonParser::parse(text, root, error)) return false;
    if (!root.isArray()) {
        error = "library file must be a JSON array: " + path;
        return false;
    }
    for (const auto& item : root.asArray()) {
        if (!item.isObject()) continue;
        LibraryEntry e = entryFromJson(item);
        if (!e.url.empty()) out.push_back(e);
    }
    return true;
}

bool saveEntries(const std::string& path, const std::vector<LibraryEntry>& entries, std::string& error) {
    std::vector<JsonValue> array;
    for (const auto& e : entries) array.push_back(entryToJson(e));
    return writeText(path, JsonWriter::stringify(JsonValue::makeArray(array), true), error);
}

} // namespace

std::string nowIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

bool AppStorage::loadFavorites(const std::string& path, std::vector<LibraryEntry>& out, std::string& error) const {
    return loadEntries(path, out, error);
}

bool AppStorage::saveFavorites(const std::string& path, const std::vector<LibraryEntry>& entries, std::string& error) const {
    return saveEntries(path, entries, error);
}

bool AppStorage::loadHistory(const std::string& path, std::vector<LibraryEntry>& out, std::string& error) const {
    return loadEntries(path, out, error);
}

bool AppStorage::saveHistory(const std::string& path, const std::vector<LibraryEntry>& entries, std::string& error) const {
    return saveEntries(path, entries, error);
}

LibraryEntry AppStorage::makeEpisodeEntry(const VodItem& item, const VodEpisode& episode, const std::string& url) {
    LibraryEntry e;
    e.kind = "episode";
    e.title = item.title + " - " + episode.title;
    e.url = url;
    e.sourceKey = item.sourceKey;
    e.sourceName = item.sourceName;
    e.extra = episode.line;
    e.timestamp = nowIso8601();
    return e;
}

LibraryEntry AppStorage::makeLiveEntry(const LiveChannel& channel, const TvBoxLive& live) {
    LibraryEntry e;
    e.kind = "live";
    e.title = channel.name;
    e.url = channel.url;
    e.sourceKey = live.name;
    e.sourceName = live.name;
    e.extra = channel.group;
    e.timestamp = nowIso8601();
    return e;
}

} // namespace switchbox
