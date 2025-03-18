<!--
 * @Author: SpenserCai
 * @Date: 2025-03-15 16:52:39
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-16 00:55:11
 * @Description: file content
-->
# NFC Analysis Platform 测试

本目录包含 NFC Analysis Platform 的自动化测试。测试分为两个主要类别：

1. 命令行功能测试 (`cmd_tests/`)
2. Web API 功能测试 (`api_tests/`)

## 测试结构

```
tests/
├── cmd_tests/       # 命令行功能测试
│   ├── nard_test.go # NARD 命令测试
│   ├── tlv_test.go  # TLV 命令测试
│   └── testdata/    # 测试数据文件
│
├── api_tests/       # Web API 功能测试
│   ├── system_test.go  # 系统端点测试
│   ├── nard_test.go    # NARD 端点测试
│   ├── tlv_test.go     # TLV 端点测试
│   └── testdata/       # 测试数据文件
│
├── common/          # 通用测试工具
│   ├── test_utils.go   # 共享测试工具
│   ├── api_test_utils.go # API 测试工具
│   └── fixtures.go     # 测试数据加载
│
└── testdata.json    # 测试数据配置文件
```

## 测试用例

<!-- TEST_TABLE_START -->
| 功能模块 | 测试用例 | 用例说明 | 测试文件 |
| ------- | ------- | ------- | ------- |
| Flipper文件 | 参数验证测试 | 测试未指定串口参数时的错误处理 | nard_test.go |
| Flipper文件 | 获取Flipper设备文件列表 | 测试使用串口模式获取Flipper设备上的文件列表 | nard_test.go |
| Flipper文件 | 获取Flipper设备特定文件内容 | 测试使用串口模式获取Flipper设备上特定APDU响应文件的内容 | nard_test.go |
| Flipper文件 | 获取不存在的文件内容 | 测试获取Flipper设备上不存在的文件时的错误处理 | nard_test.go |
| Flipper设备 | 获取Flipper设备列表 | 测试获取所有可用的Flipper Zero设备列表 | nard_test.go |
| NARD格式模板 | 获取格式模板列表 | 测试获取所有可用的NARD格式模板列表 | nard_test.go |
| NARD格式模板 | 获取特定格式模板内容 | 测试获取EMV格式模板的详细内容 | nard_test.go |
| NARD解码 | 解码APDU响应数据 | 测试使用EMV格式模板解码APDU响应数据 | nard_test.go |
| TLV提取 | 提取TLV特定标签值 | 测试从TLV数据中提取指定标签的值并转换为ASCII格式 | tlv_test.go |
| TLV解析 | 解析TLV数据 | 测试解析十六进制格式的TLV数据结构 | tlv_test.go |
| TLV错误处理 | 处理无效的十六进制数据 | 测试当提供无效的十六进制数据时的错误处理 | tlv_test.go |
| 系统信息 | 获取系统信息 | 测试获取平台的系统信息，包括版本、构建日期、Go版本、操作系统和架构 | system_test.go |

<!-- TEST_TABLE_END -->

### 更新测试用例表格

测试用例表格是通过解析测试文件中的 `@TestInfo` 注释自动生成的。要更新表格，请运行：

```bash
# 在tests目录下运行
python3 test_info_sync_readme.py
```

## 测试数据配置

测试数据存储在 `testdata.json` 文件中，包含以下部分：

```json
{
    "tlv": {
        "sample_data": "6F198407A0000000031010A50E500A4D617374657243617264",
        "sample_data_extended": "6F1A840E315041592E5359532E4444463031A5088801025F2D02zhCN"
    }
}
```

您可以根据需要修改此文件中的测试数据。

## 运行测试

### 使用 Makefile

项目根目录提供了方便的 Makefile 目标来运行测试：

```bash
# 运行所有测试
cd tools/nfc_analysis_platform
make tests-all

# 仅运行 API 测试
make tests-api

# 仅运行命令行测试
make tests-cmd

# 运行命令行测试但不输出错误信息
make tests-cmd LOG_ERROR=false

# 运行命令行测试并输出错误信息（默认行为）
make tests-cmd LOG_ERROR=true

# 运行命令行测试并禁用缓存
make tests-cmd GOFLAGS="-count=1"

# 运行命令行测试，不输出错误信息并禁用缓存
make tests-cmd LOG_ERROR=false GOFLAGS="-count=1"
```

### 直接使用 Go 测试命令

```bash
# 运行所有命令行测试
cd tools/nfc_analysis_platform
go test -v ./tests/cmd_tests/...

# 运行特定测试文件
go test -v ./tests/cmd_tests/nard_test.go

# 运行所有 API 测试
go test -v ./tests/api_tests/...

# 运行特定 API 测试文件
go test -v ./tests/api_tests/system_test.go
```

## 添加新测试

添加新测试时，请遵循以下准则：

1. 将命令行测试放在 `cmd_tests/` 目录中
2. 将 API 测试放在 `api_tests/` 目录中
3. 使用描述性测试名称，指明正在测试的功能
4. 将测试数据添加到 `testdata.json` 文件中
5. 重用 `common/` 目录中的通用测试工具

## 测试依赖项

测试使用 Go 的内置测试框架和以下附加包：

- `github.com/stretchr/testify/assert` - 用于断言
- `github.com/stretchr/testify/require` - 用于必需断言
- `net/http/httptest` - 用于 API 测试

## 注意事项

1. API 测试不应硬编码设备路径或其他环境特定值
2. 测试应该能够在没有实际硬件的情况下运行
3. 对于需要硬件的测试，应该有适当的跳过机制
4. 命令行测试可能需要二进制文件，请先运行 `make build` 