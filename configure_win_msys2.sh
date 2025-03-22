pacman -S --noconfirm --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-rapidjson mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-tomlplusplus mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make
mkdir -p CBuild_CACHE_WIN
cd CBuild_CACHE_WIN

# del .dll.as
find /ucrt64/lib -name "*.dll.a" -delete

# Run CMake to configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"






