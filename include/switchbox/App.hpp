#pragma once

#include "switchbox/Models.hpp"
#include "switchbox/PlayerBackend.hpp"
#include "switchbox/Settings.hpp"
#include "switchbox/SourceClient.hpp"
#include "switchbox/Storage.hpp"
#include "switchbox/TvBox.hpp"

#include <string>
#include <vector>

namespace switchbox {

class App {
public:
    enum class Page {
        Sources,
        VodList,
        VodDetail,
        Lives,
        LiveChannels,
        Favorites,
        History,
        Playback,
    };

    bool init();
    void run();

private:
    bool reloadConfig();
    bool loadStorage();
    bool saveStorage();
    bool openSelected();
    bool back();
    bool editConfigLocation();
    bool editCompanionServer();
    bool searchCurrentSource();
    bool addCurrentFavorite();
    bool startPlayback(const LibraryEntry& entry, Page previousPage);
    bool playEpisode(const VodEpisode& episode);
    bool playLive(const LiveChannel& channel);

    void nextPage();
    void previousPage();
    void moveSelection(int delta);
    int currentPageSize() const;

    void render() const;
    void renderSources() const;
    void renderVodList() const;
    void renderVodDetail() const;
    void renderLives() const;
    void renderLiveChannels() const;
    void renderLibrary(const std::vector<LibraryEntry>& entries, const char* title) const;
    void renderPlayback() const;

    TvBoxConfig config_;
    SourceClient client_;
    AppStorage storage_;
    PlayerBackend player_;
    AppSettings settings_;

    std::string configPath_;
    std::string favoritesPath_;
    std::string historyPath_;
    std::string settingsPath_;
    std::string status_;
    std::string searchQuery_;
    std::string activePlaybackTitle_;
    std::string activePlaybackUrl_;
    PlaybackState playbackState_;

    Page page_ = Page::Sources;
    Page previousContentPage_ = Page::Sources;
    int selected_ = 0;
    int currentSiteIndex_ = -1;
    int currentLiveIndex_ = -1;
    int currentVodPage_ = 1;

    std::vector<VodItem> vodItems_;
    VodDetail vodDetail_;
    std::vector<LiveChannel> liveChannels_;
    std::vector<LibraryEntry> favorites_;
    std::vector<LibraryEntry> history_;
};

} // namespace switchbox
