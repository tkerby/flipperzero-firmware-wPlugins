# Flipper Zero Taiko Player

一个用于 Flipper Zero 的轻量 TJA 播放器。从 SD 卡选择谱面，使用内置扬声器播放配套音频，同时显示鼓点，并通过四路 GPIO 输出打击脉冲。

## 功能

- 浏览 SD 卡并选择 `.tja` 文件，扫描文件中的难度，优先选择 Oni，没有 Oni 时选择文件中的最高难度。
- 播放预先转换的单声道 PCM WAV 音频。
- 支持将咚、咔音效按谱面时间实时混入伴奏，普通和大音符共用对应音效，可重叠播放。
- 曲名下方显示单轨滚动谱面，音符由右向左移动；咚用实心白边圆，咔用空心黑边圆，3、4 号大音符放大显示。
- 鼓面右侧固定显示当前谱面 BPM 与流速倍率（Speed），BPM 随谱面变速和播放位置更新，不乘播放速度。
- 左下角的小鼓用同心圆表示鼓面与鼓边，中央竖线分隔左右；第一拍从右侧开始，随后按音符左右交替。
- 鼓面填充表示红音符（咚，1、3），鼓边填充表示蓝音符（咔，2、4）；小鼓每次只显示最新的一拍，右下角显示播放时间和难度。
- 将左蓝、左红、右红、右蓝映射到四路 GPIO，输出 70 ms 高电平脉冲。
- 底部提供 BWD、Play/Pause、FWD 控制，支持暂停和可调间隔的前后跳转；长按返回退出。
- 上下键调节应用内总音量，播放界面最右侧的柱状条显示当前音量，不修改系统全局音量。
- 短按返回打开菜单，可返回播放、重新播放、播放新曲、分别设置前后跳转间隔和鼓声音量或退出。

休止符与长音结束标记不切换左右手。长音和气球目前只在起点触发一次 GPIO，不叠加鼓声音效。

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

## 鼓声音效

将咚、咔原始音效分别放在 `assets/dong.ogg` 和 `assets/ka.ogg`，然后转换并上传：

```sh
make sfx
make upload-sfx
```

转换结果为 `sdcard/apps_data/taiko_player/sfx/dong.wav` 和 `ka.wav`，格式与默认歌曲音频相同。`make package` 也会准备音效。没有原始音效时转换脚本会跳过；设备缺少或无法加载音效时仍可播放伴奏。

音效在应用启动时载入内存；每个 WAV 最多 32768 个 PCM 样本。播放器按音符时间在 DMA 缓冲中混音，支持音效重叠，并在歌曲采样率不同时重采样。暂停、跳转、重新播放沿用同一条音频时间轴。伴奏增益为 100%，咚、咔音量默认各为 70%，可在菜单中独立调节为 0–100%，每次调整 10%；0% 静音对应音效，返回播放后生效。超出范围的混合波形会限幅。1、3 号音符使用咚，2、4 号音符使用咔，连打与气球不生成音效。

总音量在混音后统一作用于伴奏和鼓声，默认 70%，范围为 0–100%，每次调整 10%，0% 静音但不停止谱面或 GPIO。播放界面按上/下键增减音量，也支持按住连续调整；暂停时同样可调，菜单中的上下键仍用于选择项目。音量在后续音频缓冲中生效，可能有短暂延迟。设置仅在本次运行期间保留，退出应用后恢复默认。PCM 播放直接驱动扬声器，不经过系统通知音量处理，也不读取或修改系统音量设置。

可生成一段 10 秒的无伴奏测试谱面，单独检查设备上的鼓声：

```sh
.venv/bin/python scripts/prepare_sfx.py --test-song
make upload-songs
make upload-sfx
```

在文件浏览器中选择 `Drum Sound Test/sound_test.tja`，会依次播放咚、咔及大音符，后半段加快。重新启动应用后，新上传的音效才会载入。

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
apps_data/taiko_player/songs/
  Example Song/
    chart.tja
    song.wav
apps_data/taiko_player/sfx/
  dong.wav
  ka.wav
