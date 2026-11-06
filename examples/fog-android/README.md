# fog-android — AVE Vulkan 雾场景

复用 `../fog` 的场景、计算着色器和 ImGui 编辑器，不复制一份渲染器。
Android **不包含、不链接 GLFW**：Activity 创建横屏 `SurfaceView`，AVE 通过
`ANativeWindow` 获取尺寸、创建 Vulkan Surface。保留 GCC-16、C++26 reflection、
传统头文件构建和静态 C++ 运行时方案。

Activity 固定为 `landscape`（非传感器横屏），创建 SurfaceView 前也显式请求横屏。
targetSdk 36 使用 `PROPERTY_COMPAT_ALLOW_RESTRICTED_RESIZABILITY` 退出大屏方向限制覆盖；
该兼容退出不适用于未来 targetSdk 37+，不能保证所有窗口模式下都能旋转设备显示。

渲染采用 SurfaceView 的实际像素尺寸和 identity swapchain preTransform，由系统合成器
处理显示旋转。不能直接把 `currentTransform` 当成“已经预旋转”，否则场景和 ImGui
会一起侧转。JNI 在原生线程启动前及尺寸变化时传递 SurfaceView 尺寸，避免把交换链
缓冲区的原生竖屏尺寸误当成窗口尺寸。`AVEWindow` 日志记录窗口/交换链尺寸与变换。

## 当前迁移状态

### ADB 可重复性能诊断（opt1baseline 起）

正常桌面图标仍进入启动设置。通过 `am start -S --user 0` 配合
`--ez fog_auto_start true` 可跳过设置页，但仍等待资源与 Surface 就绪。
仅在自动启动模式下接受以下诊断参数（重建 Activity 时保留）：

- `--ei fog_short_edge 720`：0/1080/720/540/360，非法值回退720。
- `--ei fog_froxel 0`：0..5，非法值回退0。
- `--ez fog_no_volume true`：真正跳过体积雾 compute pass 和主 pass 雾采样，不是只把密度设零。
- `--ez fog_no_lights true`：关闭局部点光；保留太阳与环境光供几何观察。
- `--ez fog_no_shadows true`：关闭太阳与点光阴影。
- `--ez fog_no_dragon true`：不加载 Dragon OBJ，也不创建相应实体。
- `--ez fog_no_ui true`：不构造或绘制每帧 ImGui。
- `--ez fog_local_mist true`：在 `setup()` 后生成一个控制点局部雾（最贵的形态）。

每两秒通过 `FogBenchmark` 日志输出窗口平均FPS/帧耗时与最近一次可读取的GPU分项；
GPU分项有交换链图像延迟，不应当成与该CPU采样窗口严格同步的平均值。
首次启动资源复制、模型加载以及阴影缓存预热应排除在稳态测量窗口外。
每轮先停止所有 `com.example.fogandroid.*` 应用，核对只存在目标包进程；
保持同分辨率/视角/档位，记录截图、设备温度与采样区间。

```bash
adb shell am start -S --user 0 \
  -n com.example.fogandroid.opt1baseline/com.example.fogandroid.MainActivity \
  --ez fog_auto_start true --ei fog_short_edge 720 --ei fog_froxel 0 \
  --ez fog_no_volume true --ez fog_no_lights true --ez fog_no_shadows true
adb logcat -s FogBenchmark:I
```

该入口是诊断对照工具，不代表已证明任何单项优化的性能收益。

- `resolution` 版本先显示 Android 原生启动设置，选好分辨率并点击“进入场景”后才加载
  原生渲染库、准备资源并创建渲染器。每次全新启动都会显示选择页，Activity 重建则恢复
  本次选择和已进入场景的状态。默认 720p，另提供原生、1080p、540p、360p；按屏幕比例
  计算实际宽高、不向上放大，重复尺寸会合并。
- 分辨率通过 UI 线程 `SurfaceHolder.setFixedSize` 设置真正的渲染缓冲区，保持 View 全屏；
  等 `surfaceChanged` 返回所选尺寸后才启动原生线程。Surface 重建继续应用当前选项，
  触摸坐标按 View 到缓冲区的比例换算；雾、阴影及其它质量参数不变。
