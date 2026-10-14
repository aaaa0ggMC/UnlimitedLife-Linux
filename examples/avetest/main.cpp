#include "app.h"

auto main(int argc, char** argv) -> int {
    if(argc == 2 && std::string_view(argv[1]) == "--compute-test") {
        avetest::test_compute_pipeline();
        return 0;
    }
    avetest::App app;
    app.setup();
    app.run();
    return 0;
}
