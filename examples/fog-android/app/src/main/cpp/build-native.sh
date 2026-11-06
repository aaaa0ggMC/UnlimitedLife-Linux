#!/usr/bin/env bash
# GCC-16 / C++26 reflection, traditional headers; no GLFW, modules or NDK downloads.
set -euo pipefail
CPP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$CPP_DIR/../../../../../.." && pwd)"
abspath() { realpath -m "$1"; }
PINO_DIR="$(abspath "${PINO_DIR:-/opt/android-gcc-cross}")"
ALIB_DIR="$(abspath "${ALIB_DIR:-$ROOT_DIR/../aaaa0ggmcLib}")"
FOG_DIR="$(abspath "${FOG_DIR:-$ROOT_DIR/examples/fog}")"
API="${FOG_ANDROID_API:-24}"
[[ "$API" =~ ^[0-9]+$ && "$API" -ge 24 ]] || { echo 'error: Vulkan requires FOG_ANDROID_API >= 24' >&2; exit 1; }
[[ -f "$ALIB_DIR/include/alib5/autil.h" && -f "$FOG_DIR/app.h" ]] || {
    echo 'error: ALIB_DIR and FOG_DIR must point to the alib5 and fog sources' >&2; exit 1;
}

# Headers are linked individually into an isolated directory: never -I/usr/include.
find_include() {
    local override="$1" header="$2" candidate
    for candidate in "$override" /usr/include "$ALIB_DIR/../_depend/headers"; do
        if [[ -n "$candidate" && -f "$candidate/$header" ]]; then
            abspath "$candidate"; return
        fi
    done
    echo "error: missing $header; specify its include root in local.properties/environment" >&2
    return 1
}
RAPIDJSON_SRC="$(find_include "${RAPIDJSON_DIR:-}" rapidjson/rapidjson.h)"
TOMLPLUS_SRC="$(find_include "${TOMLPLUS_DIR:-}" toml++/toml.hpp)"
GLM_SRC="$(find_include "${GLM_DIR:-}" glm/glm.hpp)"
VULKAN_SRC="$(find_include "${VULKAN_DIR:-}" vulkan/vulkan.h)"
STB_SRC="$(find_include "${STB_DIR:-}" stb/stb_image.h)"
VMA_SRC="$(find_include "${VMA_DIR:-}" vk_mem_alloc.h)"
IMGUI_DIR="$(abspath "${IMGUI_DIR:-$ROOT_DIR/examples/fog-android/.deps/imgui-1.92.9-docking}")"
for src in imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp backends/imgui_impl_android.cpp backends/imgui_impl_vulkan.cpp; do
    [[ -f "$IMGUI_DIR/$src" ]] || { echo "error: missing $IMGUI_DIR/$src; set imgui.dir to Dear ImGui docking source (1.92.9 compatible)" >&2; exit 1; }
done

