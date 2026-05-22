#pragma once

#include <string>

namespace switchbox {

class AppSettings {
public:
    bool load(const std::string& path, std::string& error);
    bool save(const std::string& path, std::string& error) const;

    std::string rewritePlaybackUrl(const std::string& url) const;

    const std::string& companionBaseUrl() const { return companionBaseUrl_; }
    const std::string& playbackMode() const { return playbackMode_; }
    void setCompanionBaseUrl(std::string value) { companionBaseUrl_ = normalizeBaseUrl(value); }
    void setPlaybackMode(std::string value) { playbackMode_ = value.empty() ? "auto" : value; }

private:
    static std::string normalizeBaseUrl(const std::string& value);

    std::string companionBaseUrl_;
    std::string playbackMode_ = "auto"; // auto, proxy, transcode, direct
};

std::string urlEncodeComponent(const std::string& value);

} // namespace switchbox
