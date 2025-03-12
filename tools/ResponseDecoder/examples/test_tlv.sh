#!/bin/bash

# 测试TLV解析功能的脚本

# 切换到脚本所在目录的父目录
cd "$(dirname "$0")/.."

# 编译程序
echo "编译程序..."
go build -o response_decoder

# 运行测试
echo "运行TLV解析测试..."
./response_decoder -f examples/emv_example.apdures --decode-format format/EMV.apdufmt

echo "测试完成！" 