#include "app.h"

auto main() -> int {
    try {
        avetest::App app;
        if (!app.setup()) {
            return 1;
        }
        app.run();
    } catch (...) {
        return 1;
    }
    return 0;
}
