# fog 改动审计说明（第二轮：ImGui 界面接入、雾的时间项、校验与生命周期修正）

> 第一轮的说明（初始场景、相机/光照、实例化与 CPU 提交优化）见同目录 `audit.md`；
> **本文件是独立文档，只描述本轮改动**，与 `audit.md` 不重叠。
> 注意 `audit.md` 里的文件表与 UBO 布局是历史快照（那时还没有 `fog_extra` / `fog_anim`），
> **本轮的行号、文件内容与 GPU 布局以本文件为准**。
>
> 本轮范围 = fog 子仓库 12 个提交（`32c9be1` ... `ae6d6c0`）+ 根仓库 2 个提交（`60343ff`、`b0ab9e8`）。
> fog 是独立仓库（`git -C examples/fog ...`），与根仓库分别提交；`alib6` 本轮没有改动。

## 1. 提交清单

### fog 子仓库（16 个功能提交，`git -C examples/fog log --oneline 32c9be1~1..HEAD`）

> 该范围里另有几个只改 `audited/*.md` 的提交（审计文档本身），不列在下表。

| 提交 | 内容 |
| --- | --- |
| `32c9be1` | 密度场拆成「全空间基础项 sigma_base + 近地面附加项」，天空不再采噪声 |
| `b233adc` | 合成统一成雾色 blend，去掉天空 `max(雾色, 天空色)` 特例 |
| `84b3503` | 撤掉 sigma_base 改层厚，加运行时调参键（后来被界面取代） |
| `d6ac51d` | 接缝修在结构上：地面雾 in-scatter 与天空共用 `atmosphere(dir)` |
| `d74b4b4` | 主项改为全空间体积密度（俯视/侧视一致），天空仍为背景 |
| `49e98db` | 接入 ImGui：Controller + ECS Inspector，去掉调参日志与按键 |
| `c8973eb` | 菜单式单窗口 + 1.25 倍缩放（可实时调）；只有 M 锁定后才允许移动视野 |
| `85dbd85` | 修 ImGui 接入 dynamic rendering：pipeline 补 depth/stencil；退出关后端 |
| `f9dde3d` | 锁定期间持续重设光标（Wayland 下 M 锁定后光标不消失） |
| `70bb372` | 退出时先等 GPU 空闲再拆 ImGui 资源 |
| `8c649a6` | 雾加时间项：整片雾按周期浓淡变化（Left/Right 调时间缩放） |
| `ae6d6c0` | 时间项改成可调周期 + 无理频率比，避免一眼看出循环 |
| `ef730a0` | 界面：面板改为在菜单栏上直接平铺（去掉「面板」子菜单）；统计面板补 `Left / Right` 键位说明 |
| `d603ef3` | 界面：加窗口背景 / 控件两个不透明度旋钮（统计面板「界面」段） |
| `f0dfc9b` | 界面：字段登记改成声明式花括号表 + 每组件一个 `register_*`（纯重构） |
| `5652cc3` | 雾：理论移入 `shaders/fog_common.md`（从 `A = epsilon*c*l`、`A = log10(I0/I)` 推起），GLSL 只留代码与一句话注释，成员注释统一上置 `///`，符号 ASCII 化 |

### 根仓库（2 个）

| 提交 | 内容 |
| --- | --- |
| `60343ff` | xmake：imgui 增加 `vulkan=true`（agetest 的 `opengl3` 保留）；fog 目标 `add_packages("imgui")` |
| `b0ab9e8` | AVE 光标：透明光标兜底 + `Window::reapply_cursor_mode()`，`cursor.cppm` 文档同步 |

## 2. 改动文件一览

### fog 子仓库

