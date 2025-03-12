# APDU Response Decoder

APDU Response Decoder是一个用于解析和显示Flipper Zero NFC APDU Runner应用程序生成的`.apdures`文件的工具。

## 功能特点

- 支持从本地文件加载`.apdures`文件
- 支持直接从Flipper Zero设备加载`.apdures`文件（通过SD卡挂载或串口通信）
- 支持自定义解码格式模板
- TLV（Tag-Length-Value）数据解析功能
- 自动检测和报告失败的APDU命令
- 美观的彩色输出

## 安装

### 安装依赖

```bash
go get go.bug.st/serial
go get go.bug.st/serial/enumerator
go get github.com/spf13/cobra
go get github.com/fatih/color
```

### 构建

```bash
go build -o response_decoder
```

## 使用方法

### 基本命令

```
用法:
  response_decoder [flags]
  response_decoder [command]

可用命令:
  help        查看帮助信息
  tlv         解析TLV数据并提取指定标签的值

参数:
      --decode-format string   格式模板文件路径 (.apdufmt)
  -d, --device                 连接Flipper Zero设备
  -f, --file string            .apdures文件路径
      --format-dir string      包含格式模板文件的目录 (默认 "./format")
  -h, --help                   查看帮助信息
  -p, --port string            指定Flipper Zero的串口 (可选)
  -s, --serial                 使用串口与Flipper Zero通信 (需要 -d)
      --debug                  启用调试模式以显示错误信息
```

### 从本地文件加载

从本地存储解析APDU响应文件：

```bash
./response_decoder -f path/to/file.apdures
```

### 从Flipper Zero设备加载（SD卡挂载）

使用SD卡挂载直接从Flipper Zero设备加载并解析APDU响应文件：

```bash
./response_decoder -d
```

这将自动检测Flipper Zero设备的挂载点，并列出可用的`.apdures`文件供您选择。

### 从Flipper Zero设备加载（串口通信）

使用串口通信直接从Flipper Zero设备加载并解析APDU响应文件：

```bash
./response_decoder -d -s
```

如果需要指定串口（当连接多个设备时很有用）：

```bash
./response_decoder -d -s -p /dev/tty.usbmodem123456
```

### 使用自定义解码格式

使用特定的格式模板进行解码：

```bash
./response_decoder -f file.apdures --decode-format path/to/format.apdufmt
```

如果不指定格式文件，程序将列出格式目录中所有可用的`.apdufmt`文件，并提示您选择一个。

### TLV数据解析

该工具包含一个独立的TLV解析器，可以单独使用：

```bash
./response_decoder tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264
```

提取特定标签：

```bash
./response_decoder tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 84,50
```

指定数据类型以获得更好的显示效果：

```bash
./response_decoder tlv --hex 6F198407A0000000031010A50E500A4D617374657243617264 --tag 50 --type ascii
```

## 解码格式模板

解码格式模板是一个文本文件，用于定义如何解析和显示APDU响应数据。模板文件的第一行是格式名称，后续行是解码规则。

例如，PBOC格式模板：

```
PBOC
卡号: {hex(O[1][10:18])}
姓名: {hex(O[2][10:20], "utf-8")}
余额: {hex(O[3][10:18])} 元
有效期: 20{O[4][10:12]}-{O[4][12:14]}
交易记录: {hex(O[5][10:30])}
```

### 模板语法

- `I[n]`: 第n个输入命令
- `O[n]`: 第n个输出响应
- `hex(data)`: 将十六进制字符串转换为可读文本
- `hex(data, "utf-8")`: 将十六进制字符串按UTF-8编码转换为文本
- `O[n]TAG(tag)`: 从第n个输出响应中提取指定TLV标签的值
- `O[n]TAG(tag, "encoding")`: 从第n个输出响应中提取指定TLV标签的值，并按指定编码转换为文本

### 错误处理

该工具自动检测错误响应（非9000状态码）并适当处理：

- 当输出具有非成功状态码（不是9000、9100或9200）时，任何引用该输出的模板表达式将返回空字符串
- 解析过程中会收集错误信息，但默认不显示
- 要查看错误信息，请在运行命令时使用`--debug`参数
- 在调试模式下，错误信息会在输出结束后显示，包括输出索引、状态码和错误描述（例如，"Output[2]: Error code 6A82 - File not found"）

### TLV解析

本工具支持解析EMV和其他智能卡中常用的TLV（Tag-Length-Value）数据结构。TLV解析功能允许您直接从APDU响应中提取特定标签的值，而无需手动计算偏移量。

支持的编码类型：
- `utf-8`: UTF-8编码
- `ascii`: ASCII编码
- `numeric`: 数字编码（如BCD码）

#### TLV模板示例

EMV格式模板：

```
EMV卡片信息
卡号: {O[1]TAG(5A), "numeric"}
持卡人姓名: {O[2]TAG(5F20), "utf-8"}
有效期: {O[2]TAG(5F24), "numeric"}
货币代码: {O[3]TAG(9F42)}
余额: {O[3]TAG(9F79)}
交易日志: {O[4]TAG(9F4D)}
应用标签: {O[0]TAG(50), "ascii"}
```

## 使用示例

### 示例1：使用默认格式解析本地文件

```bash
./response_decoder -f ./responses/visa_card.apdures
```

### 示例2：使用自定义格式从Flipper Zero解析文件

```bash
./response_decoder -d -s --decode-format ./format/EMV.apdufmt
```

### 示例3：解析TLV数据

```bash
./response_decoder tlv --hex 6F1A8407A0000000031010A50F500A4D617374657243617264870101 --tag 50,84 --type ascii
```

## 许可证

GNU通用公共许可证第3版 (GPL-3.0) 