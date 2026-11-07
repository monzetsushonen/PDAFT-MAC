# PDAFT-MAC

保留原版 Windows x86_64 PDAFT，通过 **ARM64 Wine / FEX / WineMetalGL** 在 Apple Silicon macOS 上运行的实验项目。macOS 启动器与兼容运行时使用 ARM64，原版游戏核心保持 x86_64；当前路线不依赖 Rosetta 进程转译。完整引擎重写与净室开发保持暂停。

## 2026-11-07 实测进度

- P1：原版启动、进入选曲已通过。CPU / WGL 探针及实际进程采样确认 ARM64 宿主、FEX 与原生 WineMetalGL 执行链。
- P2：首个完整 3D 曲目 **Tell Your World / EASY** 通过离线 AutoPlay v3：**204 COOL、204 连击、184970 分，FINE / SAFE / SAD / WORST 全为 0**。用户确认画面、音乐、结算和返回选曲正常。
- AutoPlay 修正同键连打的按下边沿、滑条持续接触与长滑条延续；24 项输入边界检查和 31 项调度回归检查通过。判定、得分、谱面和原版时间轴保持不变。
- 当前基线为 1280×720、离线 Guest、仅官方 MDATA；正式成绩保存关闭。视频 MV、HDR、MetalFX 在此基线关闭。

首曲通过不等于全部兼容。其他谱面、HOLD 奖励、多角色/服装、MOD、视频 MV、显示帧率稳定性、精确音画/输入延迟和正常退出/设置保存仍需验收。

详见 [开发路线](ARM64-ROADMAP.md)、[兼容测试与构建](docs/COMPATIBILITY.md)、[本次验收记录](docs/validation/2026-11-07-first-song.md)及[结构化结果](docs/validation/2026-11-07-first-song.json)。

## 游戏与依赖准备

游戏本体、官方更新、补丁和 MOD 请通过原始项目主页自行准备：**https://rentry.co/PDAFTforPC** 。保留此原始 URL；本仓库的中文说明是本项目适配说明，并非原页面全文翻译。

本仓库仅发布适配源码和说明，不提供游戏资源、MOD、第三方补丁/插件、Wine、编译器、密钥、个人存档或卡信息。正式安装包继续采用无资源精简方案，依赖用户导入合法取得的本体与补丁。

当前兼容实验固定以下版本：

- 原版 7.10 x86_64 主程序，SHA256：`8bad363bafa079c5974daba8a4cf2e2edf91f227686cbf99b45d44786d2de5af`。
- PD-Loader `becac44e`，其中 TLAC SHA256：`9b713927e729e14d3f8265eb5fff1d6b91af71168723b4706b4ee381192f4707`。
- DivaGL `0.11.0.0`；[MetalSharp Wine v5.0](https://github.com/metalsharp/MetalSharp-Wine/releases/tag/v5.0)。

这些组件由用户自行准备，来源与许可独立于本仓库，见 [THIRD-PARTY.md](THIRD-PARTY.md)。原始游戏只读使用；运行覆盖目录、前缀、缓存和隔离 RAM 放在外置 SSD。

## 构建当前实验代码

安装 Xcode 命令行工具后，可独立运行不含游戏资源的输入测试：

```sh
python3 tools/compatibility/build-adapter.py \
  --checks-only --output /Volumes/YourSSD/PDAFT-build/input-checks
```

Windows 测试插件的构建还需要用户提供 ARM64 Wine、专用编译前缀及 ARM64 MSVC 链接工具。当前本地诊断启动器依赖已建立的外置实验目录，不具备新的通用资源导入流程；具体布局和命令见 [兼容构建说明](docs/COMPATIBILITY.md)。源码目录为 `native/Compatibility/`。

此轮更新为源码与验收记录，不替换旧 DMG。完整的可导入、可分发无资源 App / DMG 仍属 P3。

## 历史研究版

已有 `native/FrameBridge/`、`native/Localization/`、`native/MetalFX/` 和旧构建工具保留供对照。旧 0.2.x 使用 x86_64 Wine / Rosetta，其 MetalFX / HDR / 本地化结果不能代替当前 ARM64 Wine 路线验收，安装与构建说明见 [旧版文档](docs/LEGACY-0.2.md)。

本项目不属于 SEGA 或 Crypton 官方项目。
