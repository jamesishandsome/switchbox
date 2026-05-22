#pragma once

#include <string>
#include <vector>

namespace switchbox {

struct TvBoxSite {
    std::string key;
    std::string name;
    int type = 0;
    std::string api;
    int searchable = 0;
    int quickSearch = 0;
    int filterable = 0;
    std::string playUrl;
    std::string ext;
    std::string jar;
};

struct TvBoxLive {
    std::string name;
    std::string url;
};

struct TvBoxParse {
    std::string name;
    std::string url;
    int type = 0;
};

struct VodItem {
    std::string id;
    std::string title;
    std::string picture;
    std::string remarks;
    std::string category;
    std::string sourceKey;
    std::string sourceName;
    std::string rawPlayFrom;
    std::string rawPlayUrl;
};

struct VodEpisode {
    std::string line;
    std::string title;
    std::string url;
};

struct PlaybackLine {
    std::string name;
    std::vector<VodEpisode> episodes;
};

struct VodDetail {
    VodItem item;
    std::string description;
    std::string director;
    std::string actor;
    std::string year;
    std::string area;
    std::vector<PlaybackLine> lines;
};

struct LiveChannel {
    std::string name;
    std::string url;
    std::string group;
    std::string logo;
};

struct LibraryEntry {
    std::string kind;      // vod, live, episode
    std::string title;
    std::string url;
    std::string sourceKey;
    std::string sourceName;
    std::string extra;
    std::string timestamp;
};

} // namespace switchbox
