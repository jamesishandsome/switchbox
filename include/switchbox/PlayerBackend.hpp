#pragma once

#include <string>

namespace switchbox {

struct PlaybackRequest {
    std::string title;
    std::string url;
};

struct PlaybackState {
    std::string title;
    std::string url;
    std::string backendName;
    std::string message;
    bool accepted = false;
    bool rendersVideo = false;
};

class PlayerBackend {
public:
    bool play(const PlaybackRequest& request, PlaybackState& state, std::string& error) const;
};

} // namespace switchbox
