#include "switchbox/PlayerBackend.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switchbox {

bool PlayerBackend::play(const PlaybackRequest& request, PlaybackState& state, std::string& error) const {
    state = PlaybackState{};
    if (request.url.empty()) {
        error = "playback url is empty";
        return false;
    }

#ifdef __SWITCH__
    WebCommonConfig config;
    Result rc = webPageCreate(&config, request.url.c_str());
    if (R_FAILED(rc)) {
        error = "webPageCreate failed: 0x" + std::to_string(rc);
        return false;
    }

    webConfigSetBootAsMediaPlayer(&config, true);
    webConfigSetWebAudio(&config, true);
    webConfigSetMediaAutoPlay(&config, true);
    webConfigSetMediaPlayerSpeedControl(&config, true);
    webConfigSetScreenShot(&config, false);
    webConfigSetWhitelist(&config, ".*");
    webConfigSetUserAgentAdditionalString(&config, "SwitchBox/0.2");

    WebCommonReply reply;
    rc = webConfigShow(&config, &reply);
    if (R_FAILED(rc)) {
        error = "webConfigShow failed: 0x" + std::to_string(rc);
        return false;
    }

    state.title = request.title.empty() ? request.url : request.title;
    state.url = request.url;
    state.backendName = "switch-webapplet";
    state.accepted = true;
    state.rendersVideo = true;
    state.message = "Opened Switch WebApplet media player. Codec/support depends on system WebApplet.";
    return true;
#else
    state.title = request.title.empty() ? request.url : request.title;
    state.url = request.url;
    state.backendName = "request-only";
    state.accepted = true;
    state.rendersVideo = false;
    state.message = "Playback URL prepared. Link a Switch video backend to render it.";
    return true;
#endif
}

} // namespace switchbox
