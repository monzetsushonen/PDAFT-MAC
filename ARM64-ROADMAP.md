# Apple Silicon 兼容运行路线（2026-11-07）

正式目标：**保留原版 Windows x86_64 游戏核心，使用 ARM64 macOS 外壳与兼容运行时，并移除正式路径对 Rosetta 的依赖。** `diva.exe` 不改写为 ARM64 二进制。

完整引擎重写、净室开发、判定/动画/资源管理重建及整套渲染器替换：**SUSPENDED / NOT IN CURRENT SCOPE**。历史代码与构建保留。

| 阶段 | 当前状态 | 验收范围 |
| --- | --- | --- |
| P0 依赖审计 | CONFIRMED，范围有限 | 已审计本地游戏、插件、运行时及历史证据；独立 Windows 原版对照基线仍待采集。 |
| P1 原版最小启动 | CONFIRMED | 自有 x86_64 CPU / WGL 探针、ARM64 宿主采样、FEX / WineMetalGL 模块、原版可交互选曲。视频关闭。 |
| P2 完整游戏兼容 | EXPERIMENTAL，首曲通过 | Tell Your World EASY：204 全 COOL / 204 连击 / 184970 分；用户确认画面、音乐、结算、返回选曲。其余曲目与机制尚未全面验收。 |
| P3 macOS App / DMG | EXPERIMENTAL 本地诊断入口 | 启动器 ARM64 编译、ad-hoc 签名及版本预检通过；按钮 UI、通用导入、干净机器安装与正式无资源发行待验收。 |
| P4 Aime / 私服 | PROPOSED | 默认离线；后续接入用户自建私服的登录、档案与成绩同步，隔离 AutoPlay 成绩。 |
| P5 Metal 增强 | PROPOSED | 原版兼容稳定后接可关闭 MetalFX / HDR；当前验收基线关闭这些增强。 |

## 当前执行链

```text
ARM64 macOS 启动器
  → MetalSharp Wine v5.0（ARM64 / ARM64EC 宿主）
  → FEX 执行原版 x86_64 PE
  → 原版 PDAFT + PD-Loader / DivaGL
  → WineMetalGL → Metal
```

实际游戏进程的采样 Code Type 为 ARM64，FEX 与原生 WineMetalGL 被映射。运行时包中存在未使用的 Intel 可选 shims，不能将整个下载包称为只含 ARM64。系统映像列表中出现 `libRosetta.dylib`，也不能单凭库名推断进程是否被 Rosetta 转译。

此轮不是新的独立游戏引擎，也不是可不导入本体的完整 App。旧 x86_64 Mach-O 画面/视频桥接库未放入这条已验收路径。

## 已确认的规避和缺口

- 默认 Metal pipeline archive 曾在自有 GL 探针触发后台崩溃。当前设置 `WINEMETALGL_PIPELINE_CACHE=0`；这是临时规避，没有宣称永久修复或性能提升。
- 运行时缺少 ASF demuxer。当前关闭视频 MV 与 Wine GStreamer，先验证 3D 场景；现有 WMV / VC-1 decoder 不能代替缺少的 demuxer。
- 构建工具与游戏使用不同前缀。旧编译器子进程异常不等同于游戏崩溃。
- AutoPlay 仅生成原版输入，由原版进行判定/计分；不改判定窗口、谱面、逻辑时钟或游戏时间轴。新输入调度共 55 项检查通过，并有一首原版全 COOL 实测。

## 下一步

1. 扩展代表谱面、HOLD、角色/服装和控制器覆盖，保留当前首曲回归基线。
2. 补齐 ASF / 视频 MV 媒体路径，验证包含视频素材的曲目。
3. 实测显示帧率、长帧和音画/输入延迟；输入轮询频率不是显示 FPS。完成正常退出、隔离设置保存和重启验收。
4. 再完成可替换运行时的 App 导入、路径与配置管理；检查依赖许可、签名、公证及干净机器无资源 DMG 导入。

不宣称全部 MV、全部 MOD、稳定 60 FPS、无延迟或 macOS 28 已兼容。Apple 平台变化参考[官方说明](https://developer.apple.com/news/?id=w5ngl9k2)，不依赖特定旧游戏例外作为项目保证。
