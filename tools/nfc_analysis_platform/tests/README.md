<!--
 * @Author: SpenserCai
 * @Date: 2025-03-15 16:52:39
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 17:24:20
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

## 测试数据配置

测试数据存储在 `testdata.json` 文件中，包含以下部分：

```json
{
    "tlv": {
        "sample_data": "6F198407A0000000031010A50E500A4D617374657243617264",
        "sample_data_extended": "6F1A840E315041592E5359532E4444463031A5088801025F2D02zhCN"
    },
    "nard": {
        "sample_data": {
            "format": "EMV",
            "data": {
                "AID": "A0000000041010",
                "Label": "Visa Credit"
            }
        },
        "sample_response": "Flipper NFC APDU Runner\nDevice: ISO14443-4A (MIFARE DESFire)\nUID: 04A23B9C5D7E8F\nATQA: 0004\nSAK: 08\n\nResponse:\nIn: 00A404000E315041592E5359532E4444463031\nOut: 6F2A840E315041592E5359532E4444463031A518BF0C15611361124F07A0000000031010870101500A4D617374657243617264 9000\nIn: 00B2010C00\nOut: 7081B89081B15A0842628D13FFFFFFFF82025800950500000080005F24032312315F25030101015F280208405F2A0208405F300202015F3401009F01060000000000019F02060000000001009F03060000000000009F0607A00000000310109F0702FF009F080200029F090200029F0D05B8508000009F0E0500000000009F0F05B8708098009F100706010A03A020009F120A4D6173746572436172649F160F3132333435363738393031323334359F1A0208409F1C0831323334353637389F1E0831323334353637389F33036028C89F34030203009F3501229F360200019F3704AAAAAAAA 9000"
    },
    "format_template": "EMV Card Information\nApplication Label: {O[1]TAG(50), \"ascii\"}\nCard Number: {O[3]TAG(9F6B)[0:16], \"numeric\"}\nExpiry Date: Year: 20{O[3]TAG(9F6B)[17:19], \"numeric\"}, Month: {O[3]TAG(9F6B)[19:21], \"numeric\"}\nCountry Code: {O[1]TAG(9F6E)[0:4]}\nApplication Priority: {O[1]TAG(87), \"hex\"}\nApplication Interchange Profile: {O[2]TAG(82), \"hex\"}\nPIN Try Counter: {O[4]TAG(9F17), \"numeric\"}"
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