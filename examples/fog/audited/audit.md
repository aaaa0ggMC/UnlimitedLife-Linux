# fog 改动审计说明

前面的场景/光照说明记录初始实现；**最新性能优化的差异、测量与限制见末尾
“CPU 提交优化”**。其中 1303 个可渲染实体仍保留，但默认不再对应 1303 次 draw。

## 范围与未实现项

本说明覆盖此前接入的 Shape/天空与相机方向修正，以及本轮相机控制、
Blinn–Phong 和方块综合场景。应用代码位于独立私有仓库 `examples/fog`，
AVE 的 Shape/Image 接口直接复用，没有新增 Model、Mesh、Material 引擎层。

本轮没有实现：阴影、点光/聚光灯、PBR、动态反射/折射、透明水排序、碰撞、
方块编辑、chunk mesh/instancing、体积雾积分或 compute shader。
Minecraft 只是场景风格，不是游戏机制或资源兼容实现。

## 逐文件变更

| 文件 | 主要职责与改动 |
| --- | --- |
| `app_comp.h` | ECS 组件与轻量实体包装。`CameraTilt` 的灵敏度改为 0.065；原始鼠标 X/Y 均在组件内取反。`Transform` 的本地轴以 quaternion × vector 转为世界轴，与渲染矩阵一致。新增 `camera_move_delta`、`DirectionalLight`、`SurfaceMaterial`。`ObjectEntity` 自动挂表面组件；天空没有 Transform。 |
| `app.h` | AppConfig、CPU/GPU 参数结构及资源所有权。新增可选 lighting 配置、太阳实体及缓存组件引用；UBO 增加相机位置/光照；push 增加材质参数/颜色；去掉旧地面资源。相机、雾和太阳由 App 长期持有。 |
| `app_setup.cpp` | 仅负责窗口、Renderer、几何、pipeline、纹理和 descriptor 初始化。加载 OBJ/STL 后检查三角形索引和 draw 数量范围，规范化尺寸/落地；只在缺少法线时计算补充法线。创建场景/天空两条 pipeline；按 swapchain image 创建 UBO 和两组 descriptor。旧平面地面、wall.jpg 与 ground descriptor 已移除。 |
| `app_scene.cpp`（新增） | `setup_world` 从资源文件分离。用共享 cube Renderable、少量表面预设和 block 助手创建岛屿、路径、湖泊木桥、木屋、树林、祭坛、遗迹、水晶及火把；设置太阳初始参数和天空实体。 |
| `app_systems.cpp` | 相机输入调用统一移动函数；原 update_fog 收拢为 update_environment，负责雾参数与 B 太阳高光开关。逐帧上传相机/雾/太阳参数，查询 Transform + Renderable + SurfaceMaterial 绘制物体，再查询 Skybox + Renderable 绘制天空。 |
| `app_run.cpp` | 更新控制说明。基准偏航围绕新场景总览方向，默认位置 (10,8,14)、pitch -22°、yaw 30°；`FOG_BENCH_STATIC=1` 冻结基准相机用于图像对比。不影响正常鼠标控制。 |
| `main.cpp` | 本轮未改。继续通过 alib6 schema 读取 `assets/config.fog.json`；缺少可选段时采用默认值。 |
| `shaders/fog.vert` | 输出 world position、world normal、view position、UV；法线用 model 的逆转置，支持当前场景的非均匀缩放；同步 UBO/push 布局。 |
| `shaders/fog.frag` | 纹理/纯色取 albedo，可选程序方块像素效果；计算环境光、Lambert 漫反射、Blinn 半角高光和自发光，最后套原来的线性雾。 |
| `shaders/sky.vert` | 仍使用 P × mat4(mat3(V))，去平移保留旋转/投影；`clip.xyww` 令普通深度为 1；同步完整 UBO 声明。 |
| `shaders/sky.frag` | 全景贴图或程序天空；程序太阳方向复用 DirectionalLight，不再独立硬编码。 |
| `build_shaders.sh` | 此前新增，本轮复用。编译四个 GLSL 到主项目 assets 下的 SPIR-V；shader 源码留在私有仓库。 |
| `tests/camera_test.cc` | 使用真实组件和统一移动函数。验证 36 组 yaw/pitch 的移动轴与 view/model 一致，鼠标符号及 pitch 限制；新增精确灵敏度、左右 Shift、Space、相反输入抵消、与 W 组合但不加速的断言。 |
| `tests/sky_math_test.cc` | 此前新增，本轮未改。独立 GLM 测试：天空不随平移、随旋转与投影变化、深度为 1。 |
| `config.example.json`（新增） | 可复现的新场景配置。模型路径、天空贴图、雾 8..55、环境光与高光开关。 |
| `README.md` | 运行命令、控制、场景组成、配置与测试入口；明确水/火把/阴影限制。 |
| `audit.md`（新增） | 本说明。 |

