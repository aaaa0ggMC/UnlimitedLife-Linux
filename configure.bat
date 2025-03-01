@echo off

:: Install vcpkg if not already installed
if not exist "%VCPKG_ROOT%" (
    echo "vcpkg not found. Please install vcpkg and set the VCPKG_ROOT environment variable."
    exit /b 1
)

:: Install build dependencies using vcpkg
vcpkg install rapidjson glfw3 glew glm

:: Create a build directory if it doesn't exist
if not exist CBuild (
    mkdir CBuild
)
cd CBuild

:: Clone the toml++ repository and copy the toml++.hpp file
if not exist tomlplusplus (
    git clone https://github.com/marzer/tomlplusplus.git
)
copy tomlplusplus\toml.hpp %VCPKG_ROOT%\installed\x64-windows\include\toml.hpp

if not exist dep (
    mkdir dep
)

:: Run CMake to configure the project
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 16 2019"

:: Build the project
cmake --build . --config Release

if not exist Data (
    mkdir Data
)
cd Data

copy ..\Release\aaaa0ggmcLib-g4.dll .
xcopy ..\test_data\ .\test_data\ /E /I
copy ..\Release\UnlimitedLife.exe .
copy ..\Release\alib4test.exe .

:: Uncomment the following lines if you have equivalent tests on Windows
:: ctest --output-on-failure
:: cpack