| 文件 | 状态 | 改动要点 | 审计关注点 |
| --- | --- | --- | --- |
| `app_ui.cpp` | **新增** | 模块侧接线：把 ECS 组件登记成 `ui::Field/Entity` 描述表并驱动后端；`App::setup_imgui/ui_frame/render_ui/camera_active`、`~App` | 字段用花括号表声明（`add_floats/set_color/add_bool/add_int/add_note`），每个组件一份 `register_*`，`ui_frame` 的实体循环里一行一个组件；**新增字段＝在对应 `register_*` 里加一行 `{标签, 目标, 步长, 下限, 上限}`**；`value` 直接指向组件真实成员，改完即生效 |
| `imgui_impl.cpp` | **新增 336 行** | 经典侧：GLFW+Vulkan 后端初始化、菜单式单窗口、按描述表渲染控件 | 见第 3 节；`init` 用三步就绪标志；depth/stencil 由 `InitDesc` 传入 |
| `ui_bridge.h` | **新增 113 行** | 纯 POD 桥（`Kind/Field/Entity/Frame/InitDesc` + 6 个函数），不依赖 std/imgui/vulkan | 两界唯一接口；`backend_shutdown` 的前提（设备有效 + GPU 空闲）写在头注释里 |
| `app.h` | 改 | `CameraFogUbo` 加 `fog_anim`（整块 240 B）；`AppConfig::Fog` 加动画三项；新增 `AppConfig::Ui`（font/scale）；App 加 `ui_visible`/`instances_dirty`/`fog_phase`、4 个声明与 `~App` | UBO 字段顺序必须与 `fog_common.glsl` 一致；`anim_period` 默认 60 s |
| `app_systems.cpp` | 改 | `update_camera`：以 `camera_active()` 为门槛 + 锁定期间 `reapply_cursor_mode()`；`update_environment`：`Left / Right` 调周期、相位累加；`render_scene`：脏实例重传、UBO 填 `fog_anim`、pass 内录制界面 | `Left / Right` 只改周期不动相位；`instances_dirty` 在任何 GPU 录制之前处理 |
| `app_scene.cpp` | 改 | 新增 `App::refresh_instances()`：Inspector 改了实例部件后重传，不重建设备缓冲 | 批大小必须等于 `instance_count`，否则 `panicf` |
| `app_setup.cpp` | 改 | `setup()` 末尾 `setup_imgui()`；swapchain 重建回调同步 ImGui 的 MinImageCount | shader/spv 路径未变 |
| `app_run.cpp` | 改 | 每渲染帧 `ui_frame()`（在 `render_scene()` 之前）；操作提示补 F1 与"需锁定才能移动" | 界面必须每帧 build（draw data 与录制 1:1） |
| `app_comp.h` | 改 | 相机移动/倾斜 API 收拢（`camera_move_delta`、`CameraTilt` 字段） | 与 `tests/camera_test.cc` 断言一致 |
| `shaders/fog_common.glsl` | 改 +203 | 密度场、闭式高度积分、`atmosphere_color`、`fog_anim_factor`（时间项） | 见第 4、5 节；天空一律传 `apply_noise=false`；**理论推导与近似取舍在 `shaders/fog_common.md`**（用 `$$` 排版，符号 ASCII），本文件只留代码与一两行注释 |
| `shaders/fog.frag`、`sky.frag`、`fog.vert`、`fog_instanced.vert`、`sky.vert` | 改 | 同步新 UBO 与新模型；天空改为直接输出 `atmosphere(dir)` | 天空 pass 不参与 tau |
| `build_shaders.sh` | 改 | `glslangValidator -I<shaders>`，输出 `assets/fog/shaders/*.spv` | 5 个 shader 全编译 |
| `README.md` | 改 +73 | 雾公式（含时间项）、键位、界面说明 | |
| `config.example.json` | 改 | 增 `fog.anim_period/anim_amplitude/anim_frequency` 与 `ui` 段 | 本地 `assets/config.fog.json` 同步 |

### 根仓库（AVE / xmake）

| 文件 | 改动要点 | 审计关注点 |
| --- | --- | --- |
| `xmake/packages.lua` | imgui 的 configs 加 `vulkan = true` | 配方里正确选项名是 `vulkan`（`glfw_vulkan` 已废弃）；只影响示例构建 |
| `xmake/examples.lua` | fog 目标 `add_packages("imgui")` | xmake 只在 target 依赖时才按项目 config 装包 |
| `include/AVE/window/window.cppm` | `ensure_empty_cursor()`、`apply_cursor_mode()`、`reapply_cursor_mode()`、`destroy()` 释放透明光标 | 见第 6 节；`GLFWcursor*` 的生命周期挂在 Window 上 |
| `include/AVE/window/cursor.cppm` | 模块文档补充隐藏机制 | 无逻辑改动 |