主项目的 `xmake/examples.lua` 中，此前增加了 fog shader 构建 hook 和非默认的
`fog_camera_test` target；本轮继续复用，没有新增根仓库构建逻辑。
本地 ignored 的 `assets/config.fog.json` 仅把雾从 5..25 调整为 8..55，
避免新场景远景被过早完全遮盖。它不提交；同等设置已放进私有 example 配置。

## 相机输入约定

- 右手世界坐标，相机本地前方为 -Z，右方为 +X，竖直为世界 +Y。
- yaw 绕世界 Y，pitch 绕 yaw 后的本地 X；旋转组合为 `Ry(yaw) * Rx(pitch)`。
- 鼠标 +X 向右、+Y 向下，因此 yaw 和 pitch 都减去原始鼠标增量。
- 默认 100 像素对应 6.5°；pitch 限制在 [-89°,89°]，不修改输入采样机制。
- WASD 方向先归一化，以免平面对角组合加速；再独立叠加世界 Y 升降。
- Space/E 上升，左右 Shift/Q 下降，速度均为 6 单位/秒；同时按上下抵消。
- Shift 不再乘速度。W 仍跟随视线，因此看向上方时 W 自身仍有向上分量；
  Space/Shift 的世界 Y 分量不会受 yaw/pitch、view matrix 或 WASD 归一化影响。
- 同时飞行和升降时总位移长度可能超过 6 单位/秒，这是独立升降的明确选择，
  而非新的 sprint。若以后要求统一总速度，应重新决定组合输入规则。

## ECS 与渲染数据流

```text
配置 → 相机/雾/太阳运行期组件
ShapeLoader / Prefab → 共享 GPU 几何切片
场景布局 → Transform + Renderable + SurfaceMaterial
天空     → Skybox + Renderable（无 Transform/SurfaceMaterial）

每帧：输入 → 相机组件 → view/proj
     雾/太阳组件 → CameraFogUbo
     acquire + 等待图像 fence → 更新该图像的 UBO
     物体查询 → push model/表面参数 → draw
     天空查询 → 专用 pipeline → draw
     提交/呈现
```

Renderable 只引用几何切片，不持有 AVE 图像/缓冲/pipeline 的所有权。
SurfaceMaterial 是应用 ECS 表面参数，不负责资产加载或 descriptor。
一个太阳实体通过缓存 ref 访问，不在每个物体 draw 中重复查询。

所有普通物体共享每图像的场景 descriptor：binding 0 UBO，binding 1 ice.png。
solid 物体不用纹理采样分支，但 descriptor 仍绑定有效纹理；水晶用纹理分支。
天空使用另一组 descriptor，binding 1 为全景图或有效的占位 ice 图。

窗口重建时先等待设备空闲，再整体替换场景 descriptor pool 和 UBO。
没有在普通帧等待设备空闲，也没有通过单个共享 UBO 覆盖正在使用的旧帧数据。

## 光照模型与 GPU 布局

在同一世界空间计算：

```text
N = normalize(inverseTranspose(model) * localNormal)
L = normalize(directionToSun)
V = normalize(cameraPosition - worldPosition)
H = normalize(L + V)

diffuse  = max(dot(N,L),0)
specular = diffuse > 0 ? Ks * pow(max(dot(N,H),0),shininess) : 0
lit      = albedo * ambient
         + lightColor * (albedo * diffuse + specular)
         + albedo * emission
```

