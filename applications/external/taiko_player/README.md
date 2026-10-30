# Flipper Zero Taiko Player

[简体中文](README.md) | [English](README.en.md)

<img src="assets/taiko_player.svg" width="80" height="80" alt="Taiko Player logo">

一个用于 Flipper Zero 的轻量 TJA 播放器：从 SD 卡选曲，通过内置扬声器播放伴奏与鼓声，同时显示滚动谱面，还可输出四路 GPIO 鼓点脉冲。

<img width="512" height="256" alt="Taiko Player 播放界面" src="screenshots/playing.png" />

## 功能

- 支持普通／大咚咔、连打、气球、分段流速（`#SCROLL`）及 GOGO TIME。
- 按谱面自动混入咚／咔音效，显示左右鼓面指示。
- 支持选难度、暂停、前后跳转，以及播放速度、流速、刷新率和音量调节。
- 可自定义四路 GPIO 输出，默认关闭；常用设置自动保存。
- 提供官方固件及新旧 Momentum 安装包。

这是谱面播放器，暂不提供打击判定；OGG 需要先在电脑上转换为 PCM WAV。

## 快速开始

1. 从 [GitHub Releases](https://github.com/ZeroNinx/flipper-zero-taiko-player/releases) 下载与设备固件兼容的安装包。固件 SDK 与 API 对应关系见[使用指南](docs/usage.md#安装与升级)。
2. 解压，将 `sdcard/` 内的内容复制到 SD 卡根目录。包内含应用、鼓声音效和转码脚本，不含歌曲及个人设置。
3. 在电脑上安装 Python 3 和 FFmpeg。把自己的 TJA 与配套音频放入解压目录的 `songs/`，保留谱面 `WAVE:` 指定的文件名，然后在解压目录运行：

   ```sh
   python3 scripts/prepare_songs.py songs --output sdcard/apps_data/taiko_player/songs
   ```

4. 将生成的 `sdcard/apps_data/taiko_player/songs/` 复制到 SD 卡同名目录。
5. 在 Flipper 上打开 **Media → Taiko Player**，选择 `.tja` 文件播放。

升级可覆盖应用和音效，保留原有歌曲与 `settings.ff`。安装发布包无需搭建编译环境。

## 基本操作

| 按键 | 功能 |
| --- | --- |
| OK | 播放／暂停，结束后重播 |
| 左／右 | 后退／前进，默认各 5 秒 |
| 上／下 | 调节应用总音量 |
| 短按返回 | 打开菜单／返回 |
| 播放时长按返回 | 退出应用 |

## 文档

- [使用指南](docs/usage.md)：安装、曲目转换、菜单、音量、GPIO 和设置保存。
- [谱面支持](docs/chart-support.md)：音符显示、HS、连打与气球、GOGO TIME 和格式限制。
- [开发指南](docs/development.md)：编译、测试、打包、资源管理及参考项目。
- [版本记录](https://github.com/ZeroNinx/flipper-zero-taiko-player/releases)：各版本更新说明与下载。
- [应用商店提交](docs/catalog-submission.md)：商店资料、校验和提交步骤。

## 从源码构建

需要 Python 3、Make 和 FFmpeg。完整环境要求及 Momentum 构建步骤见[开发指南](docs/development.md)。

```sh
git clone https://github.com/ZeroNinx/flipper-zero-taiko-player.git
cd flipper-zero-taiko-player
make setup
make build
```

## 致谢

默认咚／咔音效来自 TJAPlayer3 的 SimpleStyle 皮肤，使用 MIT 许可，见[来源及许可说明](assets/SFX-NOTICE.md)。

## 许可证

本项目代码和原创图标采用 [GPL-3.0-or-later](LICENSE)。第三方鼓声音效保留其 MIT 许可及版权声明。

## AI 协作

本项目由作者与 **GPT-6.1-sol** 协作完成。AI 参与代码实现、测试与文档编写，作者负责需求、方案确认和实机体验反馈。

让一份 TJA 谱面，在一只小海豚上响起来。
