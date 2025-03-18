#!/bin/bash
###
 # @Author: SpenserCai
 # @Date: 2025-03-15 14:13:59
 # @version: 
 # @LastEditors: SpenserCai
 # @LastEditTime: 2025-03-15 14:22:50
 # @Description: file content
### 

# 确保脚本在错误时退出
set -e

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."
SWAGGER_FILE="$PROJECT_ROOT/pkg/wapi/swagger.yml"
OUTPUT_DIR="$PROJECT_ROOT/pkg/wapi/codegen"

# 检查swagger命令是否可用
SWAGGER_CMD="swagger"
if ! command -v $SWAGGER_CMD >/dev/null 2>&1; then
  # 尝试从GOPATH/bin目录查找
  GOPATH=$(go env GOPATH)
  if [ -n "$GOPATH" ] && [ -f "$GOPATH/bin/swagger" ]; then
    SWAGGER_CMD="$GOPATH/bin/swagger"
    echo "在GOPATH/bin中找到swagger命令: $SWAGGER_CMD"
  else
    echo "错误: 未找到swagger命令。请安装go-swagger。"
    echo "安装指南: https://github.com/go-swagger/go-swagger#installation"
    echo "安装命令: go install github.com/go-swagger/go-swagger/cmd/swagger@latest"
    exit 1
  fi
fi

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

echo "开始生成API代码..."

# 生成服务器代码
$SWAGGER_CMD generate server \
  --target "$OUTPUT_DIR" \
  --spec "$SWAGGER_FILE" \
  --name "nfc-analysis-platform" \
  --flag-strategy pflag \
  --exclude-main

echo "API代码生成完成！"
echo "生成的代码位于: $OUTPUT_DIR" 