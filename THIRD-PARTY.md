# 来源与依赖

本仓库仅分发适配代码和说明，不包含下列第三方运行文件或游戏资源。

- PD Loader: https://github.com/PDModdingCommunity/PD-Loader ，参考版本 becac44e。输入和数据结构参考其公开接口。
- DivaGL: https://github.com/korenkonder/AFTMods ，参考版本 DivaGL-v0.11.0.0。渲染阶段/调试标记适配依据其公开源码。
- Wine/GPTK 包装参考：https://github.com/Gcenx/game-porting-toolkit 。用户自行准备兼容 Wine，遵守对应上游许可与 Apple 组件适用条款。
- VC++ Runtime: https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist ，用户自行取得。
- Apple Metal / MetalFX / AppKit / IOSurface: macOS 系统框架，不随本包分发。
- 游戏、MOD 与补丁准备说明的原始 URL：https://rentry.co/PDAFTforPC 。中文 README 为适配本项目重新编写的准备说明，不是原页面全文翻译。

第三方组件及游戏内容的权利仍归各自作者。本项目许可仅涵盖本仓库新增的适配源码。

## 2026-11-07 ARM64 兼容实验（运行文件不分发）

- [MetalSharp Wine v5.0](https://github.com/metalsharp/MetalSharp-Wine/releases/tag/v5.0)：本地验证原版菜单与一个 3D 曲目。上游脚本/构建胶水、Wine、FEX 和其余 payload 许可各自独立，不覆盖在本仓库 MIT 许可下；本仓库不包含运行时。
- [本次固定 WineMetalGL 源码](https://github.com/metalsharp/MetalSharp-Wine/tree/ed55ccaa535924b8d95f04cb78359bf27ed41668)：OpenGL 到 Metal 的后端。上游文件级许可与依赖声明独立适用；当前 pipeline archive 关闭，ASF 媒体路径仍有缺口。
- 原生 AppKit 外壳、自有 CPU / WGL / 媒体探针、离线输入调度与构建工具源码在 `native/Compatibility/` 和 `tools/compatibility/`。不分发主程序、第三方插件、编译器或其 DLL。
- 输入与只读结果布局参考 PD-Loader `becac44e` 的公开接口：
  - [Keyboard](https://github.com/PDModdingCommunity/PD-Loader/blob/becac44e/source-code/source/plugins/TLAC/Input/Keyboard/Keyboard.cpp)
  - [Binding](https://github.com/PDModdingCommunity/PD-Loader/blob/becac44e/source-code/source/plugins/TLAC/Input/Bindings/Binding.cpp)
  - [TouchSliderEmulator](https://github.com/PDModdingCommunity/PD-Loader/blob/becac44e/source-code/source/plugins/TLAC/Components/Input/TouchSliderEmulator.cpp)
  - [TargetInspector](https://github.com/PDModdingCommunity/PD-Loader/blob/becac44e/source-code/source/plugins/TLAC/Components/GameTargets/TargetInspector.cpp)
  - [TargetState](https://github.com/PDModdingCommunity/PD-Loader/blob/becac44e/source-code/source/plugins/TLAC/Components/GameTargets/TargetState.h)、[ScoreSaver](https://github.com/PDModdingCommunity/PD-Loader/blob/becac44e/source-code/source/plugins/TLAC/Components/ScoreSaver.cpp)
- 原版游戏、PD-Loader、DivaGL 与所有运行依赖仍由用户自行取得。首次可分发的 ARM64 Wine 集成包还需完成 P3 的许可、依赖、签名、公证和导入验收。
