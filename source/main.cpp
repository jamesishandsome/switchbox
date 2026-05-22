#include "switchbox/App.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    switchbox::App app;
    app.init();
    app.run();
    return 0;
}
