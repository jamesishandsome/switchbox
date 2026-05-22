#pragma once

#include <string>

namespace switchbox {

inline std::string appRootPath() {
#ifdef __SWITCH__
    return "sdmc:/switch/switchbox";
#else
    return ".";
#endif
}

inline std::string defaultConfigPath() {
#ifdef __SWITCH__
    return "sdmc:/switch/switchbox/config.json";
#else
    return "examples/tvbox.demo.json";
#endif
}

inline std::string defaultFavoritesPath() {
#ifdef __SWITCH__
    return "sdmc:/switch/switchbox/favorites.json";
#else
    return "favorites.json";
#endif
}

inline std::string defaultHistoryPath() {
#ifdef __SWITCH__
    return "sdmc:/switch/switchbox/history.json";
#else
    return "history.json";
#endif
}

inline std::string defaultSettingsPath() {
#ifdef __SWITCH__
    return "sdmc:/switch/switchbox/settings.json";
#else
    return "settings.json";
#endif
}

} // namespace switchbox