V 和 H 的归一化分母有 epsilon 保护。高光只在朝光面启用；B/lighting.specular
控制全局高光倍率。Dragon 使用 Ks=0.65、shininess=64；水面 0.8/96；
石材、草和木材采用较低 Ks。该模型是简单非能量守恒 Blinn–Phong，不是 PBR。
雾在光照之后混合；当前太阳不做阴影遮挡，所以屋内仍有直接光。

| GPU 数据 | 大小/布局（字节） |
| --- | --- |
| CameraFogUbo | 208：view 0、proj 64、fog_color 128、fog_params 144、camera_position 160、light_direction 176、light_color 192 |
| PushConstant | 96：model 0、params 64、base_color 80；params = solid/Ks/shininess/voxel，base_color.w = emission |
| SkyPushConstant | 16：exposure、textured、两个未用槽 |

CPU 使用 alignas(16)、GLM mat4/vec4、既有兼容性 static_assert，
另用 sizeof/offsetof 断言验证 UBO/push 总大小和尾部字段偏移。
当前 UBO 布局同时适用于 shader 默认 std140 和这些字段的 std430；无 vec3 对齐陷阱。
实际 pipeline push 范围由 AVE constant_layout 生成；96 字节低于 Vulkan 保证的 128 字节。
shader 内的 UBO 声明目前复制在四个文件中，必须同步修改。

## 场景布局与资源来源

- 确定性 27×25 岛屿：x ∈ [-13,13]，z ∈ [-16,8]；后部整数高度台阶。
- 中央石路和祭坛、左侧木屋/树丛、右侧沙岸湖泊/木桥、后方遗迹/树木，
  前方冰水晶作为高光对比对象。
- 一根缩放的 cube 表示一个地形柱，避免生成整柱内部不可见的单位方块。
- 当前加载 Dragon 时实际创建 1307 个实体，其中 1303 个普通可渲染物。
- 屋顶、门窗洞与树冠由小循环构建；栏杆/树干/火把用非均匀缩放。
- 方块表面是 shader 中的 16×16 UV 像素噪声、草皮侧面及边缘暗线；
  木纹按像素行分组。无 Minecraft 官方纹理或额外网络素材。
- 复用本地 `assets/test_data/dragon.obj` 与 `assets/test_data/imgs/ice.png`。
  没有新增下载，也没有把大型模型复制进仓库。Dragon 的来源/许可未在本轮重新核验；
  公开分发前需要确认已有文件的来源，而不是只凭文件名认定可分发。
- 仍支持自定义 OBJ/STL 和经纬度全景图；无模型时只有祭坛，没有模型占位替身。

## 已进行的简化

1. 旧平面地面及 wall.jpg 加载、GPU 切片、ground 标记、第三组 descriptor 全部移除。
2. 场景不再按对象切换“地面/立方体”descriptor；整个物体 pass 只绑定一次 descriptor，表面差异由统一 push 参数表达。
3. `app_scene.cpp` 只描述布局，`app_setup.cpp` 只处理资源，避免初始化函数继续膨胀。
4. 统一 block 助手负责实体创建、缩放和 dirty 标记，不重复写完整 Renderable。
5. 相机实际控制与单元测试共用 camera_move_delta，避免测试复制一套移动公式。
6. 天空太阳与场景太阳共享方向；缓存太阳组件引用。
7. 已有法线完整时不再计算每个三角形的补充法线，减少 Dragon 启动阶段冗余工作。

## 后续可提取或合并的部分

按收益优先，而不是现在就增加框架：

1. **GPU 参数定义同步**：C++ 与四份 GLSL UBO 容易漂移。可以提取公共 GLSL include，
   或生成布局并配套 offset 断言；需同时调整 shader 编译的 include 目录。
2. **voxel 类型约定**：目前 CPU 的 int 与 shader 分支用 1..5。适合抽取明确 enum/常量表，
   但不要把 GLSL 编译复杂化；核对测试和场景作者可读性后再定形式。
3. **Renderable.solid 与 SurfaceMaterial**：solid 是采样策略而非几何性质，
   以后可迁移到表面参数，避免两处共同决定 albedo。
4. **命名**：继承来的 axis_left 实际是本地 +X（right），CameraFogUbo 现在也含光照，
   descriptor_cubes 对应整个场景。后续统一命名比新增类型层更有价值。
