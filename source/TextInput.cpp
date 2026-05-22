#include "switchbox/TextInput.hpp"

#include <iostream>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switchbox {

bool promptText(const char* header, const char* subText, const std::string& initial, std::string& output) {
#ifdef __SWITCH__
    SwkbdConfig keyboard;
    if (R_FAILED(swkbdCreate(&keyboard, 0))) return false;
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, header ? header : "Input");
    swkbdConfigSetSubText(&keyboard, subText ? subText : "");
    swkbdConfigSetInitialText(&keyboard, initial.c_str());
    swkbdConfigSetStringLenMax(&keyboard, 256);
    char buffer[257] = {0};
    const Result rc = swkbdShow(&keyboard, buffer, sizeof(buffer));
    swkbdClose(&keyboard);
    if (R_FAILED(rc)) return false;
    output = buffer;
    return true;
#else
    std::cout << (header ? header : "Input") << ": ";
    if (!initial.empty()) std::cout << "[" << initial << "] ";
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    output = line.empty() ? initial : line;
    return true;
#endif
}

} // namespace switchbox
