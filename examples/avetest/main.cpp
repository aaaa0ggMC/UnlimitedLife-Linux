#include "app.h"

auto main() -> int {
    try {
        avetest::App app;
        app.setup();
        app.run();
    } catch (...) {
        return 1;
    }
    return 0;
}
