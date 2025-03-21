pacman -S --noconfirm --needed mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-rapidjson mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-tomlplusplus mingw-w64-ucrt-x86_64-cmake

mkdir -p CBuild_CACHE_WIN
cd CBuild_CACHE_WIN

# Run CMake to configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"

make aaaa0ggmcLib-g4
