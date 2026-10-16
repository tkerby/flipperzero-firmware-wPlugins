# 应用商店提交

[项目首页](../README.md) · [使用指南](usage.md) · [开发指南](development.md)

## 提交资料

- 应用信息：application.fam，ID 为 taiko_player，分类 Media，目标 F7，待提交版本 0.4。
- 项目许可：[GPL-3.0-or-later](../LICENSE)。代码及原创图标适用；第三方音效保留 MIT。
- 音效：[来源、固定提交及校验值](../assets/SFX-NOTICE.md)，以及[上游 MIT 原文](../assets/SFX-LICENSE.txt)。
- 商店英文介绍：[catalog-description.md](catalog-description.md)。只使用商店支持的标题、段落、列表及链接。
- 商店更新日志：[changelog.md](changelog.md)。独立于包含表格的 GitHub Release 说明。
- 截图：[playing.png](../screenshots/playing.png)，原样复制作者提供的文件，512×256 PNG，仅橙色 (254,138,44) 与黑色。
- [manifest 模板](https://github.com/ZeroNinx/flipper-zero-taiko-player/blob/main/catalog/manifest.yml.in)和 [PR 草稿](https://github.com/ZeroNinx/flipper-zero-taiko-player/blob/main/catalog/pull-request.md)位于 catalog/。

以官方[贡献指南](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md)、[Manifest 格式](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Manifest.md)及 [AGENTS.md](https://github.com/flipperdevices/flipper-application-catalog/blob/main/AGENTS.md) 为准。

## 安装后的资源

应用商店从源码构建 FAP，不执行本项目的 Release ZIP 打包流程。因此首次安装仅获得应用：歌曲和可选鼓声需在电脑上转换，再复制到应用数据目录。没有鼓声文件时仍可播放伴奏。商店介绍明确说明了这一点，并链接到英文准备步骤。

GPIO 默认关闭，内置扬声器播放无需额外硬件；可选 GPIO 使用 3.3 V 逻辑信号并需要共地。提交到商店的是官方固件构建，不需要另交 Momentum 包。

## 准备校验环境

以下命令在项目根目录执行，使用独立虚拟环境安装目录工具依赖：

```sh
mkdir -p build/catalog-submission
git clone --depth 1 https://github.com/flipperdevices/flipper-application-catalog.git build/catalog-submission/catalog
python3 -m venv build/catalog-submission/venv
build/catalog-submission/venv/bin/python -m pip install -r build/catalog-submission/catalog/tools/requirements.txt
```

目录工具会初始化或更新官方 uFBT SDK，然后拉取 manifest 指定的源码提交、编译应用并检查描述、截图等资料。若目录已经克隆或虚拟环境已经存在，可复用它们。

## 生成正式 manifest

先审阅并提交、推送本项目的源码和资料，再生成 manifest。不可使用未提交文件对应的假 SHA、短 SHA、分支名或标签代替提交哈希。

下面的命令不会创建提交或推送；它要求工作区干净，再将当前 HEAD 写入生成文件：

```sh
python3 - <<'PY'
from pathlib import Path
import subprocess

if subprocess.check_output(['git', 'status', '--porcelain']):
    raise SystemExit('Commit and push source changes before generating the submission manifest.')
sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
manifest = Path('catalog/manifest.yml.in').read_text().replace('@SOURCE_COMMIT@', sha)
output = Path('build/catalog-submission/catalog/applications/Media/taiko_player/manifest.yml')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(manifest)
print(output)
print('Source commit:', sha)
PY
```

模板中的应用名称、ID、版本、作者、图标及短描述从 application.fam 读取，避免两处信息不一致。最终交给目录仓库的文件必须命名为 **manifest.yml**，模板文件不直接提交给目录仓库。

## 官方目录校验

仍在项目根目录执行：

```sh
PATH="$PWD/build/catalog-submission/venv/bin:$PATH" \
  build/catalog-submission/venv/bin/python build/catalog-submission/catalog/tools/bundle.py \
  --nolint build/catalog-submission/catalog/applications/Media/taiko_player/manifest.yml \
  build/catalog-submission/bundle.zip
```

这个命令保留完整编译和资料检查，只跳过源码格式检查。移除 --nolint 可额外运行目录的源码格式检查。

开发期间可在本地 Git 快照上先运行同样的检查；本地快照的通过只证明对应内容通过工具校验，不能代替最终公开提交的再次校验。正式 PR 必须引用公开 GitHub 仓库中可拉取的提交。

## 发起目录 PR

Fork 官方目录仓库，建立单独分支，将生成的文件放到 applications/Media/taiko_player/manifest.yml。PR 标题可使用 **Add Taiko Player v0.4**，正文以 catalog/pull-request.md 为基础，保留上游模板的所有章节和 Reviewer Checklist。

PR 草稿保留未勾选的作者检查项，正式提交者应在审阅和校验后逐项确认。AI 声明使用 Fully AI generated，并解释解析、音频、UI、GPIO、设置和测试的作用；作者负责需求及实机反馈，原始截图由作者提供，鼓声来自明确标注的第三方资源。

当前官方 1.5.1 SDK 编译检查与 Momentum 实机验证分开记录；不要将 Momentum 上的结果写成官方固件实机验证。若方便，正式提交前再在官方固件设备上验收。
