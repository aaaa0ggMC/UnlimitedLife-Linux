#!/usr/bin/env bash
# fog-android 原生构建：GCC-16 Android 交叉工具链编译 alib6 全量 + App。
#
# 关键规则（来自 AndroidJniGccModules 的实测）：
#  1. import std; 的 TU 严禁 #include C++ 标准库/平台 C 头（编译期重定义冲突）
#  2. std 模块 BMI 需预构建，gcm.cache 位于 cwd
#  3. C 出口函数不能 inline；链接加 -static-libstdc++（std 模块引用
#     libstdc++.so 未导出的无版本整数 to_chars 重载）
#  4. PINO_STATIC_RUNTIME=true：工具链官方的"static 一切"开关
#     （等价 NDK 的 ANDROID_STL=c++_static）
#  5. 第三方 header（rapidjson）只符号链接单目录暴露，绝不能 -I 整个
#     /usr/include——宿主 glibc 头会污染 bionic 头（__BIONIC_AVAILABILITY_GUARD）
set -euo pipefail

CPP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PINO_DIR="${PINO_DIR:-/opt/android-gcc-cross}"
ALIB_DIR="${ALIB_DIR:-../../../aaaa0ggmcLib}"
ALIB_INC="$ALIB_DIR/include"
API="${FOG_ANDROID_API:-21}"

if [[ ! -x "$PINO_DIR/bin/aarch64-unknown-linux-android${API}-g++" ]]; then
    echo "error: GCC toolchain not found at $PINO_DIR (API $API)" >&2
    exit 1
fi
if [[ ! -d "$ALIB_INC/alib6" ]]; then
    echo "error: aaaa0ggmcLib sources not found at $ALIB_DIR" >&2
    exit 1
fi

# rapidjson（header-only）：优先 local.properties 的 rapidjson.dir，
# 其次系统 /usr/include，最后仓库内的 _depend/headers。
RAPIDJSON_SRC="${RAPIDJSON_DIR:-}"
if [[ -z "$RAPIDJSON_SRC" ]]; then
    if [[ -f /usr/include/rapidjson/rapidjson.h ]]; then
        RAPIDJSON_SRC=/usr/include
    elif [[ -f "$ALIB_DIR/../_depend/headers/rapidjson/rapidjson.h" ]]; then
        RAPIDJSON_SRC="$ALIB_DIR/../_depend/headers"
    fi
fi

# toml++（header-only，toml.cpp 需要）
TOMLPLUS_SRC="${TOMLPLUS_DIR:-}"
if [[ -z "$TOMLPLUS_SRC" ]]; then
    if [[ -f /usr/include/toml++/toml.hpp ]]; then
        TOMLPLUS_SRC=/usr/include
    elif [[ -f "$ALIB_DIR/../_depend/headers/toml++/toml.hpp" ]]; then
        TOMLPLUS_SRC="$ALIB_DIR/../_depend/headers"
    fi
fi

# 注意不加引号：让默认值/环境变量按空格分词成多个 ABI
declare -a ABIS=(${FOG_ANDROID_ABIS:-arm64-v8a armeabi-v7a x86_64})
declare -A PREFIX=(
    [arm64-v8a]="aarch64-unknown-linux-android"
    [armeabi-v7a]="armv7-unknown-linux-androideabi"
    [x86_64]="x86_64-unknown-linux-android"
)

for abi in "${ABIS[@]}"; do
    GXX="$PINO_DIR/bin/${PREFIX[$abi]}${API}-g++"
    [[ -x "$GXX" ]] || { echo "skip $abi: $GXX not found"; continue; }

    work="$CPP_DIR/build/$abi/work"
    mkdir -p "$work" "$CPP_DIR/out/$abi"
    cd "$work" # gcm.cache 固定在 cwd

    FLAGS=(-std=c++26 -fmodules -freflection -fPIC -O2 -I"$ALIB_INC")
    # 只把第三方 header 单目录链接进隔离 deps/，不污染整个 include 搜索路径
    # （绝不能 -I 整个 /usr/include：宿主 glibc 头会污染 bionic 头）
    mkdir -p "$work/deps"
    [[ -n "$RAPIDJSON_SRC" ]] && ln -sfn "$RAPIDJSON_SRC/rapidjson" "$work/deps/rapidjson"
    [[ -n "$TOMLPLUS_SRC" ]] && ln -sfn "$TOMLPLUS_SRC/toml++" "$work/deps/toml++"
    if compgen -G "$work/deps/*" > /dev/null; then
        FLAGS+=(-I"$work/deps")
    fi

    echo "########## $abi ##########"

    echo "== std module BMI =="
    "$GXX" "${FLAGS[@]}" -fmodule-only -c -x c++ \
        "$PINO_DIR/include/c++/16/bits/std.cc" -o std.o

    echo "== alib6 module interfaces (topo order) =="
    python3 "$CPP_DIR/topo.py" "$ALIB_INC/alib6" > modules.order
    echo "   $(wc -l < modules.order) units"
    while read -r src; do
        rel="${src#"$ALIB_INC"/}"
        obj="$(echo "$rel" | tr '/' '_' | sed 's/\.cppm$/.o/')"
        if [[ ! -f "$obj" || "$src" -nt "$obj" ]]; then
            if ! "$GXX" "${FLAGS[@]}" -c "$src" -o "$obj" 2> "$obj.err"; then
                echo "!! FAILED: $rel" >&2
                head -25 "$obj.err" >&2
                exit 1
            fi
        fi
    done < modules.order

    echo "== alib6 implementation units =="
    shopt -s globstar nullglob
    for src in "$ALIB_DIR"/modules/alib6/**/*.cpp; do
        rel="impl_$(echo "${src#"$ALIB_DIR/modules/alib6/"}" | tr '/' '_' | sed 's/\.cpp$/.o/')"
        if [[ ! -f "$rel" || "$src" -nt "$rel" ]]; then
            if ! "$GXX" "${FLAGS[@]}" -c "$src" -o "$rel" 2> "$rel.err"; then
                echo "!! FAILED: ${src#"$ALIB_DIR"/}" >&2
                head -25 "$rel.err" >&2
                exit 1
            fi
        fi
    done

    echo "== app modules =="
    "$GXX" "${FLAGS[@]}" -c "$CPP_DIR/alib_probe.cppm" -o alib_probe.o
    "$GXX" "${FLAGS[@]}" -c "$CPP_DIR/jni_bridge.cpp" -o jni_bridge.o

    echo "== link libfogandroid.so (PINO_STATIC_RUNTIME=true) =="
    # 全静态运行时：libstdc++/libgcc 均静态进 .so（工具链 README 推荐用法）
    PINO_STATIC_RUNTIME=true "$GXX" -shared "${FLAGS[@]}" \
        *.o -llog -o "$CPP_DIR/out/$abi/libfogandroid.so"

    echo "-> out/$abi/libfogandroid.so ($(du -h "$CPP_DIR/out/$abi/libfogandroid.so" | cut -f1))"
    readelf -d "$CPP_DIR/out/$abi/libfogandroid.so" | grep NEEDED | sed 's/^/   /'
done

echo "fog-android native build done."
