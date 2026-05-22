#include "switchbox/SourceClient.hpp"

#include "switchbox/Http.hpp"
#include "switchbox/Json.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace switchbox {
namespace {

std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::vector<std::string> split(const std::string& text, const std::string& delim) {
    std::vector<std::string> out;
    if (delim.empty()) {
        out.push_back(text);
        return out;
    }
    size_t pos = 0;
    while (true) {
        const size_t next = text.find(delim, pos);
        if (next == std::string::npos) {
            out.push_back(text.substr(pos));
            break;
        }
        out.push_back(text.substr(pos, next - pos));
        pos = next + delim.size();
    }
    return out;
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::ostringstream ss;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            ss << c;
        } else if (c == ' ') {
            ss << '+';
        } else {
            ss << '%' << hex[c >> 4] << hex[c & 15];
        }
    }
    return ss.str();
}

std::string appendQuery(std::string url, const std::vector<std::pair<std::string, std::string>>& params) {
    bool hasQuery = url.find('?') != std::string::npos;
    for (const auto& p : params) {
        if (p.second.empty()) continue;
        url += hasQuery ? '&' : '?';
        hasQuery = true;
        url += p.first + "=" + urlEncode(p.second);
    }
    return url;
}

std::string valueToString(const JsonValue& v) {
    if (v.isString()) return v.asString();
    if (v.isNumber()) return std::to_string(v.asInt());
    if (v.isBool()) return v.asBool() ? "1" : "0";
    return "";
}

std::string firstString(const JsonValue& object, const std::vector<const char*>& keys) {
    for (const char* key : keys) {
        const std::string value = valueToString(object.get(key));
        if (!value.empty()) return value;
    }
    return "";
}

const JsonValue* listArrayFromResponse(const JsonValue& root) {
    if (root.isArray()) return &root;
    if (root.get("list").isArray()) return &root.get("list");
    if (root.get("data").isArray()) return &root.get("data");
    if (root.get("data").get("list").isArray()) return &root.get("data").get("list");
    if (root.get("result").isArray()) return &root.get("result");
    if (root.get("result").get("list").isArray()) return &root.get("result").get("list");
    return nullptr;
}

VodItem parseVodItem(const JsonValue& item, const TvBoxSite& site) {
    VodItem vod;
    vod.id = firstString(item, {"vod_id", "id", "vid", "url"});
    vod.title = firstString(item, {"vod_name", "name", "title"});
    vod.picture = firstString(item, {"vod_pic", "pic", "cover", "poster"});
    vod.remarks = firstString(item, {"vod_remarks", "remarks", "note", "sub"});
    vod.category = firstString(item, {"type_name", "category", "class"});
    vod.rawPlayFrom = firstString(item, {"vod_play_from", "play_from", "from"});
    vod.rawPlayUrl = firstString(item, {"vod_play_url", "play_url", "url"});
    vod.sourceKey = site.key;
    vod.sourceName = site.name;
    if (vod.title.empty()) vod.title = vod.id.empty() ? "Untitled" : vod.id;
    return vod;
}

void parseEpisodes(const std::string& playFrom, const std::string& playUrl, std::vector<PlaybackLine>& lines) {
    const auto fromParts = split(playFrom, "$$$");
    const auto urlParts = split(playUrl, "$$$");
    const size_t count = std::max(fromParts.size(), urlParts.size());
    for (size_t i = 0; i < count; ++i) {
        PlaybackLine line;
        line.name = i < fromParts.size() && !trim(fromParts[i]).empty() ? trim(fromParts[i]) : "Line " + std::to_string(i + 1);
        const std::string part = i < urlParts.size() ? urlParts[i] : "";
        for (const auto& rawEpisode : split(part, "#")) {
            const std::string ep = trim(rawEpisode);
            if (ep.empty()) continue;
            const size_t sep = ep.rfind('$');
            VodEpisode episode;
            episode.line = line.name;
            if (sep == std::string::npos) {
                episode.title = ep;
                episode.url = ep;
            } else {
                episode.title = ep.substr(0, sep);
                episode.url = ep.substr(sep + 1);
            }
            if (!episode.url.empty()) line.episodes.push_back(episode);
        }
        if (!line.episodes.empty()) lines.push_back(line);
    }
}

VodDetail parseDetailObject(const JsonValue& object, const TvBoxSite& site, const VodItem& fallback) {
    VodDetail detail;
    detail.item = parseVodItem(object, site);
    if (detail.item.id.empty()) detail.item.id = fallback.id;
    if (detail.item.title.empty() || detail.item.title == "Untitled") detail.item.title = fallback.title;
    if (detail.item.picture.empty()) detail.item.picture = fallback.picture;
    if (detail.item.remarks.empty()) detail.item.remarks = fallback.remarks;
    if (detail.item.rawPlayFrom.empty()) detail.item.rawPlayFrom = fallback.rawPlayFrom;
    if (detail.item.rawPlayUrl.empty()) detail.item.rawPlayUrl = fallback.rawPlayUrl;
    detail.description = firstString(object, {"vod_content", "content", "description", "desc"});
    detail.director = firstString(object, {"vod_director", "director"});
    detail.actor = firstString(object, {"vod_actor", "actor", "actors"});
    detail.year = firstString(object, {"vod_year", "year"});
    detail.area = firstString(object, {"vod_area", "area"});
    parseEpisodes(detail.item.rawPlayFrom, detail.item.rawPlayUrl, detail.lines);
    return detail;
}

