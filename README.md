# Flipper Zero Taiko Player

一个用于 Flipper Zero 的轻量 TJA 播放器。从 SD 卡选择谱面，使用内置扬声器播放配套音频，同时显示鼓点，并通过四路 GPIO 输出打击脉冲。

## 功能

- 浏览 SD 卡并选择 `.tja` 文件，自动读取文件中的第一份谱面。
- 播放预先转换的单声道 PCM WAV 音频。
- 曲名下方显示单轨滚动谱面，音符由右向左移动；咚用实心白边圆，咔用空心黑边圆，3、4 号大音符放大显示。
- 左下角的小鼓用同心圆表示鼓面与鼓边，中央竖线分隔左右；第一拍从右侧开始，随后按音符左右交替。
- 鼓面填充表示红音符（咚，1、3），鼓边填充表示蓝音符（咔，2、4）；小鼓每次只显示最新的一拍，右下角显示播放时间和难度。
- 将左蓝、左红、右红、右蓝映射到四路 GPIO，输出 70 ms 高电平脉冲。
- 播放时长按返回退出，播放结束后按 OK 重新选曲。

休止符与长音结束标记不切换左右手。长音和气球目前只在起点触发一次，不叠加鼓声音效。

## 构建

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

Momentum SDK 地址由 [Makefile](Makefile) 中的 `MOMENTUM_SDK` 指定。若默认版本与固件不匹配，可在安装 SDK 时覆盖地址：

```sh
make momentum-sdk MOMENTUM_SDK="<对应固件的 SDK ZIP URL>"
```

后续的打包和上传命令也需要加上 `FIRMWARE=momentum`。构建操作不更新设备固件。

## 准备曲目

将自己的 TJA 和对应音频放入本地 `songs/` 目录，音频文件名由谱面的 `WAVE:` 字段指定。例如：

```text
songs/
  Example Song/
    chart.tja
    song.ogg
```

转换音频并生成 SD 卡目录：

```sh
make songs
```

也可以直接调用转换脚本：

```sh
.venv/bin/python scripts/prepare_songs.py songs --sample-rate 22050
```

默认转换为 **22050 Hz、8 位无符号、单声道 PCM WAV**。脚本支持读取 UTF-8、Shift-JIS 和 GB18030 编码的 TJA，输出 UTF-8 副本，并将副本中的 `WAVE:` 改为 WAV 文件名。原始文件保留不变；格式正确且未过期的 WAV 会跳过转换，使用 `--force` 可强制重新转换。

曲目资源不包含在仓库中，`songs/` 和生成的 `sdcard/` 均已加入 `.gitignore`。

## 安装与使用

准备好本地曲目后，打包应用和转换后的曲目：

```sh
make package
# Momentum 构建：
# make package FIRMWARE=momentum
```

将生成的 `sdcard/` 目录内的内容复制到 SD 卡根目录，也可以通过 qFlipper 文件管理上传。目录结构为：

```text
apps/Media/taiko_player.fap
apps_data/taiko_player/Songs/
  Example Song/
    chart.tja
    song.wav
```

通过 USB 上传曲目、构建并启动应用：

```sh
make upload-songs
make launch
# Momentum 构建：
# make launch FIRMWARE=momentum
```

上传前退出设备上正在运行的应用，并关闭占用串口的程序。

在 Flipper 的应用菜单中打开 **Media → Taiko Player**。默认曲目路径为 `/ext/apps_data/taiko_player/Songs`，也可浏览 SD 卡上的其他目录。播放器根据 `WAVE:` 查找对应 WAV，因此保留原始 TJA 时，也可以将同名 WAV 放到其指定位置。

| 操作 | 按键 |
| --- | --- |
| 浏览、选择曲目 | 方向键、OK |
| 文件浏览器返回上级目录 | 短按返回 |
| 播放时退出应用 | 长按返回 |
| 播放结束后重新选曲 | OK |

## GPIO 输出

