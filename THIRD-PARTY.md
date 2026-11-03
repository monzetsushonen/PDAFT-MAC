# 来源与依赖

本仓库仅分发适配代码和说明，不包含下列第三方运行文件或游戏资源。

- PD Loader: https://github.com/PDModdingCommunity/PD-Loader ，参考版本 becac44e。输入和数据结构参考其公开接口。
- DivaGL: https://github.com/korenkonder/AFTMods ，参考版本 DivaGL-v0.11.0.0。渲染阶段/调试标记适配依据其公开源码。
- Wine/GPTK 包装参考：https://github.com/Gcenx/game-porting-toolkit 。用户自行准备兼容 Wine，遵守对应上游许可与 Apple 组件适用条款。
- VC++ Runtime: https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist ，用户自行取得。
- Apple Metal / MetalFX / AppKit / IOSurface: macOS 系统框架，不随本包分发。
- 游戏、MOD 与补丁准备说明的原始 URL：https://rentry.co/PDAFTforPC 。中文 README 为适配本项目重新编写的准备说明，不是原页面全文翻译。

第三方组件及游戏内容的权利仍归各自作者。本项目许可仅涵盖本仓库新增的适配源码。
