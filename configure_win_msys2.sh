pacman -S --noconfirm --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-rapidjson mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-tomlplusplus mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make
mkdir -p CBuild_CACHE_WIN
cd CBuild_CACHE_WIN

cp /ucrt64/lib/libglfw3.dll.a /ucrt64
cp /ucrt64/lib/libglew32.dll.a /ucrt64

# del .dll.as
find /ucrt64/lib -name "*.dll.a" -delete

cp /ucrt64/libglfw3.dll.a /ucrt64/lib/
cp /ucrt64/libglew32.dll.a /ucrt64/lib/

# Run CMake to configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"






