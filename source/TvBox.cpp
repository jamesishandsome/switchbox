#include "switchbox/TvBox.hpp"

#include "switchbox/Http.hpp"
#include "switchbox/Json.hpp"

namespace switchbox {
namespace {

std::string stringField(const JsonValue& object, const char* key) {
    return object.get(key).asString();
}

int intField(const JsonValue& object, const char* key, int fallback = 0) {
    const JsonValue& value = object.get(key);
    if (value.isNumber()) return value.asInt(fallback);
    if (value.isBool()) return value.asBool() ? 1 : 0;
    return fallback;
}

} // namespace

bool TvBoxConfig::loadFromLocation(const std::string& location, std::string& error) {
    std::string text;
    if (!HttpClient::fetchText(location, text, error)) return false;
    return loadFromText(text, error);
}

bool TvBoxConfig::loadFromText(const std::string& text, std::string& error) {
    sites_.clear();
    lives_.clear();
    parses_.clear();

    JsonValue root;
    if (!JsonParser::parse(text, root, error)) return false;
    if (!root.isObject()) {
        error = "config root must be a JSON object";
        return false;
    }

    for (const auto& item : root.get("sites").asArray()) {
        if (!item.isObject()) continue;
        TvBoxSite site;
        site.key = stringField(item, "key");
        site.name = stringField(item, "name");
        site.type = intField(item, "type");
        site.api = stringField(item, "api");
        site.searchable = intField(item, "searchable");
        site.quickSearch = intField(item, "quickSearch");
        site.filterable = intField(item, "filterable");
        site.playUrl = stringField(item, "playUrl");
        site.ext = stringField(item, "ext");
        site.jar = stringField(item, "jar");
        if (site.name.empty()) site.name = site.key.empty() ? "Unnamed Source" : site.key;
        if (!site.api.empty()) sites_.push_back(site);
    }

    for (const auto& item : root.get("lives").asArray()) {
        TvBoxLive live;
        if (item.isString()) {
            live.name = "Live Playlist";
            live.url = item.asString();
        } else if (item.isObject()) {
            live.name = stringField(item, "name");
            live.url = stringField(item, "url");
            if (live.name.empty()) live.name = "Live Playlist";
        }
        if (!live.url.empty()) lives_.push_back(live);
    }

    for (const auto& item : root.get("parses").asArray()) {
        if (!item.isObject()) continue;
        TvBoxParse parse;
        parse.name = stringField(item, "name");
        parse.url = stringField(item, "url");
        parse.type = intField(item, "type");
        if (parse.name.empty()) parse.name = parse.url.empty() ? "Direct" : parse.url;
        parses_.push_back(parse);
    }

    return true;
}

} // namespace switchbox
