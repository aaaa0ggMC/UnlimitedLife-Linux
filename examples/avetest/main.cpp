#include "app.h"

auto main() -> int {
    avetest::App app;
    app.setup();
    app.run();
    return 0;
}
