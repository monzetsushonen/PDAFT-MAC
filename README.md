# PDAFT-MAC

实验性 macOS 启动器、画面共享与 MetalFX 输出。当前不是完整原生 arm64 游戏引擎：前端使用 arm64，游戏仍通过 x86_64 Wine/Rosetta 与 DivaGL/OpenGL 执行。完整原生 arm64 是后续目标。

## 下载与准备

游戏本体、官方更新、补丁与 MOD 请通过原站自行准备：**https://rentry.co/PDAFTforPC** 。该页面已停止维护，MOD 组合可能过时。本仓库和 DMG 不镜像任何游戏本体、MOD、补丁、中文文本/歌词、第三方游戏插件、Wine 或 VC 安装程序。

1. 从原站的 Game、Patch、PD Loader 等章节准备需要的本地文件。先解压本体，将更新中的 mdata 合并到游戏目录。首版只接受文件夹，不接受 ZIP / 7z。
2. 游戏目录应包含 diva.exe、rom、firm、mdata。当前仅接受 SHA256 为 `8bad363bafa079c5974daba8a4cf2e2edf91f227686cbf99b45d44786d2de5af` 的已验证可执行文件，以及 M215 至 M270 更新。
3. 准备单独的运行依赖目录：`Bootstrap/`（PD Loader 配置、dinput8.dll、plugins/TLAC.dva、plugins/Render.dva、plugins/Patches.dva、plugins/DivaSound.dva、plugins/DivaGL.dva 等）、`Wine/`（含 bin/wine64 与 GStreamer.framework）、`vc_redist.x64.exe`。插件版本与配置来自测试过的 PD Loader becac44e / DivaGL 0.11.0.0；其他版本尚未验证。现有私人测试 App 的 Contents/Resources 可以作为本机依赖目录使用，但不要上传或分发它。
4. 使用 Apple Silicon Mac、macOS 13+、Rosetta 2 和外置 APFS 运行目录。游戏资源可以位于 exFAT；运行前缀需要 APFS 符号链接。
5. 从 DMG 将 PDAFT MAC.app 复制到可写目录后打开，先选择外置 APFS 运行位置，设置语言/分辨率/特效，再选择合并后的游戏目录与运行依赖目录。菜单可重新选择目录。游戏原始文件只读，前缀、配置与缓存写入独立运行目录。

## 语言与画面

默认 English；实际语言取决于用户导入的资源。中文需要用户在依赖目录额外提供兼容的 Localization 数据与字体，精简版不包含中文补丁。未安装时选择中文会明确报错，可改选 English / 日本語。

整帧 MetalFX Spatial 支持 1080p / 1440p / 2160p 输出。HDR 是仅 MV 的实验性 SDR→EDR 高光增强，UI 保持 SDR；不是原始场景 HDR。AutoPlay 用于离线测试，不提交正式成绩。Aime/档案/成绩同步尚未接入。

## 已知限制

Ghost Rule 的既有研究构建有效采样约43–45fps；稳定60fps、载入停顿及所有HDR视觉异常尚未解决。公开包的首次导入与启动验证记录见 release notes。包使用临时本地签名，未作 Developer ID 签名或公证。互斥皮肤只启用一套；White Rainbow 已知结算崩溃。

## 构建

安装 Xcode 命令行工具及 macOS SDK，运行 `python3 tools/build-public.py --output /path/on/external/ssd/build`。打包使用 `hdiutil create -srcfolder /path/on/external/ssd/build -format UDZO -volname PDAFT-MAC PDAFT-MAC.dmg`。不要将游戏、依赖目录或测试日志放进构建目录。

项目适配源码见 native/，第三方来源见 [THIRD-PARTY.md](THIRD-PARTY.md)。本仓库不属于 SEGA 或 Crypton 官方项目。

## 准备依赖工具

`python3 tools/prepare-components.py --pd-loader /path/PD-Loader-becac44e.zip --divagl /path/DivaGL-v0.11.0.0.dva --wine /path/wine --vc-redist /path/vc_redist.x64.exe --output /path/on/apfs/components`

此工具只读取用户下载的文件，验证指定插件版本并生成本地配置；输出绝不能提交到仓库或放入发布 DMG。可选中文数据需另行自行提供。

后续优先级：[完整原生 arm64 路线](ARM64-ROADMAP.md)。当前研究版不承诺 macOS 28 可运行。