read -r -a ABIS <<< "${FOG_ANDROID_ABIS:-arm64-v8a armeabi-v7a x86_64}"
(( ${#ABIS[@]} )) || { echo 'error: no Android ABI selected' >&2; exit 1; }
declare -A PREFIX=([arm64-v8a]=aarch64-unknown-linux-android [armeabi-v7a]=armv7-unknown-linux-androideabi [x86_64]=x86_64-unknown-linux-android)
shopt -s globstar nullglob
for abi in "${ABIS[@]}"; do
    [[ -n "${PREFIX[$abi]:-}" ]] || { echo "error: unsupported ABI $abi" >&2; exit 1; }
    GXX="$PINO_DIR/bin/${PREFIX[$abi]}${API}-g++"
    [[ -x "$GXX" ]] || { echo "error: compiler unavailable: $GXX" >&2; exit 1; }
    work="$CPP_DIR/build/$abi/fog"
    mkdir -p "$work/deps" "$CPP_DIR/out/$abi"
    ln -sfn "$RAPIDJSON_SRC/rapidjson" "$work/deps/rapidjson"
    ln -sfn "$TOMLPLUS_SRC/toml++" "$work/deps/toml++"
    ln -sfn "$GLM_SRC/glm" "$work/deps/glm"
    ln -sfn "$VULKAN_SRC/vulkan" "$work/deps/vulkan"
    # Recent Vulkan headers include sibling vk_video headers.
    [[ ! -d "$VULKAN_SRC/vk_video" ]] || ln -sfn "$VULKAN_SRC/vk_video" "$work/deps/vk_video"
    ln -sfn "$STB_SRC/stb" "$work/deps/stb"
    ln -sfn "$VMA_SRC/vk_mem_alloc.h" "$work/deps/vk_mem_alloc.h"
    FLAGS=(-std=c++26 -freflection -fPIC -O2 -pthread
        -DALIB5_ENABLE_REFLECTION -DGLM_ENABLE_EXPERIMENTAL
        -DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DVK_USE_PLATFORM_ANDROID_KHR
        -I"$ALIB_DIR/include" -I"$ROOT_DIR/include" -I"$FOG_DIR"
        -I"$work/deps" -I"$IMGUI_DIR" -I"$IMGUI_DIR/backends")
    FLAGS_SIG="$work/.flags.sig"
    sig="$(printf '%s\n' "$GXX" "$(stat -Lc '%Y:%s' "$GXX")" "${FLAGS[@]}" \
        "$RAPIDJSON_SRC" "$TOMLPLUS_SRC" "$GLM_SRC" "$VULKAN_SRC" "$STB_SRC" "$VMA_SRC")"
    if [[ ! -f "$FLAGS_SIG" || "$(cat "$FLAGS_SIG")" != "$sig" ]]; then
        printf '%s' "$sig" > "$FLAGS_SIG"
    fi
    OBJS=()
    compile_unit() {
        local src="$1" name obj rebuild=false dep joined
        name="$(printf '%s' "$src" | sha256sum | cut -c1-16)"
        obj="$work/$(basename "${src%.cpp}")_$name.o"
        if [[ ! -f "$obj" || ! -f "$obj.d" || "$src" -nt "$obj" || "$FLAGS_SIG" -nt "$obj" ]]; then
            rebuild=true
        else
            joined="$(sed -e ':a' -e 'N' -e '$!ba' -e 's/\\\n/ /g' "$obj.d" | sed 's/^[^:]*://')"
            # GCC depfile escaping assumes dependency paths without whitespace.
            for dep in $joined; do
                if [[ ! -f "$dep" || "$dep" -nt "$obj" ]]; then rebuild=true; break; fi
            done
        fi
        if $rebuild; then
            echo "  CC $src"
            "$GXX" "${FLAGS[@]}" -MD -MF "$obj.d" -c "$src" -o "$obj.tmp"
            mv "$obj.tmp" "$obj"
        fi
        OBJS+=("$obj")
    }
    echo "== $abi: alib5 + AVE Android + fog =="
    for src in "$ALIB_DIR"/modules/alib5/**/*.cpp "$ROOT_DIR"/modules/AVE/**/*.cpp; do compile_unit "$src"; done
    for src in "$FOG_DIR"/app*.cpp; do
        [[ "$(basename "$src")" == app_radiance_check.cpp ]] || compile_unit "$src"
    done
    compile_unit "$FOG_DIR/imgui_impl.cpp"
    compile_unit "$CPP_DIR/jni_bridge.cpp"
    for src in imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp backends/imgui_impl_android.cpp backends/imgui_impl_vulkan.cpp; do
        compile_unit "$IMGUI_DIR/$src"
    done
    PINO_STATIC_RUNTIME=true "$GXX" -shared -pthread -Wl,--no-undefined -Wl,-z,max-page-size=16384 \
        "${OBJS[@]}" -lstdc++exp -landroid -lvulkan -llog -o "$CPP_DIR/out/$abi/libfogandroid.so.tmp"
    mv "$CPP_DIR/out/$abi/libfogandroid.so.tmp" "$CPP_DIR/out/$abi/libfogandroid.so"
done
