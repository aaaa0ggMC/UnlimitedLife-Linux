# UnlimitedLife-Linux 一个长期项目
## 项目介绍
这个项目主要是作为一个游戏来写的，目前我对游戏的构思中的一部分如下：<br>
>*Dream is a branch of your life.*<br>
>*Now,here is your dream*<br>
>*So ... Wh677aa3e1d934b671d2fb5d69927497439131eab560f79fd56fc4cd984ea48d69758aebd610ea1d7a48fcbe4a239c440b*<br>

## 构建与运行

项目使用 xmake，当前构建 AGE 引擎共享库及两个交互式示例，游戏原型尚未接入构建。

前置条件：

- xmake、Bash，以及支持 C++26 和 `-freflection` 的 GCC 工具链。
- 与工具链兼容的 alib5 和 `stdc++exp`。Linux 上先运行 `./scripts/install alib5`；
  迁移旧安装使用 `--adopt-existing`，详见 [安装与升级](docs/install.md)。
  UL 通过 `alib5` 包声明读取安装元数据，默认前缀为 `/usr/local`。
- OpenGL 开发/运行环境。GLFW、GLEW、GLM、stb、miniaudio、ImGui 由 xmake 声明和获取。
  Windows 配置使用 MSYS2 UCRT64/MinGW；当前实际验证以 Linux 为准。

```bash
./scripts/configure -m release --examples=y
./scripts/build
./scripts/run age_simptest
# 完整演示（也可直接 ./scripts/run）
./scripts/run agetest
```

运行前需准备 [本地资源](assets/README.md)。只构建引擎可使用：

```bash
./scripts/configure --examples=n
./scripts/build AGE
```

切回演示构建时设置 `--examples=y`。开发模式使用 `./scripts/dev`。
构建产物位于 `build/<平台>/<架构>/<模式>/`，编译数据库自动生成于根目录。
VS Code 的默认构建任务同样调用 xmake。

文件职责、依赖方向及未接入构建的模块见 [目录分层](docs/structure.md)，
脚本参数及旧入口变更见 [脚本说明](scripts/README.md)。