5. **每图像资源聚合**：当前平行 vector 保存 UBO/场景 descriptor/天空 descriptor。
   可以变成一个 FrameResources/ImageResources vector，收拢 resize 和索引检查。
6. **Shape 规范化**：尺寸/落地/缺失法线预处理适合提成纯函数并增加小资源测试。
   当前它仍是 fog 的展示策略，不应直接变成 AVE Loader 的强制行为。
7. **场景块批处理**：现在每实体一个 draw、全量绘制。若要规模扩大，先按材质分组，
   再决定 instancing 或 chunk 外表面 mesh；不要先设计复杂 SceneGraph。
8. **光照多源**：当前只有一个方向光，可以保持单源 UBO 直到确实需要灯光列表。
   火把要照亮周边时才引入点光/衰减；阴影另作独立渲染 pass。
9. **固定材质预设**：当前局部预设足够；未来共享到多个场景时再抽成表，
   不需为了这一张场景定义 Model/Material 资产管理框架。

## 验收证据与复现

从主项目目录执行：

```sh
xmake build fog fog_camera_test
xmake run fog_camera_test
xmake run fog
g++ -std=c++26 -O2 examples/fog/tests/sky_math_test.cc -o /tmp/opencode/sky_math_test
/tmp/opencode/sky_math_test
LD_LIBRARY_PATH=build/linux/x86_64/debug build/linux/x86_64/debug/ave_shape_loader_test
```

实际已完成：

- fog 和 camera_test 使用项目 C++26/xmake 工具链真实编译。
- camera_test 新旧方向及世界升降断言全部通过。
- 现有 ShapeLoader 真实资源测试通过，Dragon 为 438976 顶点、2613918 索引。
- 四个 SPIR-V 通过 spirv-val。
- NVIDIA Vulkan + Khronos validation 下，dynamic、legacy、天空贴图、关闭高光
  四种配置正常运行；每种连续两次 resize 并正常退出，日志无 WARN/ERROR。
- 独立测试运行目录生成配置和资源符号链接，不修改用户已有运行配置。
- 静止相机、相同尺寸的高光开/关 GPU 图片对比：20053 个像素变亮，
  最大单通道增加 85（8-bit）；证明 shader 高光分支实际生效，不只是参数上传。
- 已审查实际新场景截图：祭坛/木屋/湖桥/方块树/水晶正常显示。

临时图像与 smoke 脚本在 `/tmp/opencode/`，不提交。图像对比是固定场景证据，
不是逐像素 BRDF 数值真值测试，也不是跨驱动 golden image。
基准辅助：`FOG_BENCH=1 FOG_BENCH_STATIC=1 FOG_BENCH_WARMUP=0 FOG_BENCH_SECONDS=9`。
没有把该短时 smoke 的 FPS 当性能结论；未运行 clang-tidy 或完整 sanitizer 构建。

## 已知限制与审计关注点

- 线性雾不是此前讨论的前向体积积分；天空也没有独立沿视线计算雾。
- 普通物体无 alpha blend；水只做不透明颜色/高光，树叶是实心方块。
- 环境光、材质颜色和曝光为展示参数；没有 HDR、tone mapping、IBL 或能量校正。
- 生成几何时不去掉相邻块隐藏面，没有视锥裁剪/批处理；适合演示规模。
- normals 的逆转置在 vertex shader 中逐顶点计算。若性能证据表明瓶颈，
  可预计算 normal matrix；注意新增 push 字节不能超过设备限制。
- Shape index 新增应用级检查不等于完整不可信资产安全审计；NaN 坐标、解析器容错、
  极端尺寸、内存分配失败等还应在 Loader/资源层单独审查。
- 自定义天空贴图缺失/损坏沿用既有错误通道，不保证自动恢复为程序天空。
- 新对象必须拥有 SurfaceMaterial 才进入普通渲染查询；天空故意不满足此查询。
- 每图像 UBO 的正确性依赖 AVE acquire 对该图像旧 in-flight fence 的等待；
  若以后改队列/提交模型，应重新审查资源生命周期，不直接套用当前假设。
- 继承的 App 资源析构顺序和 AVE wait-idle 策略保留，本轮未重构全局资源所有权。