| 鼓点 | GPIO | 排针编号 |
| --- | --- | --- |
| 左蓝（咔） | A7 / PA7 | 2 |
| 左红（咚） | A6 / PA6 | 3 |
| 右红（咚） | A4 / PA4 | 4 |
| 右蓝（咔） | B3 / PB3 | 5 |

输出为 **3.3 V 逻辑信号**，接收端应与 Flipper 的 GND 共地。引脚定义见[官方 GPIO 文档](https://docs.flipper.net/zero/gpio-and-modules)。

每个音符输出一个 **70 ms 高电平脉冲**，空闲为低电平。四路独立计时，不同路的脉冲可以重叠；同一路重复触发时，先拉低再拉高，并重新计时。软件定时器的实际脉宽会有少量调度抖动。

播放结束、出错或退出时，清空所有输出；应用退出后引脚恢复为下拉输入。音频只输出到内置扬声器，不占用这四路 GPIO。

映射位于 [src/taiko_gpio.c](src/taiko_gpio.c)，脉宽由 [src/taiko_gpio.h](src/taiko_gpio.h) 中的 `TAIKO_HIT_MS` 控制。

## 支持范围与限制

- 支持 `BPM`、`OFFSET`、`#BPMCHANGE`、`#MEASURE`、非负 `#DELAY`、跨行小节、注释和空小节。
- 忽略 `#SCROLL`、GOGO、显示小节线等视觉命令；暂不支持分歧谱和段位串烧。
- 单谱最多 8192 个事件，单小节最多 2048 格，单行最多 2047 字节，时长最多一小时。
- 支持 8000–48000 Hz、8 位无符号、单声道 PCM WAV；不在设备上直接解码 OGG。
- 音频流式读取，使用 8 KiB 循环缓冲。
- 谱面轨道固定占用 17 像素高度，以 128 像素/秒滚动，目标刷新率为 50 FPS；位置按播放时间计算，只收集可见音符，每帧最多显示 64 个。视觉刷新与鼓点检查分开调度。
- 轨道暂时只绘制普通咚、咔及大音符，不绘制 GOGO、连打、气球或小节线，也不按 `#SCROLL` 改变速度。
- 暂不支持难度选择、打击判定、精准同步、暂停、进度跳转和音量设置。
- 曲名显示取决于固件字体支持，音乐音质受内置扬声器限制。

## 开发与测试

```text
src/             C 源码与头文件
scripts/         曲目转换与测试入口
tests/           本地测试和硬件接口模拟
application.fam  Flipper 应用清单
Makefile         构建、测试、打包和上传命令
```

运行本地测试：

```sh
make test
```

测试覆盖谱面时间计算、异常输入、WAV 解析、轨道可见范围和时间跳转，以及 GPIO 映射、脉冲定时、重触发和退出复位。本地 `songs/` 中有谱面时会一并解析检查，没有曲目也可运行核心测试。GPIO 测试使用模拟接口，不能替代设备和实际波形测量。

播放时可以通过 USB 采集画面，观察滚动效果并统计帧缓冲流的实际帧率：

```sh
~/.ufbt/toolchain/current/bin/python3 scripts/device_preview.py \
  --port /dev/ttyACM0 --seconds 10 --output build/playback.png
```

该工具依赖 pyserial 和 Pillow（uFBT 工具链的 Python 环境中已包含），只读取画面，不发送按键或修改 GPIO。截图保存在 `build/`；统计的是 USB 帧缓冲流速率，不是 LCD 像素响应速度。

## 参考

- [Flipper Zero uFBT](https://github.com/flipperdevices/flipperzero-ufbt)
- [Flipper Zero 固件](https://github.com/flipperdevices/flipperzero-firmware)
- [Momentum 固件与 SDK](https://github.com/Next-Flip/Momentum-Firmware)

## AI 协作

本项目由作者与 **GPT-6.1-sol** 协作完成。AI 参与了代码实现、测试与文档编写，作者负责提出需求、确认方案和提供实机体验反馈。

让一份 TJA 谱面，在一只小海豚上响起来。
