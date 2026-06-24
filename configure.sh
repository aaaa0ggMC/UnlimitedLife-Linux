#!/bin/bash

OS="$(uname -s)"

echo "Detecting operating system for AGE Engine..."

case "${OS}" in
    Linux*)
        echo "Arch Linux environment detected."
        if command -v pacman > /dev/null; then
            echo "Installing dependencies via pacman for Arch Linux..."
            sudo pacman -S --needed glfw-wayland glew glm stb
        else
            echo "Error: pacman not found. Make sure you are on Arch Linux."
            exit 1
        fi
        ;;
        
    MINGW*|MSYS*|UCRT64*)
        echo "MSYS2 (Windows) environment detected."
        # 更新数据库 (非常重要，防止出现 404 错误)
        echo "Updating MSYS2 package database..."
        pacman -Sy
        
        echo "Installing UCRT64 dependencies for OpenGL/GLFW via pacman..."
        pacman -S --needed mingw-w64-ucrt-x86_64-glfw \
                           mingw-w64-ucrt-x86_64-glew \
                           mingw-w64-ucrt-x86_64-glm \
                           mingw-w64-ucrt-x86_64-stb
        ;;
        
    *)
        echo "Unsupported operating system: ${OS}"
        exit 1
        ;;
esac

echo "======================================"
echo "Configuration Successful!"
echo "Next steps in your UCRT64 terminal (for MSYS2 users):"
echo "  1. Clean & Config: xmake f -p mingw -c"
echo "  2. Build:          xmake"
echo "======================================"