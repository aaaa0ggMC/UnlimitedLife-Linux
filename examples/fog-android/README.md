# fog-android — alib6 移植到 Android 的第一步

`examples/fog-android` 是 UnlimitedLife 移动端迁移的**平台验证工程**（本阶段目标：
把 alib6（aaaa0ggmcLib）交叉编译到 Android 并对每个模块做综合探测）。

当前状态：**alib6 全量（75 个模块接口 + 16 个实现单元）已用 GCC-16 Android
交叉工具链编译通过**（arm64-v8a / armeabi-v7a / x86_64），App 以 JNI 展示各模块
探测结果，同内容打到 logcat（tag `AlibProbe`）。

## 前置

1. GCC-16 Android 交叉工具链（AmanoTeam/android-gcc-cross）
   - **必须**用每日构建产出的 **`x86_64-unknown-linux-gnu`** 宿主包
     （内含全部 Android target 编译器）；release 里按三元组命名的是
     **设备自托管**版本（bin 里是 Android 二进制，桌面跑不了）
   - 本仓库开发者固化于 `~/Files/Repos/android-gcc-toolchain/`
2. Android SDK（platform 36 / build-tools 35.0.0）
3. alib6 源码（`~/Propos/aaaa0ggmcLib` 或 sibling 路径）

`local.properties`（已 gitignore）：

```properties
sdk.dir=/opt/android-sdk
pino.dir=/home/<you>/Files/Repos/android-gcc-toolchain
alib.dir=/home/<you>/Propos/aaaa0ggmcLib
```

## 构建

```bash
# 原生（alib6 全量 + App，静态运行时；Gradle 的 preBuild 会自动做这步）
cd app/src/main/cpp
PINO_DIR=<tc> ALIB_DIR=<alib6> bash build-native.sh

# APK
cd examples/fog-android
./gradlew :app:assembleDebug
```

## 设计要点

- **模块拓扑编译**：`topo.py` 解析 alib6 的 `import` 图，按依赖序编译 75 个
  `.cppm`（BMI 进 `build/<abi>/work/gcm.cache`），再编译 `modules/alib6/**/*.cpp`
  实现单元，最后与 App 模块一起链接。Gradle 自定义任务替代 CMake/NDK。
- **全静态运行时**：`PINO_STATIC_RUNTIME=true`（工具链官方开关，等价 NDK 的
  `ANDROID_STL=c++_static`）+ `-static-libstdc++`。产物 NEEDED 只剩
  `liblog/libm/libc/libdl` 系统库。
- **TU 拆分铁律**（详见 `~/Files/Repos/AndroidJniGccModules` 的踩坑记录）：
  - `import std;` 的 TU 禁止 `#include` C++ 标准库头/平台 C 头
    （编译期重定义；`--allow-multiple-definition` 无效，那是链接期 flag）
  - 模块侧 `extern "C"` 出口不能 `inline`（不产生独立符号 → UnsatisfiedLinkError）
  - std 模块 BMI 需预构建且 `gcm.cache` 必须位于 cwd
  - 第三方 header（rapidjson/toml++）只符号链接单目录到 `deps/`，
    **绝不能 `-I` 整个 `/usr/include`**（宿主 glibc 头会污染 bionic 头）

## 探测覆盖（alib_probe.cppm）

`core.types / core.str(ext) / clock / core.io / core.storage / core.error /
log(+log.prefab Console) / ecs / data(AData) / table / perf`，每项
PASS/FAIL + 实测值；后续模块（window/render/shape）按移动端路线图继续接入。

## 已知事项

- `alib6::ext::to_string`（不是 `alib6::str::to_string`，后者只有 StringPool）
- `alib6::Clock`（无 `clock` 子命名空间）、`alib6::perf::Benchmark` 需传 lambda
- log/fastfmt 无泛型整数 `write_to_log` 重载（数字走 `std::format`）
- 设备上 APK 装在 `/data/app/~~.../base.apk`，Waydroid 无可见窗口时
  Activity 有 "wait for adding window timeout" 警告，不影响逻辑执行
