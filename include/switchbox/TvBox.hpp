#pragma once

#include "switchbox/Models.hpp"

#include <string>
#include <vector>

namespace switchbox {

class TvBoxConfig {
public:
    bool loadFromLocation(const std::string& location, std::string& error);
    bool loadFromText(const std::string& text, std::string& error);

    const std::vector<TvBoxSite>& sites() const { return sites_; }
    const std::vector<TvBoxLive>& lives() const { return lives_; }
    const std::vector<TvBoxParse>& parses() const { return parses_; }

private:
    std::vector<TvBoxSite> sites_;
    std::vector<TvBoxLive> lives_;
    std::vector<TvBoxParse> parses_;
};

} // namespace switchbox