## CPU 提交优化（后续任务）

### 调查结论

用户报告约 250 FPS，猜测 CPU/频繁数据上传；明确要求不优化 cull face。
本轮保留所有 `VK_CULL_MODE_NONE`，不修改阴影、场景内容、分辨率或校验层默认设置。

原始实现每帧只有一次 208B 的 UBO 上传，几何和纹理已在初始化时上传。
真正频繁的是 1303 个物体的 push constant、vertex/index bind 和 indexed draw，
加上天空共 1304 次 draw。Vulkan validation 放大了大量命令调用的 CPU 成本。

CPU 分阶段基准：Debug、NVIDIA RTX 5060 Laptop、dynamic rendering、MAILBOX、
1280×960 framebuffer、固定相机、2 秒预热 + 5 秒测量，各配置 3 轮。
表中每个值是三轮对应统计量的中位数；各列中位数不必来自同一轮。

| 对照 | FPS | CPU record µs/帧 | CPU UBO 阶段 µs/帧 |
| --- | ---: | ---: | ---: |
| 原始逐对象，validation 开 | 244.72 | 3830.76 | 5.54 |
| 原始逐对象，validation 关 | 784.25 | 945.48 | 4.29 |
| 仅缓存 UBO，仍逐对象，validation 开 | 248.84 | 3787.17 | 1.47 |
| 实例化 + UBO 缓存，validation 开 | 1123.24 | 74.05 | 1.65 |
| 实例化 + UBO 缓存，validation 关 | 1125.21 | 22.84 | 1.58 |
| 实例化 + 强制上传 UBO，validation 开 | 1125.44 | 71.57 | 6.14 |
| 保留的逐对象 + 强制上传对照，validation 开 | 248.53 | 3790.15 | 5.38 |

结论是 **CPU 命令录制/提交量为原始主要瓶颈，UBO 上传不是关键问题**。
只缓存 UBO 的改善处于短基准波动量级；实例化收益约 4.6 倍（validation 开）。
没有用“关闭校验层”冒充优化，也没有通过少画东西或更改 cull 来获取收益。

以上主对照是在用户提示断电前测得。未记录 GPU 功耗/时钟和电源档位，不声称
这些绝对 FPS 在电池供电或其它系统状态下恒定。断电后的最终正确性检查不并入该表。
原始完整日志/JSON 曾存放在临时目录，恢复时已被清理；本表保留本次工具实测统计，
复现入口保留在 README。后续测试截图/日志由仓库内 render_check.py 重新生成。

### 当前修改按文件说明

| 文件 | 性能任务的增量 |
| --- | --- |
| `app_comp.h` | 增加 StaticInstance/DirectDraw 标记。ObjectEntity 默认直接绘制，显式 batch_cube=true 才加入静态方块批次；两种标记互斥，避免新增实体漏画或重复画。 |
| `app.h` | 新增实例 SSBO/pipeline、每图像 UBO 内容缓存、性能开关与四阶段累加器；make_draw_data 统一逐对象和实例路径的 96B 表面/变换打包。 |
| `app_scene.cpp` | 方块和水晶选择静态 cube；Dragon 保持 DirectDraw。setup_instances 只在启动时遍历静态 ECS 数据，验证共享 cube 几何与实例数范围，上传 1302×96=124992B 的 device-only SSBO。 |
| `app_setup.cpp` | 初始化顺序改为 world → instances → UBO/descriptors；普通场景配置派生实例化 pipeline，共用其余 raster/depth 状态；descriptor binding 2 增加 vertex readonly storage buffer；resize 清除各图像 UBO 缓存。 |
| `app_systems.cpp` | 仍 acquire/fence 后才写 UBO；memcmp 比较该图像最后内容，变化才上传，成功后缓存。正常帧只查询 DirectDraw，再一次 indexed instanced draw；FOG_INSTANCING=0 保留全实体路径。按需累计四段 CPU 墙钟时间。 |
| `app_run.cpp` | 预热结束时重置计时与 upload 计数；退出输出 CPU_PROFILE。正常未开启 profiling 时不读取这些阶段的时钟。 |
| `shaders/fog_instanced.vert`（新增） | std430 SSBO stride 96，gl_InstanceIndex 读取 model/params/color；与普通 vertex shader 输出相同接口。无需 compute、额外实例扩展或 AVE 新 API。 |
| `shaders/fog.vert` | 表面参数通过 flat varying 输出，与实例 vertex shader 一致。 |
| `shaders/fog.frag` | 接收 flat 参数，移除 fragment push 声明；Blinn–Phong/雾/像素表面公式保持不变。 |
| `build_shaders.sh` | 增加第五个 shader 的编译。 |
| `tests/camera_test.cc` | 新增 DirectDraw/StaticInstance 构造标记互斥检查；保留所有相机控制检查。 |
| `tests/render_check.py`（新增） | 可复现的原生渲染回归；强制测试子进程使用 X11，以 PID 匹配窗口，只捕获/缩放自己的窗口。验证实例/直接路径像素一致、legacy/dynamic、天空贴图、高光开关、无模型和动态相机缓存更新。 |
| `README.md` / `audit.md` | 记录开关、复现、测量结论及只读批次的限制。 |

