# cd .. 工作目录就是 ./
pacman -S --noconfirm --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glm mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-rapidjson mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-tomlplusplus mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-stb mingw-w64-ucrt-x86_64-ninja
# @todo IMGUI编译
mkdir -p CBuild_CACHE_WIN
mkdir -p CBuild/Windows
mkdir -p third_party
cd third_party
git clone https://github.com/ocornut/imgui.git
cd imgui
git branch docking
cp ../../scripts/imgui/CMakeLists.txt .
cp ./backends/imgui_impl_opengl3.cpp .
cp ./backends/imgui_impl_opengl3.h .
cp ./backends/imgui_impl_opengl3_loader.h .
cp ./backends/imgui_impl_glfw.cpp .
cp ./backends/imgui_impl_glfw.h .
mkdir -p abuild
cd abuild
cmake .. -G "Unix Makefiles"
make
mv libimgui.dll ../../../CBuild/Windows
mv libimgui.dll.a ../../../CBuild/Windows
#copy headers
mkdir -p ../../../CDep/headers/imgui/
cd ..
cp ./backends/imgui_impl_opengl3.h ./backends/imgui_impl_opengl3_loader.h ./backends/imgui_impl_glfw.h ./imconfig.h ./imgui.h ./imgui_internal.h ./imstb_rectpack.h ./imstb_textedit.h ./imstb_truetype.h ../../CDep/headers/imgui
cd ..
cd ..

cd CBuild_CACHE_WIN

cp /ucrt64/lib/libglfw3.dll.a /ucrt64
cp /ucrt64/lib/libglew32.dll.a /ucrt64

# del .dll.as this may cause link problems
# find /ucrt64/lib -name "*.dll.a" -delete

cp /ucrt64/libglfw3.dll.a /ucrt64/lib/
cp /ucrt64/libglew32.dll.a /ucrt64/lib/

# Run CMake to configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release -G "Ninja"