## 3. ImGui 接入：为什么"经典侧 / 模块侧 + 纯 POD 桥"

GCC 下 AVE 模块会把 libc/GLFW 声明带进模块，**和 imgui 的经典头不能在同一个 TU 混用**。用最小探针确认的边界：

| TU 内容 | 结果 |
| --- | --- |
| `#include <vulkan/vulkan.h>` + `import ave` | OK（与 `tests/ave_buffer_test.cpp` 一致） |
| `import ave` + `#include <imgui.h>` | (fail) `struct __locale_struct` 重定义（报 "of module ave"） |
| `import ave` + `#define GLFW_INCLUDE_VULKAN` + `<GLFW/glfw3.h>` | (fail) `struct GLFWwindow` 冲突 |

于是拆成两侧，中间只传纯 POD（`ui_bridge.h` 顶部写了原因）：

- **经典侧 `imgui_impl.cpp`**：include `imgui.h`/`imgui_impl_glfw.h`/`imgui_impl_vulkan.h`，**不** import `std`/`ave`；负责 ImGui 上下文、两个后端、把 `Frame` 里的字段画成控件、起帧与录制。
- **模块侧 `app_ui.cpp`**：import `std`/`ave`，只 include `ui_bridge.h`；遍历 ECS 填 `Field`（`value` 指向组件成员）与 `Entity` 表。
- 代价：检查器字段是**手工登记**的；换来的是两侧编译互不干扰、且界面逻辑不依赖 AVE 内部类型。

后端的 Vulkan 初始化要点（`backend_init`）：`ImGui_ImplGlfw_InitForVulkan(handle, true)`（链式接管 GLFW 回调，AVE 原有回调继续工作）、`DescriptorPoolSize = 32` 让后端自建描述符池、`MinImageCount = 2` / `ImageCount = swapchain 数`、dynamic rendering 用 `PipelineRenderingCreateInfo`（颜色 + depth/stencil），legacy 模式改用 `RenderPass`。`io.ConfigFlags` 打开键盘导航与 docking、**关掉多视口**（否则 ImGui 会另开系统窗口）。

界面形态：**单窗口 + 菜单栏平铺**（统计 / 雾 / 光照 / 天空 / 检查器 五个 `MenuItem` 直接排在菜单栏上，不套「面板」子菜单，当前面板高亮），按 `Field::group` 过滤到各面板；统计面板里有全部快捷键说明，以及三个即时生效的界面旋钮：**界面缩放**（基于未缩放的 style 副本重算 + `FontGlobalScale` 补偿，反复拖不累积）、**窗口背景不透明度**（`SetNextWindowBgAlpha`，初始化时取 ImGui 默认 `WindowBg.a = 0.94`，调到 0 只剩控件、场景完全透出）、**控件不透明度**（`PushStyleVar(ImGuiStyleVar_Alpha)` 覆盖本帧绘制、帧末 `Pop`，下限 0.2 以免整块界面淡到无法操作）。三者都只在本会话内生效，重启回默认。`ui.scale`（默认 1.25）在初始化时作用于字体与 `ScaleAllSizes`。

## 4. 雾的密度场与时间项（`fog_common.glsl`）

不加时间项时，密度场是：

    sigma(p) = sigma_base + sigma_mist * exp(-k * max(p.y - ground, 0)) * noise(p.xz)

`tau = int sigma ds` 沿视线解析求：基础项按距离线性累加，近地面项的高度部分用 `fog_height_depth()` 闭式积分（以 `y = ground` 分两段），噪声按 `1/(1+span*noise_scale)` 衰减；天空（`apply_noise=false`）不采噪声，避免透视下出格子硬边。

本轮新增的时间项：

    sigma(p, t) = anim(p, phi) * [ sigma_base + sigma_mist * exp(-k*max(p.y-ground,0)) * noise(p.xz) ]
    anim(p, phi) = 1 + amp * [ 0.55*breath(phi) + 0.45*wave(p, phi) ]

