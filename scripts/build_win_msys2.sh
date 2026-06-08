# cd .. 工作目录就是 ./
mkdir -p CBuild_CACHE_WIN
cd CBuild_CACHE_WIN

ninja

# Move Artifact
mv ../CBuild/Windows/* ../CBuild/
rmdir ../CBuild/Windows/

#mov deps
cp /ucrt64/bin/glfw3.dll ../CBuild/
cp /ucrt64/bin/glew32.dll ../CBuild/
cp /ucrt64/bin/libwinpthread-1.dll ../CBuild/
cp /ucrt64/bin/libgcc_s_seh-1.dll ../CBuild/