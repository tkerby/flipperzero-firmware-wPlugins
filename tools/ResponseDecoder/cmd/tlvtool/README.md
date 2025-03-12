# TLV工具

TLV工具是一个用于解析TLV（Tag-Length-Value）数据结构的命令行工具。它可以解析十六进制格式的TLV数据，并提取指定标签的值。

## 功能特点

- 解析十六进制格式的TLV数据
- 支持提取指定标签的值
- 支持多种数据类型的解码（UTF-8、ASCII、数字）
- 支持递归解析嵌套的TLV结构
- 彩色输出，易于阅读

## 安装

### 从源代码构建

```bash
# 克隆仓库
git clone https://github.com/spensercai/nfc_apdu_runner.git
cd nfc_apdu_runner/tools/ResponseDecoder/cmd/tlvtool

# 安装依赖
make deps

# 构建
make build
```

### 跨平台构建

```bash
# 构建所有平台版本
make build-all

# 或者单独构建特定平台版本
make build-linux
make build-windows
make build-mac
```

## 使用方法

### 解析所有标签

```bash
./tlvtool --hex 6F198407A0000000031010A50E500A4D617374657243617264
```

### 解析指定标签

```bash
./tlvtool --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 84,50
```

### 指定数据类型

```bash
./tlvtool --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 50 --type ascii
```

## 命令行参数

- `--hex`: 要解析的十六进制TLV数据（必需）
- `--tag`: 要提取的标签列表，用逗号分隔（可选）
- `--type`: 数据类型（可选，支持utf8、ascii、numeric）

## 示例

### 解析EMV数据

```bash
./tlvtool --hex 6F3A8407A0000000031010A52F500A4D6173746572436172649F38069F5C089F4D029F6E07BF0C089F5A0551031000009F0A0800010501000000
```

### 提取应用标签和AID

```bash
./tlvtool --hex 6F3A8407A0000000031010A52F500A4D6173746572436172649F38069F5C089F4D029F6E07BF0C089F5A0551031000009F0A0800010501000000 --tag 50,84 --type ascii
```

## 许可证

GNU通用公共许可证第3版 (GPL-3.0) 