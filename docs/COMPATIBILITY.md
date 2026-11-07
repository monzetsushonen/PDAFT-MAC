# ARM64 Wine / FEX 兼容实验

本目录对应 2026-11-07 的本地诊断路线。当前只完成一个官方 3D 曲目的整曲验收，不提供完整导入器，也不将旧 0.2.x DMG 标为已迁移到这条路线。

## 源码

- `native/Compatibility/PDAFTAutoPlay.c`：仅供离线测试的 PE x86_64 输入适配器。拦截固定 TLAC 版本的 `GetAsyncKeyState` 导入，保留原始按键函数；不向桌面发送输入事件。
- `P2InputScheduler.h` / `TestTargetInput.h`：普通音符独立按下、交替绑定处理连打；滑条持续接触与长滑条延续。只有音符时间推进时生成输入。
- `input-check.c` / `scheduler-check.c`：24 + 31 项边界与调度检查。通过这些检查不等于所有谱面/机制已通过。
- `P2Launcher.m`：ARM64 AppKit 诊断入口，提供启动/停止、AutoPlay 开关、日志入口与只读状态。按钮流程仍待独立验收。
- `probes/`：自有 CPU、WGL 与媒体 factory 探针；`summarize-session.py` 只读汇总插件日志，轮询计数不作为 FPS。

固定的主程序哈希与 TLAC 哈希见首页。主程序版本不匹配时不要加载这些固定地址适配。原版判定、计分、谱面与时间轴均保持原样。

## 构建输入插件

以下命令写入用户选择的外置构建目录，不把输出放入源码 checkout。`--checks-only` 仅需 Apple Silicon、Xcode 命令行工具和 Python 3：

```sh
python3 tools/compatibility/build-adapter.py \
  --checks-only --output /Volumes/YourSSD/PDAFT-build/checks
```

构建 `.dva` 还需要用户自行取得的 ARM64 Wine、ARM64 MSVC `lib.exe` / `link.exe` 及所需 DLL，以及单独的构建前缀：

```sh
python3 tools/compatibility/build-adapter.py \
  --output /Volumes/YourSSD/PDAFT-build/adapter \
  --wine /path/to/MetalSharp-Wine.app/Contents/MacOS/wine \
  --compiler-dir /path/to/private/ARM64-MSVC-link-tools \
  --compiler-prefix /Volumes/YourSSD/PDAFT-build/compiler-prefix
```

Apple Clang 生成 AMD64 COFF，再由用户的 ARM64 MSVC 工具导入/链接。输出为 `PDAFTAutoPlay.dva`，工具不会自动安装它，不包含任何第三方二进制。PE 链接时间戳可能使重建哈希不同，安装后须重建启动器的版本校验。

## 本地诊断 App 布局

此 App 使用已有的隔离实验布局，并非通用游戏文件夹导入器：

```text
external-experiments/
├── 20261107-p1/runtime/MetalSharp-Wine.app/
└── your-p2-experiment/
    ├── game/                  # 私有运行覆盖目录，不提交
    ├── game-prefix/           # 独立游戏前缀，不用于编译
    ├── isolation.sb
    ├── logs/
    ├── tmp/
    └── cache/shaders/
```

私有 `game/` 包含用户自行准备的已固定版本主程序与 PD-Loader / DivaGL、自己构建的 `plugins/PDAFTAutoPlay.dva`、隔离 RAM 和配置；`rom`、`firm`、官方 `mdata` 指向只读原始资源。游戏前缀的 `drive_c/PDAFT-P2` 指向这个覆盖目录。

只在用户自己的覆盖目录中启用 `PDAFTAutoPlay.dva`、`input_emulator`、`touch_slider_emulator` 和所需基础插件；`score_saver=false`、无 Aime 登录，关闭自定义 MOD。保持默认键位：W/I、D/L、S/K、A/J，以及左向滑条 Q、右向滑条 O。改绑后需要相应调整适配器。

当前基线配置是 1280×720、`No_Movies=1`、HDR / MetalFX 关闭。Wine 环境包含：

```text
WINEDLLOVERRIDES=dinput8=n,b;mscoree,mshtml,winegstreamer=
WINEMETALGL_PIPELINE_CACHE=0
PDAFT_P2_AUTOPLAY=1
PDAFT_P2_REPORT=<可写日志文件的 Windows Z: 路径>
```

不沿用旧版 Intel Mach-O 注入库。游戏写入仅允许当前实验根目录、必要临时目录与设备；沙箱禁止游戏外网连接和向原始资源目录写入。`isolation.sb` 是用户本地生成的路径配置，不能把其他机器的绝对路径原样复用。

在已配置好的 P2 实验旁构建诊断 App：

```sh
python3 tools/compatibility/build-launcher.py \
  --experiment-root /Volumes/YourSSD/external-experiments/your-p2-experiment
```

工具仅生成无资源的本地 App，检查当前游戏/TLAC 哈希、独立目录与正式成绩关闭状态，自动固定当前安装适配器的哈希；使用 ad-hoc 签名，不复制 Wine、游戏或补丁。关闭/停止仅作用于本次 P2 前缀。App 的 runtime 位置仍按上面的固定同级实验布局查找，通用路径选择将在 P3 实现。

游戏要保持前台直至结算。切到其他窗口会触发上游的失焦暂停，返回后需手动继续。原版结算是判定依据；内部目标转换数包含滑条内部节点，不能当作谱面音符数。HOLD 奖励和所有曲目组合尚未完成验收。

此次仅更新公开源码和脱敏验收摘要；不打包资源、编译器、运行时或私有前缀，不更新旧 DMG。