- 启动设置新增“体积雾精度（Froxel 网格）”：六档 极低/很低/低/中/高/极高，与桌面端
  `app_ui.cpp` 的预设对齐并向下延伸。桌面当前默认即像素步长 8 / 深度 64 / 光源 4 步，
  实测性能刚刚好，故以它为“高”档基准；移动端 GPU 远弱于桌面，默认给最省算力的“极低”
  档（32px / 16 层），最高档与桌面最高预设齐平，不提供超过桌面的档位。选项标签按所选
  渲染分辨率实时显示实际网格与体素数（横纵向上限 192×128，与 `app_setup.cpp` 的 clamp
  一致），档间成本约 3 倍递进。选择随 `froxelLevel` 进 `onSaveInstanceState`，Activity
  重建后保持。
- 两个最低档把 `light_steps` 设为 0：`lighting.glsl` 的 `medium_visibility` 会整体跳过，
  局部光不再被介质自身衰减（可见差异：灯光穿透雾）。这比降网格更有效，选项标签与摘要
  行都会标注“关闭光衰减”，不做静默降质。
- 精度经 `nativeStart` 的三个整型参数传入，在原生线程 `App::setup()` 之前写入
  `App::volume`（`setup_ubo()` 据此分配 froxel/injection/history SSBO 并重建网格），
  以 `FogAndroid` 日志打印最终生效值；运行期仍可用界面内的体积雾面板继续调整。
- 界面内已有 GPU 时间戳分解（shadow / density+inject / temporal / integrate / scene+UI），
  若瓶颈不在体积雾这几项，调 Froxel 档位不会有明显收益。
- ImGui 字体/控件尺寸同步乘渲染缩放比例，避免低分辨率放大后界面巨大；不同分辨率
  不复用像素坐标布局文件，避免旧窗口位置/大小超出新的缓冲区。
- Surface 创建后在原生线程初始化 App；暂停或 Surface 销毁时请求退出、join，
  完成 GPU/UI/Surface 资源清理后释放 native window；恢复时重新创建 App。
- 触摸单指操作映射为 ImGui 鼠标事件，在渲染线程消费，支持编辑场景参数。
- Android 请求 Vulkan 1.1，关闭 validation，保留 dynamic rendering / legacy 自动回退。
- Gradle 打包 SPIR-V、法线贴图、scene、配置、模型、字体和 ice.png；运行前复制至
  App 私有目录，保留 fog 的相对路径加载和编辑器文件写入能力。
- ARM64 Debug APK 已完成 GCC-16 原生编译/链接、shader 编译及 Kotlin/Gradle 打包；
  必需资源、JNI 出口、系统库依赖、APK 签名和 ELF/ZIP 16 KiB 对齐检查通过。
  尚未安装到设备运行，也未构建其它 ABI；不能据此认定画面及生命周期已经跑通。
- 暂未实现移动端虚拟摇杆、软键盘及完整 WASD/鼠标锁定交互；相机可由编辑器调整。
  原生 panic/驱动崩溃仍需 logcat 排查，普通 C++ 异常会显示到状态栏。

## 本地依赖

1. 已安装的 AmanoTeam/android-gcc-cross GCC-16 **Linux 宿主包**，包含 Android 24+
   target 编译器（不是设备自托管包）；不使用 Clang/普通 NDK。
2. Android SDK platform 36 / build-tools 35.0.0、JDK 17、已有 Gradle 环境。
3. alib5 源码、当前引擎及独立的 fog 仓库。
4. 本地 Dear ImGui **1.92.9 docking 源码**（核心 `.cpp` 与 `backends/`，不是只有头文件
   或桌面静态库的安装目录）。Android/Vulkan 后端从该源码交叉编译。
5. rapidjson、toml++、GLM、Vulkan headers（含 `vk_video`）、STB、VMA 头文件；
   `glslangValidator` 用于构建时编译场景 shader。

ImGui 默认源码目录为 `.deps/imgui-1.92.9-docking/`（已 gitignore）；也可用
`imgui.dir` / `IMGUI_DIR` 覆盖。固定上游 tag `v1.92.9-docking`，对应 commit
`9b4eb24cee2071e61dc1f9ef3e5228097cdde720`，不使用浮动 docking 分支。
源码包：`https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.92.9-docking`，
SHA-256：`61f2ec7ef19702e47689b77cc602e3aeeda423941bfbc5d8ecb6a02d05a9dd73`。

