#include "switchbox/App.hpp"

#include "switchbox/Paths.hpp"
#include "switchbox/TextInput.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switchbox {
namespace {

int episodeCount(const VodDetail& detail) {
    int count = 0;
    for (const auto& line : detail.lines) count += static_cast<int>(line.episodes.size());
    return count;
}

bool episodeAt(const VodDetail& detail, int flatIndex, VodEpisode& out) {
    int index = 0;
    for (const auto& line : detail.lines) {
        for (const auto& episode : line.episodes) {
            if (index == flatIndex) {
                out = episode;
                return true;
            }
            index++;
        }
    }
    return false;
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool looksDirectPlayable(const std::string& url) {
    const std::string lower = lowerCopy(url);
    return lower.find(".m3u8") != std::string::npos ||
           lower.find(".mp4") != std::string::npos ||
           lower.find(".flv") != std::string::npos ||
           lower.find(".ts") != std::string::npos ||
           lower.find(".mkv") != std::string::npos ||
           lower.find(".webm") != std::string::npos ||
           lower.find(".mov") != std::string::npos;
}

std::string applyParseUrl(const TvBoxParse& parse, const std::string& rawUrl) {
    if (parse.url.empty()) return rawUrl;
    std::string result = parse.url;
    const size_t token = result.find("{url}");
    if (token != std::string::npos) {
        result.replace(token, 5, urlEncodeComponent(rawUrl));
        return result;
    }
    if (!result.empty() && (result.back() == '=' || result.back() == '/' || result.back() == '?')) {
        return result + urlEncodeComponent(rawUrl);
    }
    return result + urlEncodeComponent(rawUrl);
}

void pushUniqueFront(std::vector<LibraryEntry>& entries, const LibraryEntry& entry, size_t maxCount) {
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const LibraryEntry& other) {
        return other.kind == entry.kind && other.url == entry.url;
    }), entries.end());
    entries.insert(entries.begin(), entry);
    if (entries.size() > maxCount) entries.resize(maxCount);
}

const char* pageTitle(App::Page page) {
    switch (page) {
        case App::Page::Sources: return "Sources";
        case App::Page::VodList: return "VOD List";
        case App::Page::VodDetail: return "VOD Detail";
        case App::Page::Lives: return "Live Playlists";
        case App::Page::LiveChannels: return "Live Channels";
        case App::Page::Favorites: return "Favorites";
        case App::Page::History: return "History";
        case App::Page::Playback: return "Playback";
    }
    return "Unknown";
}

} // namespace

bool App::init() {
    configPath_ = defaultConfigPath();
    favoritesPath_ = defaultFavoritesPath();
    historyPath_ = defaultHistoryPath();
    settingsPath_ = defaultSettingsPath();
    std::string settingsError;
    settings_.load(settingsPath_, settingsError);
    if (!settingsError.empty()) status_ = "Settings load failed: " + settingsError;
    loadStorage();
    return reloadConfig();
}

bool App::reloadConfig() {
    std::string error;
    if (!config_.loadFromLocation(configPath_, error)) {
        status_ = "Config load failed: " + error;
        return false;
    }
    selected_ = 0;
    currentSiteIndex_ = -1;
    currentLiveIndex_ = -1;
    currentVodPage_ = 1;
    searchQuery_.clear();
    vodItems_.clear();
    liveChannels_.clear();
    page_ = Page::Sources;
    status_ = "Loaded " + std::to_string(config_.sites().size()) + " sites, " +
              std::to_string(config_.lives().size()) + " live playlists, " +
              std::to_string(config_.parses().size()) + " parsers.";
    return true;
}

bool App::loadStorage() {
    std::string error;
    bool ok = storage_.loadFavorites(favoritesPath_, favorites_, error);
    if (!ok) status_ = "Favorites load failed: " + error;
    error.clear();
    ok = storage_.loadHistory(historyPath_, history_, error) && ok;
    if (!error.empty()) status_ = "History load failed: " + error;
    return ok;
}