根仓库及 AVE 代码未改，复用现有 `DescriptorBinding::ssbo`、VMA Buffer、
`draw_indexed(index_count, instance_count)` 和 staging upload。

### 缓存与生命周期

1. 静态实例仍保留在 ECS，但默认绘制的是启动时的 GPU 快照，不再逐帧遍历全部静态组件。
2. 相机/雾/太阳参数是全局 UBO，移动或切换高光仍影响整批，无需重传实例数据。
3. 普通 UBO 数据由连续 mat4/vec4 构成，sizeof 208 对应所有字段之和，无字段间 padding；
   全部字段赋值后比较对象表示。未来更改布局时应重新审查，不能任意沿用 memcmp。
4. 每 swapchain image 独立缓存；新图像第一次使用强制上传，resize 全部失效。
5. 静态实例用自己尺寸匹配的临时 staging，而不是依赖 Dragon 几何 staging 恰好够大。
   无 Dragon 消融测试曾发现借用 staging 容量不足，已修正并重新验收。
6. upload 等待复制完成后释放临时实例 staging 和原几何 staging，保留 device-only 实例缓冲。
7. 新建 ObjectEntity 默认 DirectDraw；运行时移动/修改材质的对象应保持此默认路径。
   **对标记 StaticInstance 的对象修改 Transform/SurfaceMaterial，当前不会自动刷新快照**。
   静态块的运行期增删、rebuild、多个批次尚未实现；不可在正在使用时随意替换 buffer。

### 计时的边界与剩余开销

- acquire 段包括 Renderer 的 fence/acquire 等待，不是纯 CPU 执行工作，更不是 GPU timestamp。
- record 段包括 begin rendering、ECS 查询、命令录制；submit/present 段包含 vkQueuePresent 等。
- 不含更早的 view/proj/UBO 参数计算、poll_events、输入更新及 resize 回调；总 FPS 包含这些。
- profiler 只在启用时调用阶段时钟，预热后的 ubo_uploads 只计测量区间。
- 优化后校验开启时 acquire 中位约 641µs，record 约 74µs。
  校验关闭后 record 约 23µs，但 FPS 仍约 1125：原始 CPU 提交瓶颈已大幅削弱。
- 同一组后续消融：framebuffer 改为 640×480 时中位约 1987 FPS；禁用 Dragon 时约 1363 FPS
  （三轮范围 1292..1584，波动较大）。这提示像素/GPU 工作与呈现等待已更重要，
  但未测 GPU timestamp，不能精确归因 acquire 等待。没有用这些降负载结果作为优化收益。
- 移动相机检查中 5246 个测量帧均上传了 UBO；不会因缓存导致相机画面冻结。

### 最新验收

- 当前代码真实编译，camera_test（含分类断言）通过。
- 五个 SPIR-V 验证通过。
- 仓库内 render_check.py 的七种配置全部正常运行并连续 resize，validation 无 WARN/ERROR。
- 实例化与直接路径的固定视角截图逐像素相同（0 差异像素）。
- 无 Dragon 资源路径正常，实例 staging 不依赖模型大小；动态相机仍按需更新 UBO。
- 没有修改系统电源/GPU 设置；最终图像检查与早期性能统计不混算。
