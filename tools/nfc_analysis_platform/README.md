# NFC Analysis Platform

NFC Analysis Platform是一个用于分析Flipper Zero NFC应用程序数据的综合工具。该平台提供了多种工具，用于NFC数据分析，包括：

- APDU响应解码器 (nard)
- TLV解析器 (tlv)

## 安装

### 前提条件

- Go 1.21或更高版本
- Make (可选，用于简化构建过程)

### 使用Make构建和安装

```bash
# 初始化项目
make init

# 构建项目
make build

# 安装到~/bin目录
make install
```

### 手动构建和安装

```bash
# 初始化Go模块
go mod tidy

# 构建项目
go build -o nfc_analysis_platform .

# 安装到~/bin目录
mkdir -p ~/bin
cp nfc_analysis_platform ~/bin/
mkdir -p ~/bin/format
cp -r format/* ~/bin/format/ 2>/dev/null || :
```

确保`~/bin`目录在您的PATH环境变量中。

## 使用方法

### APDU响应解码器 (nard)

APDU响应解码器用于解析和显示来自Flipper Zero NFC APDU Runner应用程序的.apdures文件。

```bash
# 从本地文件加载.apdures文件
nfc_analysis_platform nard --file path/to/file.apdures

# 从Flipper Zero设备加载.apdures文件
nfc_analysis_platform nard --device

# 使用串口通信从Flipper Zero设备加载.apdures文件
nfc_analysis_platform nard --device --serial

# 指定串口
nfc_analysis_platform nard --device --serial --port /dev/ttyACM0

# 指定解码格式模板
nfc_analysis_platform nard --file path/to/file.apdures --decode-format path/to/format.apdufmt

# 启用调试模式
nfc_analysis_platform nard --file path/to/file.apdures --debug
```

### TLV解析器 (tlv)

TLV解析器用于解析TLV（Tag-Length-Value）数据并提取指定标签的值。

```bash
# 解析所有标签
nfc_analysis_platform tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264

# 解析特定标签
nfc_analysis_platform tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 84,50

# 指定数据类型
nfc_analysis_platform tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 50 --type ascii
```

## 功能特点

### APDU响应解码器 (nard)

- 支持自定义解码格式模板
- 支持从本地文件或Flipper Zero设备加载.apdures文件
- 支持串口通信
- 支持调试模式，显示详细错误信息
- 支持十六进制到十进制的转换
- 支持数学运算
- 支持函数结果切片

### TLV解析器 (tlv)

- 支持解析TLV数据
- 支持递归解析嵌套TLV结构
- 支持提取特定标签的值
- 支持多种数据类型显示（十六进制、ASCII、UTF-8、数字）
- 支持彩色输出，提高可读性

## 贡献

欢迎贡献代码、报告问题或提出改进建议。请通过GitHub Issues或Pull Requests提交您的贡献。

## 许可证

本项目采用MIT许可证。详情请参阅LICENSE文件。 