bool App::saveStorage() {
    std::string error;
    bool ok = storage_.saveFavorites(favoritesPath_, favorites_, error);
    if (!ok) { status_ = "Favorites save failed: " + error; return false; }
    ok = storage_.saveHistory(historyPath_, history_, error);
    if (!ok) { status_ = "History save failed: " + error; return false; }
    return true;
}

bool App::openSelected() {
    std::string error;
    if (currentPageSize() <= 0) return false;

    switch (page_) {
        case Page::Sources: {
            currentSiteIndex_ = selected_;
            currentVodPage_ = 1;
            const auto& site = config_.sites()[currentSiteIndex_];
            page_ = Page::VodList;
            selected_ = 0;
            vodItems_.clear();
            status_ = "Opening source: " + site.name;
            if (!client_.fetchVodList(site, currentVodPage_, searchQuery_, "", vodItems_, error)) {
                status_ = "Source fetch failed: " + error;
                return false;
            }
            status_ = "Loaded " + std::to_string(vodItems_.size()) + " items from " + site.name;
            return true;
        }
        case Page::VodList: {
            if (currentSiteIndex_ < 0 || currentSiteIndex_ >= static_cast<int>(config_.sites().size())) return false;
            const auto& site = config_.sites()[currentSiteIndex_];
            if (!client_.fetchVodDetail(site, vodItems_[selected_], vodDetail_, error)) {
                status_ = "Detail fetch failed: " + error;
                return false;
            }
            page_ = Page::VodDetail;
            selected_ = 0;
            status_ = "Detail loaded: " + vodDetail_.item.title;
            return true;
        }
        case Page::VodDetail: {
            VodEpisode episode;
            if (!episodeAt(vodDetail_, selected_, episode)) return false;
            return playEpisode(episode);
        }
        case Page::Lives: {
            currentLiveIndex_ = selected_;
            if (!client_.fetchLiveChannels(config_.lives()[currentLiveIndex_], liveChannels_, error)) {
                status_ = "Live fetch failed: " + error;
                return false;
            }
            page_ = Page::LiveChannels;
            selected_ = 0;
            status_ = "Loaded " + std::to_string(liveChannels_.size()) + " live channels.";
            return true;
        }
        case Page::LiveChannels:
            return playLive(liveChannels_[selected_]);
        case Page::Favorites:
            return startPlayback(favorites_[selected_], Page::Favorites);
        case Page::History:
            return startPlayback(history_[selected_], Page::History);
        case Page::Playback:
            return true;
    }
    return false;
}

bool App::back() {
    switch (page_) {
        case Page::VodList:
            page_ = Page::Sources;
            selected_ = currentSiteIndex_ >= 0 ? currentSiteIndex_ : 0;
            return true;
        case Page::VodDetail:
            page_ = Page::VodList;
            selected_ = 0;
            return true;
        case Page::LiveChannels:
            page_ = Page::Lives;
            selected_ = currentLiveIndex_ >= 0 ? currentLiveIndex_ : 0;
            return true;
        case Page::Playback:
            page_ = previousContentPage_;
            selected_ = 0;
            return true;
        default:
            return false;
    }
}

bool App::editConfigLocation() {
    std::string location;
    if (!promptText("TVBox Config", "Enter local path or http(s) URL", configPath_, location)) {
        status_ = "Config edit canceled.";
        return false;
    }
    if (location.empty()) {
        status_ = "Config location is empty.";
        return false;
    }
    configPath_ = location;
    return reloadConfig();
}

bool App::editCompanionServer() {
    std::string baseUrl;
    std::string initial = settings_.companionBaseUrl();
    if (!initial.empty()) initial += "|" + settings_.playbackMode();
    if (!promptText("Companion Server", "URL or URL|auto/proxy/transcode/direct", initial, baseUrl)) {
        status_ = "Companion edit canceled.";
        return false;
    }
    const size_t sep = baseUrl.find('|');
    std::string mode = "auto";
    if (sep != std::string::npos) {
        mode = baseUrl.substr(sep + 1);
        baseUrl = baseUrl.substr(0, sep);
    }
    settings_.setCompanionBaseUrl(baseUrl);
    settings_.setPlaybackMode(baseUrl.empty() ? "auto" : mode);
    std::string error;
    if (!settings_.save(settingsPath_, error)) {
        status_ = "Settings save failed: " + error;
        return false;
    }
    status_ = settings_.companionBaseUrl().empty()
        ? "Companion disabled. Playback uses direct URL."
        : "Companion enabled: " + settings_.companionBaseUrl() + " [" + settings_.playbackMode() + "]";
    return true;
}

