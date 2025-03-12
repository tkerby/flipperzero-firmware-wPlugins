# APDU Response Decoder

APDU Response Decoder是一个用于解析和显示Flipper Zero NFC APDU Runner应用程序生成的`.apdures`文件的工具。

## 功能特点

- 支持从本地文件加载`.apdures`文件
- 支持直接从Flipper Zero设备加载`.apdures`文件（通过SD卡挂载或串口通信）
- 支持自定义解码格式模板
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

### 从本地文件加载

```bash
./response_decoder -f path/to/file.apdures
```

### 从Flipper Zero设备加载（SD卡挂载）

```bash
./response_decoder -d
```

### 从Flipper Zero设备加载（串口通信）

```bash
./response_decoder -d -s
```

如果需要指定串口：

```bash
./response_decoder -d -s -p /dev/tty.usbmodem123456
```

### 使用自定义解码格式

```bash
./response_decoder -f file.apdures --decode-format path/to/format.apdufmt
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

## 示例

查看`examples`目录中的示例文件和脚本。

## 许可证

GNU通用公共许可证第3版 (GPL-3.0) 