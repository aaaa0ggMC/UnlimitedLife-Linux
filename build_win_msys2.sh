mkdir -p CBuild_CACHE_WIN
cd CBuild_CACHE_WIN

make

# Move Artifact
mv ../CBuild/Windows/* ../CBuild/
rmdir ,,/CBuild/Windows/