bool App::searchCurrentSource() {
    int siteIndex = currentSiteIndex_;
    if (page_ == Page::Sources) siteIndex = selected_;
    if (siteIndex < 0 || siteIndex >= static_cast<int>(config_.sites().size())) {
        status_ = "Select a source before searching.";
        return false;
    }
    std::string query;
    if (!promptText("Search", "Enter TVBox source keyword", searchQuery_, query)) {
        status_ = "Search canceled.";
        return false;
    }
    searchQuery_ = query;
    currentSiteIndex_ = siteIndex;
    currentVodPage_ = 1;
    std::string error;
    if (!client_.fetchVodList(config_.sites()[currentSiteIndex_], currentVodPage_, searchQuery_, "", vodItems_, error)) {
        status_ = "Search failed: " + error;
        return false;
    }
    page_ = Page::VodList;
    selected_ = 0;
    status_ = "Search loaded " + std::to_string(vodItems_.size()) + " items.";
    return true;
}

bool App::addCurrentFavorite() {
    LibraryEntry entry;
    if (page_ == Page::VodDetail) {
        if (currentSiteIndex_ < 0 || currentSiteIndex_ >= static_cast<int>(config_.sites().size())) return false;
        VodEpisode episode;
        if (!episodeAt(vodDetail_, selected_, episode)) return false;
        entry = AppStorage::makeEpisodeEntry(vodDetail_.item, episode, SourceClient::playableUrlFromEpisode(config_.sites()[currentSiteIndex_], episode));
    } else if (page_ == Page::LiveChannels) {
        if (currentLiveIndex_ < 0 || currentLiveIndex_ >= static_cast<int>(config_.lives().size())) return false;
        entry = AppStorage::makeLiveEntry(liveChannels_[selected_], config_.lives()[currentLiveIndex_]);
    } else if (page_ == Page::Playback && !activePlaybackUrl_.empty()) {
        entry.kind = "playback";
        entry.title = activePlaybackTitle_;
        entry.url = activePlaybackUrl_;
        entry.timestamp = nowIso8601();
    } else {
        status_ = "Nothing playable selected to favorite.";
        return false;
    }

    pushUniqueFront(favorites_, entry, 200);
    saveStorage();
    status_ = "Added favorite: " + entry.title;
    return true;
}

bool App::startPlayback(const LibraryEntry& entry, Page previousPage) {
    PlaybackRequest request;
    request.title = entry.title;
    request.url = settings_.rewritePlaybackUrl(entry.url);
    std::string error;
    if (!player_.play(request, playbackState_, error)) {
        status_ = "Playback failed: " + error;
        return false;
    }
    activePlaybackTitle_ = playbackState_.title;
    activePlaybackUrl_ = playbackState_.url;
    previousContentPage_ = previousPage;
    page_ = Page::Playback;
    selected_ = 0;
    status_ = playbackState_.message;
    return true;
}

bool App::playEpisode(const VodEpisode& episode) {
    if (currentSiteIndex_ < 0 || currentSiteIndex_ >= static_cast<int>(config_.sites().size())) return false;
    const auto& site = config_.sites()[currentSiteIndex_];
    const std::string url = SourceClient::playableUrlFromEpisode(site, episode);
    std::string playableUrl = url;
    if (!looksDirectPlayable(playableUrl)) {
        for (const auto& parse : config_.parses()) {
            if (!parse.url.empty()) {
                playableUrl = applyParseUrl(parse, playableUrl);
                break;
            }
        }
    }
    LibraryEntry entry = AppStorage::makeEpisodeEntry(vodDetail_.item, episode, playableUrl);
    pushUniqueFront(history_, entry, 300);
    saveStorage();
    return startPlayback(entry, Page::VodDetail);
}