- `breath(phi) = 0.50*sin(phi) + 0.30*sin(sqrt(phi)*phi + 1.7) + 0.20*sin(phi*phi + 0.4)`：频率比 **1 : sqrt(phi) : phi**（无理、彼此接近），振幅也接近 -> 拍频在分钟量级、理论无公共周期；
- `wave` 是 xz 平面对行波，相位里再套一层慢波：`sin(dot(p.xz,k)*2*pi*f - sqrt(phi)*phi + 0.45*sin(dot(p.xz,k.yx)*0.59*2*pi*f + 1.09*phi))`；
- phi 由 CPU 按 `2pi / anim_period` 累加（默认 60 s，范围 5..900 s，`Left / Right` 每次 *0.8/*1.25），所以改周期**不跳相位**；
- 实现上只在 `fog_transmittance()` 末尾乘一次：`depth *= fog_anim_factor(mid, apply_noise)` -- 高度项的闭式积分仍然精确，没有退化成步进；
- 天空传 `apply_noise=false`，只保留与位置无关的 `breath`（行波在长射线上等于取均值，单点采样会出硬边）；天空 pass 本身也不吃 tau。

**审计关注点**：幅度默认 0.35，此时远处几何与天空的融合程度会随周期轻微起伏（随距离渐变的柔和过渡，不是硬边）；嫌明显就调小幅度或加大 sigma_base/end。

## 5. dynamic rendering 的 depth/stencil 与退出生命周期

- **depth/stencil**：ImGui 的 pipeline 建在 `PipelineRenderingCreateInfo` 里，之前只填了颜色 -> 与场景 dynamic rendering 的 `D24_UNORM_S8_UINT` 附件不符，每帧 `vkCmdDrawIndexed` 触发 `VUID-...-dynamicRenderingUnusedAttachments-08914/08917`。现在从 AVE 现成的
  `DynamicRender::get_depth_attachment_format()` / `get_stencil_attachment_format()` 取值填进 `InitDesc`（legacy render pass 模式留 `VK_FORMAT_UNDEFINED`，格式由 render pass 决定）。
- **退出**：`~App()` 先 `renderer->wait_idle()` 再 `fog::ui::backend_shutdown()`。顺序依据是本项目约定"销毁不隐式 `vkDeviceWaitIdle`，调用方保证 GPU 已不再使用该资源"（见 AVE `buffer.cppm`）；析构函数体先于成员析构执行，所以此时 device/window 仍有效。不等空闲直接释放会报 `vkDestroyBuffer` / `vkFreeDescriptorSets` / `vkDestroySampler` / `vkDestroyPipeline` 的"still in use"。
- `backend_shutdown()` 用 `g_context_ready`/`g_glfw_ready`/`g_vulkan_ready` 三步标志：Init 中途失败也能按已完成步骤回滚、可重复调用；所有后端入口在未就绪时直接返回。

## 6. 光标隐藏（M 锁定）

- 现实现：GLFW 只在**切换模式那一次**写隐藏（Wayland 上 `wl_pointer_set_cursor(..., NULL, ...)`），而它的 `lockedPointerHandleLocked`/`Unlocked` 监听器是**空实现** -> 合成器激活指针锁后可能把光标画回来（KDE/KWin 实测；agetest 用同样的"设一次"方式，所以是同款问题；那张透明光标在 Wayland 下也帮不上忙，因为 `HIDDEN/DISABLED` 分支会忽略光标图像）。
- AVE 侧：`Hidden`/`Disabled` 除设置 GLFW 输入模式外，再挂一张 1x1 全透明光标（`ensure_empty_cursor()`，懒创建、`destroy()` 释放；AGE 的 `Cursor::emptyCursor` 同款），并新增 `Window::reapply_cursor_mode()` 供调用方持续重设。fog 在 `camera_active()` 时按 tick（120 Hz）调用它。
- `App::camera_active()` 只看 `CursorMode::Disabled`：锁定后系统不显示光标、界面也无法操作，所以不再叠加 ImGui 的占用判断。

## 7. GPU / CPU 布局与命令流（本轮）

| 数据 | 布局（字节） |
| --- | --- |
| `CameraFogUbo` | **240**：view 0、proj 64、fog_color 128、fog_params 144、camera_position 160、light_direction 176、light_color 192、fog_extra 208、fog_anim 224 |
| `PushConstant` | 96：model 0、params 64、base_color 80 |
| `SkyPushConstant` | 16：exposure、textured、两个未用槽 |

`fog_params` = (start, end, sigma_mist, 噪声强度)；`fog_extra` = (高度衰减 k, 参考面 ground, 噪声频率, sigma_base)；`fog_anim` = (相位 phi, 幅度, 行波频率, 保留)。

每帧命令流：`poll_events()`（最前，喂输入状态机）-> 120 Hz 一拍（`process_events` -> M/ESC -> `update_camera` -> `update_environment` -> `input.end_frame()`）-> `ui_frame()`（`NewFrame` + 构建界面）-> `render_scene()`（脏实例先重传 -> `acquire_context` -> 上传 UBO（与上帧 memcmp 不同才传）-> `begin` -> 场景/天空 -> `render_ui(cmd)` 在 pass 内录制 -> `end` -> submit/present）-> FPSDetective。

## 8. 验收证据（本轮实际执行）

- `xmake b`（全量目标）通过；`xmake b fog` 通过。
- 着色器：`build_shaders.sh` 在 `before_build` 里重编；本轮另外手动用 `glslangValidator -V --target-env vulkan1.3 -Ishaders` 复核 `fog.frag`/`sky.frag` 无报错，`assets/fog/shaders/*.spv` 均为最新。
- 7 个测试二进制全部 exit 0：`ave_buffer_test`、`ave_shape_loader_test`、`ave_prefab_test`、`ave_reflect_test`、`ave_event_test`、`fog_camera_test`、`ave_debug_messenger_test`。
- 隐私扫描（两仓库分别跑）通过。
- ImGui 依赖：`imgui v1.92.9+b-docking` 的 **vulkan 后端**已安装（`~/.xmake/packages/i/imgui/.../include/backends/imgui_impl_vulkan.h` 存在，`libimgui.a` 含 `ImGui_ImplVulkan` 符号）。
- **未由我验证（按你的要求不自己跑程序）**：M 锁定后光标是否消失、ESC 退出是否还出现 `vkDestroyBuffer`/`vkFreeDescriptorSets` 之类的 "still in use"、`vkDestroyDevice` 是否还有泄漏计数、界面观感与时间项效果。

## 9. 待确认 / 未实现 / 已知限制

- 若 M 锁定后光标**仍可见**，剩两条路（各有取舍，需要你定）：`GLFW_CURSOR_CAPTURED` + 透明光标（隐藏必然，但 GLFW 只在 `DISABLED` 下建相对指针，视角会被限制在窗口边界内）；或让示例走 XWayland（`glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11)`，`DISABLED` 下用 `cursorNone` 隐藏且给虚拟无限坐标，两者都正常）。
- `assets/scene.json` 是否入库仍未定（生成器 `tools/gen_scene.py` 已入库）。
- 未实现：froxel/体积步进、阴影、点光/聚光、PBR、透明水排序、碰撞、方块编辑（与上一节一致）。
- 本轮没有改动 `alib6`；AVE 由本仓库 target 直接构建，不需要安装。

## 10. 着色文件整理

- `examples/fog/shaders/`：6 个文件**全部在用** -- `fog.vert`/`fog_instanced.vert`/`fog.frag`/`sky.vert`/`sky.frag` 由 `build_shaders.sh` 编译，`fog_common.glsl` 被这 5 个 include（`-I` 提供路径）。`examples/fog_gl` 用的是它自己的 `fog_gl/shaders/*`，不共享这里的文件（此前按 basename 搜索会误判，已确认）。
- `assets/fog/shaders/`：里面那两份 **9 月 29 日的旧副本** `fog.frag`/`fog.vert`（与当前源码不同，运行期只加载 `*.spv`，没有任何地方引用）已移到 **`assets/fog/shaders/unused/`**。`assets/` 未纳入版本控制（只跟踪 `assets/README.md`），所以这次移动是本地整理、不产生提交。
