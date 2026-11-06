# fog-android 项目规则

## APK 构建命名（必须遵守）

- 每次构建 Android APK 前，选择一个概括本次版本内容的总结名。用户指定时使用用户的名字；未指定时根据本次改动选择并说明，不沿用无意义的默认名。
- 通过 Gradle 参数 `-PfogBuildName=<总结名>` 显式传入，不允许无名构建。
- 总结名必须是合法的 application ID 单段：使用小写字母开头，后续仅含小写字母、数字或下划线（`[a-z][a-z0-9_]*`）。
- 应用 ID 为 `com.example.fogandroid.<总结名>`，桌面显示名为 `<总结名>-fogandroid`。
- 当前直接移植存档版本总结名为 `directport`：应用 ID `com.example.fogandroid.directport`，显示名 `directport-fogandroid`。重新构建同一存档可以继续用相同名字。
- 启动前分辨率选择版本总结名为 `resolution`：应用 ID `com.example.fogandroid.resolution`，显示名 `resolution-fogandroid`。保留 `directport` 作为直接移植基线的名字。
- 命名只改变 application ID 与显示名，不修改 Kotlin package、Gradle namespace 或 JNI 符号，否则原生方法会失配。不同总结名的版本可以并存，私有数据互相隔离。
- `app-debug.apk` 保留标准输出文件名及既有单文件 HTTP 下载路径。交付前检查实际 APK 中的应用 ID、显示名与完整 Activity 类名，不能只检查源码。

## 仓库与验收

- 本目录已并入根仓库公开跟踪（科研轮转结束），按根仓库流程提交；
  不要在目录内重新 `git init` 建独立仓。
- 保留用户改动；构建成功不等于设备运行或无泄漏已经验证。未获授权不顺带做性能优化。