std::string buildVodListUrl(const TvBoxSite& site, int page, const std::string& query, const std::string& category) {
    std::vector<std::pair<std::string, std::string>> params;
    params.push_back({"ac", "videolist"});
    params.push_back({"pg", std::to_string(page <= 0 ? 1 : page)});
    if (!category.empty()) params.push_back({"t", category});
    if (!query.empty()) params.push_back({"wd", query});
    return appendQuery(site.api, params);
}

std::string buildDetailUrl(const TvBoxSite& site, const std::string& id) {
    return appendQuery(site.api, {{"ac", "detail"}, {"ids", id}});
}

std::string attributeValue(const std::string& line, const std::string& attr) {
    const std::string pattern = attr + "=\"";
    const size_t start = line.find(pattern);
    if (start == std::string::npos) return "";
    const size_t valueStart = start + pattern.size();
    const size_t end = line.find('"', valueStart);
    if (end == std::string::npos) return "";
    return line.substr(valueStart, end - valueStart);
}

std::vector<LiveChannel> parseM3u(const std::string& text) {
    std::vector<LiveChannel> out;
    std::istringstream ss(text);
    std::string line;
    LiveChannel pending;
    bool hasPending = false;

    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (startsWith(line, "#EXTINF")) {
            pending = LiveChannel{};
            pending.logo = attributeValue(line, "tvg-logo");
            pending.group = attributeValue(line, "group-title");
            const size_t comma = line.rfind(',');
            pending.name = comma == std::string::npos ? "Live Channel" : trim(line.substr(comma + 1));
            hasPending = true;
        } else if (!startsWith(line, "#")) {
            LiveChannel channel = hasPending ? pending : LiveChannel{};
            channel.url = line;
            if (channel.name.empty()) channel.name = line;
            out.push_back(channel);
            hasPending = false;
        }
    }
    return out;
}

} // namespace

bool SourceClient::fetchVodList(const TvBoxSite& site,
                                int page,
                                const std::string& query,
                                const std::string& category,
                                std::vector<VodItem>& items,
                                std::string& error) const {
    items.clear();
    if (site.api.empty()) {
        error = "site api is empty";
        return false;
    }
    std::string body;
    const std::string url = buildVodListUrl(site, page, query, category);
    if (!HttpClient::fetchText(url, body, error)) return false;

    JsonValue root;
    if (!JsonParser::parse(body, root, error)) return false;
    const JsonValue* list = listArrayFromResponse(root);
    if (!list) {
        error = "response does not contain a supported list array";
        return false;
    }
    for (const auto& item : list->asArray()) {
        if (!item.isObject()) continue;
        items.push_back(parseVodItem(item, site));
    }
    return true;
}

bool SourceClient::fetchVodDetail(const TvBoxSite& site,
                                  const VodItem& item,
                                  VodDetail& detail,
                                  std::string& error) const {
    error.clear();
    if (!item.rawPlayUrl.empty()) {
        detail.item = item;
        parseEpisodes(item.rawPlayFrom, item.rawPlayUrl, detail.lines);
        return true;
    }
    if (item.id.empty()) {
        error = "vod id is empty and list item has no play url";
        return false;
    }
    std::string body;
    const std::string url = buildDetailUrl(site, item.id);
    if (!HttpClient::fetchText(url, body, error)) return false;

    JsonValue root;
    if (!JsonParser::parse(body, root, error)) return false;
    const JsonValue* list = listArrayFromResponse(root);
    if (list && !list->asArray().empty() && list->asArray()[0].isObject()) {
        detail = parseDetailObject(list->asArray()[0], site, item);
        return true;
    }
    if (root.isObject()) {
        detail = parseDetailObject(root, site, item);
        return true;
    }
    error = "detail response is not a supported object/list";
    return false;
}

bool SourceClient::fetchLiveChannels(const TvBoxLive& live,
                                     std::vector<LiveChannel>& channels,
                                     std::string& error) const {
    channels.clear();
    std::string body;
    if (!HttpClient::fetchText(live.url, body, error)) return false;
    channels = parseM3u(body);
    if (channels.empty()) {
        error = "no live channels found in playlist";
        return false;
    }
    return true;
}

std::string SourceClient::playableUrlFromEpisode(const TvBoxSite& site, const VodEpisode& episode) {
    if (site.playUrl.empty()) return episode.url;
    std::string result = site.playUrl;
    const size_t token = result.find("{url}");
    if (token != std::string::npos) {
        result.replace(token, 5, episode.url);
        return result;
    }
    return result + episode.url;
}

} // namespace switchbox
