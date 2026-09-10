# 本地运行资源

xmake 将两个演示的工作目录设置为本目录。资源文件目前由开发者在本地准备，
尚未提供下载地址或完整资源包，因此仅克隆源码无法直接运行完整演示。
本目录除 README 外暂不纳入 Git，已有资源不受本次整理影响。

- `age_simptest` 读取 `test_data/cube.vert`、`test_data/cube.frag`。
- `agetest` 的模型、纹理、音频与着色器路径见 `examples/agetest/config.h`，
  另需 `test_data/wqy-microhei.ttf` 字体（见 `examples/agetest/imgui.h`）。
- `logs/` 为运行生成的日志，不应提交。

后续应将自有的小型着色器等必要资源纳入版本管理，并为大型或外部资源提供来源、
许可证和版本/校验值。未确认资源归属之前，不自动把现有本地资源加入仓库。
