#pragma once

#include "switchbox/Models.hpp"
#include "switchbox/TvBox.hpp"

#include <string>
#include <vector>

namespace switchbox {

class SourceClient {
public:
    bool fetchVodList(const TvBoxSite& site,
                      int page,
                      const std::string& query,
                      const std::string& category,
                      std::vector<VodItem>& items,
                      std::string& error) const;

    bool fetchVodDetail(const TvBoxSite& site,
                        const VodItem& item,
                        VodDetail& detail,
                        std::string& error) const;

    bool fetchLiveChannels(const TvBoxLive& live,
                           std::vector<LiveChannel>& channels,
                           std::string& error) const;

    static std::string playableUrlFromEpisode(const TvBoxSite& site, const VodEpisode& episode);
};

} // namespace switchbox
