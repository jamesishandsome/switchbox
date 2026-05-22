#pragma once

#include "switchbox/Models.hpp"

#include <string>
#include <vector>

namespace switchbox {

class AppStorage {
public:
    bool loadFavorites(const std::string& path, std::vector<LibraryEntry>& out, std::string& error) const;
    bool saveFavorites(const std::string& path, const std::vector<LibraryEntry>& entries, std::string& error) const;

    bool loadHistory(const std::string& path, std::vector<LibraryEntry>& out, std::string& error) const;
    bool saveHistory(const std::string& path, const std::vector<LibraryEntry>& entries, std::string& error) const;

    static LibraryEntry makeEpisodeEntry(const VodItem& item, const VodEpisode& episode, const std::string& url);
    static LibraryEntry makeLiveEntry(const LiveChannel& channel, const TvBoxLive& live);
};

std::string nowIso8601();

} // namespace switchbox