```

通过 USB 上传曲目、构建并启动应用：

```sh
make upload-songs
make launch
# Momentum 构建：
# make launch FIRMWARE=momentum
```

上传前退出设备上正在运行的应用，并关闭占用串口的程序。

在 Flipper 的应用菜单中打开 **Media → Taiko Player**。选曲默认打开 `/ext/apps_data/taiko_player/songs` 目录，在其中选择谱面。播放器根据 `WAVE:` 查找对应 WAV，因此保留原始 TJA 时，也可以将同名 WAV 放到其指定位置。

| 操作 | 按键 |
| --- | --- |
| 浏览、选择曲目 | 方向键、OK |
| 文件浏览器返回上级目录 | 短按返回 |
| 播放 / 暂停 | OK |
| 后退 / 前进（默认 5 秒） | 左 / 右 |
| 增加 / 减少应用总音量 | 上 / 下（可按住连续调整） |
| 打开 / 关闭播放菜单 | 短按返回 |
| 菜单中选择项目 | 上 / 下 |
| 菜单中修改难度、速度、间隔、音量 | 左 / 右（OK 切换或增加） |
| 执行菜单项目 | OK |
| 播放时退出应用 | 长按返回 |
| 播放结束后从头重播 | OK |

暂停时，音频、谱面与鼓点指示一起停止，并清空 GPIO 输出。暂停期间也可前后跳转，按 OK 后从当前位置继续播放。跳转会限制在曲目起止范围内，不补发跳过的鼓点；左右手交替仍按谱面中的原始音符顺序计算。恢复和跳转时需要重新读取音频缓冲，可能有短暂延迟。

播放菜单从上到下为 **Return（返回）、Restart（重新播放）、Play New Song（播放新曲）、Difficulty（难度）、Playback Speed（播放速度）、Scroll Speed（谱面流速）、Backward Interval（后退间隔）、Forward Interval（前进间隔）、Dong Volume（咚音量）、Ka Volume（咔音量）、Exit（退出应用）**。菜单每屏显示五行，上下选择时自动滚动。前后间隔分别支持 **1 / 3 / 5 / 10 秒**，默认均为 5 秒；音量默认各为 70%。修改在本次应用运行期间保留，退出后恢复默认。打开菜单会暂停音频并清空 GPIO，选择 Return 或短按返回时恢复进入菜单前的播放或暂停状态；Restart 从头开始播放；Play New Song 停止当前曲目并回到 `taiko_player/songs/` 目录重新选曲，保留本次运行中的音量和跳转间隔设置，选定后从头播放。取消选曲会退出应用。播放结束时进入菜单再返回，仍保留结束状态。

难度选项只列出当前 TJA 中存在的 COURSE，支持名称、数字编号和 Ura（Edit）别名；优先 Oni，否则按 COURSE 顺序选最高难度。返回播放或选择 Restart 时应用难度变更，并从头开始；原本暂停时返回仍保持暂停。文件中尚未选择的难度不会同时载入音符数组。

播放速度支持 **0.5～2.0 倍，每档 0.1**；谱面流速支持 **0.5～2.5 倍（每档 0.1），以及 3.0、4.0 倍**。两者默认均为 1.0 倍，独立设置并在本次运行期间保留。播放速度同步改变音频、鼓声与 GPIO 的节奏，音高随速度变化，播放时间显示原曲位置；修改速度保留当前位置。流速只改变音符显示距离，画面实际移动速度为基准流速乘以播放速度和流速倍率，不改变音频或 GPIO。换曲时重新选择默认难度，速度设置保留。

## GPIO 输出

| 鼓点 | GPIO | 排针编号 |
| --- | --- | --- |
| 左蓝（咔） | A7 / PA7 | 2 |
| 左红（咚） | A6 / PA6 | 3 |
| 右红（咚） | A4 / PA4 | 4 |
| 右蓝（咔） | B3 / PB3 | 5 |

输出为 **3.3 V 逻辑信号**，接收端应与 Flipper 的 GND 共地。引脚定义见[官方 GPIO 文档](https://docs.flipper.net/zero/gpio-and-modules)。

每个音符输出一个 **70 ms 高电平脉冲**，空闲为低电平。四路独立计时，不同路的脉冲可以重叠；同一路重复触发时，先拉低再拉高，并重新计时。软件定时器的实际脉宽会有少量调度抖动。

暂停、跳转、播放结束、出错或退出时，清空所有输出；应用退出后引脚恢复为下拉输入。音频只输出到内置扬声器，不占用这四路 GPIO。

映射位于 [src/taiko_gpio.c](src/taiko_gpio.c)，脉宽由 [src/taiko_gpio.h](src/taiko_gpio.h) 中的 `TAIKO_HIT_MS` 控制。

## 支持范围与限制

- 支持 `BPM`、`OFFSET`、`#BPMCHANGE`、`#MEASURE`、非负 `#DELAY`、跨行小节、注释和空小节。
- 忽略 `#SCROLL`、GOGO、显示小节线等视觉命令；暂不支持分歧谱和段位串烧。
- 单谱最多 8192 个事件，单小节最多 2048 格，单行最多 2047 字节，时长最多一小时。
- 支持 8000–48000 Hz、8 位无符号、单声道 PCM WAV；不在设备上直接解码 OGG。
- 音频流式读取，使用 8 KiB 循环缓冲。
- 谱面轨道固定占用 17 像素高度，基准流速为每谱面秒 128 像素，目标刷新率为 50 FPS；位置按播放时间计算，只收集可见音符，每帧最多显示 64 个。视觉刷新与鼓点检查分开调度。
- 轨道暂时只绘制普通咚、咔及大音符，不绘制 GOGO、连打、气球或小节线，也不按 `#SCROLL` 改变速度。
- 暂不支持打击判定和精准同步；不提供保持音高的时间伸缩。
- 曲名显示取决于固件字体支持，音乐音质受内置扬声器限制。

## 开发与测试

```text
src/             C 源码与头文件
scripts/         曲目转换与测试入口
assets/          原始鼓声音效
tests/           本地测试和硬件接口模拟
application.fam  Flipper 应用清单
Makefile         构建、测试、打包和上传命令
```

运行本地测试：

```sh
make test
```

测试覆盖谱面时间计算、异常输入、WAV 解析、轨道可见范围和时间跳转，菜单顺序、动作与独立间隔设置，音效混音、重叠、限幅、重采样、跨 DMA 缓冲和跳转一致性、独立鼓声音量与总音量，以及 GPIO 映射、脉冲定时、重触发和退出复位。本地 `songs/` 中有谱面时会一并解析检查，没有曲目也可运行核心测试。GPIO 测试使用模拟接口，不能替代设备和实际波形测量。

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
