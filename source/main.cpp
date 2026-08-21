#include "App.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    app::App application;

    if (!application.init()) {
        return 1;
    }

    application.run();
    application.shutdown();

    return 0;
}