bool App::playLive(const LiveChannel& channel) {
    if (currentLiveIndex_ < 0 || currentLiveIndex_ >= static_cast<int>(config_.lives().size())) return false;
    LibraryEntry entry = AppStorage::makeLiveEntry(channel, config_.lives()[currentLiveIndex_]);
    pushUniqueFront(history_, entry, 300);
    saveStorage();
    return startPlayback(entry, Page::LiveChannels);
}

void App::nextPage() {
    if (page_ == Page::Sources) page_ = Page::Lives;
    else if (page_ == Page::Lives) page_ = Page::Favorites;
    else if (page_ == Page::Favorites) page_ = Page::History;
    else if (page_ == Page::History) page_ = Page::Sources;
    else page_ = Page::Sources;
    selected_ = 0;
}

void App::previousPage() {
    if (page_ == Page::Sources) page_ = Page::History;
    else if (page_ == Page::History) page_ = Page::Favorites;
    else if (page_ == Page::Favorites) page_ = Page::Lives;
    else if (page_ == Page::Lives) page_ = Page::Sources;
    else page_ = Page::Sources;
    selected_ = 0;
}

void App::moveSelection(int delta) {
    const int count = currentPageSize();
    if (count <= 0) return;
    selected_ = (selected_ + count + delta) % count;
}

int App::currentPageSize() const {
    switch (page_) {
        case Page::Sources: return static_cast<int>(config_.sites().size());
        case Page::VodList: return static_cast<int>(vodItems_.size());
        case Page::VodDetail: return episodeCount(vodDetail_);
        case Page::Lives: return static_cast<int>(config_.lives().size());
        case Page::LiveChannels: return static_cast<int>(liveChannels_.size());
        case Page::Favorites: return static_cast<int>(favorites_.size());
        case Page::History: return static_cast<int>(history_.size());
        case Page::Playback: return playbackState_.accepted ? 1 : 0;
    }
    return 0;
}

void App::renderSources() const {
    int i = 0;
    for (const auto& site : config_.sites()) {
        std::printf("%c %02d. %s [%s]\n", i == selected_ ? '>' : ' ', i + 1, site.name.c_str(), site.key.c_str());
        std::printf("      api: %s\n", site.api.c_str());
        i++;
    }
}

void App::renderVodList() const {
    std::printf("Source: %s | Query: %s | Page: %d\n\n",
        currentSiteIndex_ >= 0 ? config_.sites()[currentSiteIndex_].name.c_str() : "-",
        searchQuery_.empty() ? "-" : searchQuery_.c_str(), currentVodPage_);
    int i = 0;
    for (const auto& item : vodItems_) {
        std::printf("%c %02d. %s\n", i == selected_ ? '>' : ' ', i + 1, item.title.c_str());
        if (!item.remarks.empty()) std::printf("      %s\n", item.remarks.c_str());
        i++;
    }
    if (vodItems_.empty()) {
        std::printf("  (no items loaded)\n");
        std::printf("  If this source is type=3/CSP/Spider, use Companion resolve mode or choose a normal VOD API source.\n");
        std::printf("  Press B to return, Y to search/retry, or X to reload config.\n");
    }
}

void App::renderVodDetail() const {
    std::printf("Title: %s\n", vodDetail_.item.title.c_str());
    if (!vodDetail_.year.empty() || !vodDetail_.area.empty()) std::printf("Meta: %s %s\n", vodDetail_.year.c_str(), vodDetail_.area.c_str());
    if (!vodDetail_.actor.empty()) std::printf("Actor: %.160s\n", vodDetail_.actor.c_str());
    if (!vodDetail_.description.empty()) std::printf("Desc: %.200s\n", vodDetail_.description.c_str());
    std::printf("\nEpisodes:\n");
    int i = 0;
    for (const auto& line : vodDetail_.lines) {
        for (const auto& ep : line.episodes) {
            std::printf("%c %02d. [%s] %s\n", i == selected_ ? '>' : ' ', i + 1, ep.line.c_str(), ep.title.c_str());
            i++;
        }
    }
    if (i == 0) std::printf("  (no playable episodes found)\n");
}