`local.properties` 已被忽略，示例：

```properties
sdk.dir=/opt/android-sdk
pino.dir=/path/to/android-gcc-toolchain
alib.dir=/path/to/aaaa0ggmcLib
imgui.dir=/path/to/imgui-docking-source
vma.dir=/path/to/vma/include
```

其余可选键：`rapidjson.dir`、`tomlplus.dir`、`glm.dir`、`vulkan.dir`、`stb.dir`；
均指包含相应子目录的 include 根目录（例如 `stb.dir` 下有 `stb/stb_image.h`）。
脚本默认检查系统 `/usr/include` 和 sibling `_depend/headers`，但只将单个依赖
目录链接到隔离的 `deps/`，绝不把宿主 `/usr/include` 加入 Android include 搜索路径。
上述依赖还可通过同名大写环境变量指定，如 `IMGUI_DIR`、`VMA_DIR`、`TOMLPLUS_DIR`。

可选 `fog.dir`、`fog.assets.dir` 覆盖 fog 源码和引擎 assets 路径；默认使用本工作区。
所有属性相对路径均相对于 `app/`，shell 直接执行时相对路径相对于当前目录。
缺失依赖直接报错，不自动安装或下载。

## 构建（需要自行执行）

每次 APK 构建必须取一个总结名并传入 `-PfogBuildName=<name>`，无名或非法名字会报错。
名字使用小写字母开头，后续仅含小写字母、数字、下划线。项目规则见 `AGENTS.md`。

当前直接移植存档为 `directport`，应用 ID 为 `com.example.fogandroid.directport`，
显示名为 `directport-fogandroid`。不同名字的版本可以并存，App 私有目录互相隔离；
Kotlin package、namespace 与 JNI 类名保持 `com.example.fogandroid` 不变。
这次命名不改变画质、渲染策略或性能，不表示已排除真机泄漏。

启动分辨率选择版本使用 `resolution`，应用 ID `com.example.fogandroid.resolution`，
显示名 `resolution-fogandroid`；与 `directport` 并存，不覆盖基线 App 的私有数据。

在本目录运行：

```bash
# 首次先只构建所需 ABI；默认构建 arm64-v8a / armeabi-v7a / x86_64
FOG_ANDROID_ABIS=arm64-v8a ./gradlew -PfogBuildName=resolution :app:assembleDebug
# 分辨率/触摸坐标/Froxel 精度纯逻辑单测（无需设备）
./gradlew -PfogBuildName=resolution :app:testDebugUnitTest
```

`FOG_ANDROID_API` 默认 24；更高值同时提高 APK minSdk，避免 Java/native API 不匹配。
ABI filter 与原生选择同步，不会打包其它 ABI 遗留的旧探测库。
`preBuild` 调用 `build-native.sh` 和 `prepare-assets.sh`；头文件依赖、编译选项、
编译器变化参与增量判断。输出位于 `app/src/main/cpp/out/<abi>/libfogandroid.so`，
APK 位于 `app/build/outputs/apk/debug/app-debug.apk`，保留标准文件名以兼容既有下载地址。

手动运行原生脚本时，使用环境变量提供依赖（不会读取 `local.properties`）。
链接包含 `libandroid/libvulkan/liblog`，C++ 运行时由 `PINO_STATIC_RUNTIME=true`
静态链接，ELF 使用 16 KiB 最大页对齐。ARM64 已确认只依赖
`libandroid/libvulkan/liblog/libm/libc/libdl`；设备兼容性仍待实机验收。

## 待验收

APK 安装/启动、首次资源加载、Vulkan 1.1 legacy 与动态渲染、旋转/尺寸变化、
暂停恢复、Surface 重建、触摸编辑、异常提示、GPU 格式/内存限制及实际帧率。
日志过滤 tag：`FogAndroid`（宿主），底层 Vulkan/panic 另查完整 logcat。

`alib_probe.cpp` 保留作迁移历史参考，不再作为默认 Activity 或库入口。
