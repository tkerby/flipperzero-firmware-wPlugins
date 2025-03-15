<!--
 * @Author: SpenserCai
 * @Date: 2025-03-15 16:52:39
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-15 17:24:20
 * @Description: file content
-->
# NFC Analysis Platform Tests

本目录包含NFC Analysis Platform的自动化测试。测试分为两个主要类别：

1. 命令行功能测试 (`cmd_tests/`)
2. Web API功能测试 (`api_tests/`)

## 测试结构

```
tests/
├── cmd_tests/       # 命令行功能测试
│   ├── nard_test.go # NARD命令测试
│   ├── tlv_test.go  # TLV命令测试
│   └── testdata/    # 测试数据文件
│
├── api_tests/       # Web API功能测试
│   ├── system_test.go  # 系统端点测试
│   ├── nard_test.go    # NARD端点测试
│   ├── tlv_test.go     # TLV端点测试
│   └── testdata/       # 测试数据文件
│
└── common/          # 通用测试工具
    ├── test_utils.go   # 共享测试工具
    ├── api_test_utils.go # API测试工具
    └── fixtures.go     # 测试固定数据
```

## 运行测试

### 使用Makefile

项目根目录提供了方便的Makefile目标来运行测试：

```bash
# 运行所有测试
cd tools/nfc_analysis_platform
make tests-all

# 仅运行API测试
make tests-api

# 仅运行命令行测试
make tests-cmd
```

### 直接使用Go测试命令

```bash
# 运行所有命令行测试
cd tools/nfc_analysis_platform
go test -v ./tests/cmd_tests/...

# 运行特定测试文件
go test -v ./tests/cmd_tests/nard_test.go

# 运行所有API测试
go test -v ./tests/api_tests/...

# 运行特定API测试文件
go test -v ./tests/api_tests/system_test.go
```

## 添加新测试

添加新测试时，请遵循以下准则：

1. 将命令行测试放在`cmd_tests/`目录中
2. 将API测试放在`api_tests/`目录中
3. 使用描述性测试名称，指明正在测试的功能
4. 将测试数据文件添加到适当的`testdata/`目录
5. 重用`common/`目录中的通用测试工具

## 测试依赖项

测试使用Go的内置测试框架和以下附加包：

- `github.com/stretchr/testify/assert` - 用于断言
- `github.com/stretchr/testify/require` - 用于必需断言
- `net/http/httptest` - 用于API测试

安装这些依赖项：

```bash
go get github.com/stretchr/testify/assert
go get github.com/stretchr/testify/require
```

## 注意事项

1. API测试不应硬编码设备路径或其他环境特定值
2. 测试应该能够在没有实际硬件的情况下运行
3. 对于需要硬件的测试，应该有适当的跳过机制
4. APDU API测试目前是框架，因为API尚未实现 