void App::renderLives() const {
    int i = 0;
    for (const auto& live : config_.lives()) {
        std::printf("%c %02d. %s\n", i == selected_ ? '>' : ' ', i + 1, live.name.c_str());
        std::printf("      %s\n", live.url.c_str());
        i++;
    }
}

void App::renderLiveChannels() const {
    int i = 0;
    for (const auto& channel : liveChannels_) {
        std::printf("%c %03d. %s", i == selected_ ? '>' : ' ', i + 1, channel.name.c_str());
        if (!channel.group.empty()) std::printf(" [%s]", channel.group.c_str());
        std::printf("\n");
        i++;
    }
}

void App::renderLibrary(const std::vector<LibraryEntry>& entries, const char* title) const {
    std::printf("%s\n\n", title);
    int i = 0;
    for (const auto& entry : entries) {
        std::printf("%c %02d. %s\n", i == selected_ ? '>' : ' ', i + 1, entry.title.c_str());
        std::printf("      %s | %s\n", entry.kind.c_str(), entry.timestamp.c_str());
        i++;
    }
}

void App::renderPlayback() const {
    std::printf("Playback request prepared\n\n");
    std::printf("Title: %s\n", playbackState_.title.c_str());
    std::printf("URL: %s\n", playbackState_.url.c_str());
    std::printf("Backend: %s\n", playbackState_.backendName.c_str());
    std::printf("Renders video: %s\n\n", playbackState_.rendersVideo ? "yes" : "no");
    std::printf("%s\n", playbackState_.message.c_str());
}

void App::render() const {
#ifdef __SWITCH__
    consoleClear();
#endif
    std::printf("SwitchBox Lite 0.2.0\n");
    std::printf("TVBox-like client for Nintendo Switch homebrew\n");
    std::printf("------------------------------------------------\n");
    std::printf("Page: %s | Selected: %d/%d\n", pageTitle(page_), currentPageSize() > 0 ? selected_ + 1 : 0, currentPageSize());
    std::printf("Config: %s\n", configPath_.c_str());
    std::printf("Companion: %s [%s]\n",
        settings_.companionBaseUrl().empty() ? "off" : settings_.companionBaseUrl().c_str(),
        settings_.playbackMode().c_str());
    std::printf("Status: %s\n\n", status_.c_str());

    switch (page_) {
        case Page::Sources: renderSources(); break;
        case Page::VodList: renderVodList(); break;
        case Page::VodDetail: renderVodDetail(); break;
        case Page::Lives: renderLives(); break;
        case Page::LiveChannels: renderLiveChannels(); break;
        case Page::Favorites: renderLibrary(favorites_, "Favorites"); break;
        case Page::History: renderLibrary(history_, "History"); break;
        case Page::Playback: renderPlayback(); break;
    }

    std::printf("\nControls: A open/play | B back | Up/Down select | L/R main tabs\n");
    std::printf("          X reload config | Y search | ZL config URL | ZR companion | - favorite | + exit\n");
}

void App::run() {
#ifdef __SWITCH__
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus) break;
        if (down & HidNpadButton_A) openSelected();
        if (down & HidNpadButton_B) back();
        if (down & HidNpadButton_X) reloadConfig();
        if (down & HidNpadButton_Y) searchCurrentSource();
        if (down & HidNpadButton_ZL) editConfigLocation();
        if (down & HidNpadButton_ZR) editCompanionServer();
        if (down & HidNpadButton_Minus) addCurrentFavorite();
        if (down & HidNpadButton_L) previousPage();
        if (down & HidNpadButton_R) nextPage();
        if (down & HidNpadButton_Up) moveSelection(-1);
        if (down & HidNpadButton_Down) moveSelection(1);

        render();
        consoleUpdate(nullptr);
    }
    saveStorage();
    consoleExit(nullptr);
#else
    render();
#endif
}

} // namespace switchbox
