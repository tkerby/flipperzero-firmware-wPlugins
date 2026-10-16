# 开发指南

[项目首页](../README.md) · [使用指南](usage.md) · [谱面支持](chart-support.md) · [开发指南](development.md)

## 构建与发布

需要 Python 3、Make、FFmpeg，以及用于下载 uFBT SDK 和工具链的网络连接。本地测试还需要支持 AddressSanitizer / UndefinedBehaviorSanitizer 的 C 编译器。以下命令以 Linux / macOS 为例。

```sh
git clone https://github.com/ZeroNinx/flipper-zero-taiko-player.git
cd flipper-zero-taiko-player
make setup
make build
```

`make setup` 在 `.venv/` 中安装 uFBT，并下载官方 release SDK。构建产物为 `dist/taiko_player.fap`，同时保存在 `dist/official/` 中。

应用需要使用与设备固件 API 兼容的 SDK 构建。使用 Momentum 固件时，可通过独立的 SDK 目录构建：

```sh
make momentum-sdk
make build FIRMWARE=momentum
```

Momentum SDK 地址由 [Makefile](https://github.com/ZeroNinx/flipper-zero-taiko-player/blob/main/Makefile) 中的 `MOMENTUM_SDK` 指定。若默认版本与固件不匹配，可在安装 SDK 时覆盖地址：

```sh
make momentum-sdk MOMENTUM_SDK="<对应固件的 SDK ZIP URL>"
```

后续的打包和上传命令也需要加上 `FIRMWARE=momentum`。构建操作不更新设备固件。

两套 SDK 初始化后，可生成不含本地曲目和设置的双固件发布包：

```sh
make release VERSION=0.3
```

此命令运行测试、依次编译两个版本，并输出 `dist/releases/v0.3/` 下的两个 ZIP 和 SHA-256 校验文件。每个包记录源码提交、固件 SDK 版本及 FAP API，并包含中英文 README 与 `docs/` 下的 Markdown 文档。GitHub Release 工作流使用固定版本的 SDK；准备好对应的 `docs/releases/v<版本>.md` 并推送 `v<版本>` 标签后，会自动构建并发布。

## 打包与 USB 上传

准备好本地曲目后，打包应用和转换后的曲目：

```sh
make package
# Momentum 构建：
# make package FIRMWARE=momentum
```

通过 USB 上传曲目、构建并启动应用：

```sh
make upload-songs
make launch
# Momentum 构建：
# make launch FIRMWARE=momentum
```

上传前退出设备上正在运行的应用，并关闭占用串口的程序。

SD 卡目录与手动安装步骤见[使用指南](usage.md)。

## 鼓声音效

将咚、咔原始音效分别放在 `assets/dong.ogg` 和 `assets/ka.ogg`，然后转换并上传：

```sh
make sfx
make upload-sfx
```

转换结果为 `sdcard/apps_data/taiko_player/sfx/dong.wav` 和 `ka.wav`，格式与默认歌曲音频相同。`make package` 也会准备音效。没有原始音效时转换脚本会跳过；设备缺少或无法加载音效时仍可播放伴奏。

音效在应用启动时载入内存；每个 WAV 最多 32768 个 PCM 样本。播放器按音符时间在 DMA 缓冲中混音，普通音符支持音效重叠，并在歌曲采样率不同时重采样。连打和气球的自动咚共用一个声部：下一次自动击打替换旧尾音，替换前按原曲时间做 3 ms 淡出，最后一击自然收尾，减少高速连打时的音效堆叠；普通咚、咔仍可独立叠加。暂停、跳转、重新播放沿用同一条音频时间轴。伴奏增益为 100%，咚、咔音量默认各为 70%，可在菜单中独立调节为 0–100%，每次调整 10%；0% 静音对应音效，返回播放后生效。超出范围的混合波形会限幅。1、3 号音符使用咚，2、4 号音符使用咔；连打和气球的自动击打使用咚音效及 Dong Volume 音量。

可生成一段 10 秒的无伴奏测试谱面，单独检查设备上的鼓声：

```sh
.venv/bin/python scripts/prepare_sfx.py --test-song
make upload-songs
make upload-sfx
```

在文件浏览器中选择 `Drum Sound Test/sound_test.tja`，会依次播放咚、咔及大音符，后半段加快。重新启动应用后，新上传的音效才会载入。

## 资源管理

播放结束后保留当前谱面供重播，回收音频播放对象、8 KiB 缓冲和工作线程；Restart 复用当前谱面，并先释放旧音频对象再重新播放。切换难度时先停止音频、释放旧谱面，再载入新难度，避免两份谱面同时占用内存。新难度解析失败时重新读取旧难度；若恢复也失败，则返回选曲界面并显示错误。

设备端的谱面分配会预先检查可用内存和最大连续空闲块，并为音频及界面保留 16 KiB 余量。内存不足时提示错误；单谱事件上限不代表任何大小的谱面都能装入设备内存。

音效样本和音频播放对象分配前也会检查内存余量；无法加载音效时保留伴奏播放，音频播放对象无法创建时显示错误并返回选曲。绘制文本缓冲在应用内复用，混音时跳过已经结束的事件，减少重复分配和扫描。

内存预检查不会为其他线程预留实际内存；它用于尽早拒绝内存不足或连续空闲块不足的分配。

## 开发与测试

```text
src/             C 源码与头文件
scripts/         曲目转换与测试入口
assets/          原始鼓声音效
tests/           本地测试和硬件接口模拟
docs/            使用、谱面、开发指南及版本记录
application.fam  Flipper 应用清单
Makefile         构建、测试、打包和上传命令
```

运行本地测试：

```sh
make test
```

测试覆盖 GOGO 开关时刻、小节内变化、OFFSET、DELAY、结束边界、跳转和难度隔离，以及连打/气球限速、全部播放倍速、变速与相邻长音边界、限速后的计数和混音、连打/气球次数、后续音符中断及同一时刻不重复击打、BPM 变更时的击打相位、长拖尾、跳转后的气球状态和跨缓冲混音一致性，以及谱面时间计算、异常输入、WAV 解析、轨道可见范围和时间跳转，菜单顺序、动作与独立间隔设置，音效混音、重叠、限幅、重采样、跨 DMA 缓冲和跳转一致性、独立鼓声音量与总音量，以及 GPIO 映射、脉冲定时、重触发和退出复位。本地 `songs/` 中有谱面时会一并解析检查，没有曲目也可运行核心测试。GPIO 测试使用模拟接口，不能替代设备和实际波形测量。

内存测试模拟可用内存不足和碎片化，检查扩容失败后的清理、释放后重试及播放速度变更失败时的回退。

播放时可以通过 USB 采集画面，观察滚动效果并统计帧缓冲流的实际帧率：

```sh
~/.ufbt/toolchain/current/bin/python3 scripts/device_preview.py \
  --port /dev/ttyACM0 --seconds 10 --output build/playback.png
```

该工具依赖 pyserial 和 Pillow（uFBT 工具链的 Python 环境中已包含），只读取画面，不发送按键或修改 GPIO。截图保存在 `build/`；统计的是 USB 帧缓冲流速率，不是 LCD 像素响应速度。

## 参考资料与项目

- [TJA 格式说明](https://gist.github.com/KatieFrogs/e000f406bbc70a12f3c34a07303eec8b)：音符、气球次数与谱面指令。
- [OpenTaiko NotesManager](https://github.com/0auBSQ/OpenTaiko/blob/main/OpenTaiko/src/Stages/07.Game/Taiko/NotesManager.cs)：谱面流速计算参考。
- [flipper-zero-video-player](https://github.com/LTVA1/flipper-zero-video-player)：最初调研的播放器参考项目。
- [Flipper Zero GPIO 文档](https://docs.flipper.net/zero/gpio-and-modules)：引脚定义与外部模块接口。
- [Flipper Zero uFBT](https://github.com/flipperdevices/flipperzero-ufbt)：应用构建工具。
- [Flipper Zero 固件](https://github.com/flipperdevices/flipperzero-firmware)：官方固件与开发接口。
- [Momentum 固件与 SDK](https://github.com/Next-Flip/Momentum-Firmware)：Momentum 兼容构建。
