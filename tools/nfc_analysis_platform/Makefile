# Makefile for APDU Response Decoder

# Go parameters
GOCMD=go
GOBUILD=$(GOCMD) build
GOCLEAN=$(GOCMD) clean
GOTEST=$(GOCMD) test
GOGET=$(GOCMD) get
BINARY_NAME=nfc_analysis_platform
BINARY_UNIX=$(BINARY_NAME)_linux
BINARY_MACOS=$(BINARY_NAME)_macos

.PHONY: build install clean generate-api run-api tests-all tests-api tests-cmd build-web

# 项目名称
PROJECT_NAME := nfc_analysis_platform

# 目录
BIN_DIR := $(HOME)/bin
FORMAT_DIR := $(BIN_DIR)/format
WEB_DIR := web
WEB_SRC_DIR := ../nfc_analysis_platform_web

# Go命令
GO := go
GOFLAGS :=

# Web构建工具
NPM := npm
# 强制使用npm
WEB_CMD := $(NPM)

# 设置默认值
LOG_ERROR ?= true

# 构建目标
build:
	@echo "构建 $(PROJECT_NAME)..."
	@$(GO) build $(GOFLAGS) -o $(PROJECT_NAME) .

# 安装目标
install: build
	@echo "安装 $(PROJECT_NAME) 到 $(BIN_DIR)..."
	@mkdir -p $(BIN_DIR)
	@cp $(PROJECT_NAME) $(BIN_DIR)/
	@mkdir -p $(FORMAT_DIR)
	@cp -r format/* $(FORMAT_DIR)/ 2>/dev/null || :
	@echo "安装完成！"

# 清理目标
clean:
	@echo "清理构建文件..."
	@rm -f $(PROJECT_NAME)
	@rm -f $(BINARY_UNIX)
	@rm -f $(BINARY_MACOS)
	@echo "清理完成！"

# 清理Web构建文件
clean-web:
	@echo "清理Web构建文件..."
	@rm -rf $(WEB_DIR)/*
	@echo "Web清理完成！"

# 生成API代码
generate-api:
	@echo "生成API代码..."
	@chmod +x scripts/generate_api.sh
	@if ./scripts/generate_api.sh; then \
		echo "API代码生成成功！"; \
	else \
		echo "API代码生成失败，请检查错误信息。"; \
		exit 1; \
	fi

# 运行API服务器
run-api: build
	@echo "启动API服务器..."
	@./$(PROJECT_NAME) wapi

# 构建Web项目
build-web:
	@echo "构建Web项目..."
	@if [ ! -d "$(WEB_SRC_DIR)" ]; then \
		echo "错误: Web源代码目录 $(WEB_SRC_DIR) 不存在"; \
		exit 1; \
	fi
	@mkdir -p $(WEB_DIR)
	@cd $(WEB_SRC_DIR) && $(WEB_CMD) install
	@cd $(WEB_SRC_DIR) && $(WEB_CMD) run build
	@echo "复制Web构建文件到 $(WEB_DIR)..."
	@cp -r $(WEB_SRC_DIR)/dist/* $(WEB_DIR)/
	@echo "Web项目构建完成！"

# 测试目标
tests-all: tests-cmd tests-api
	@echo "所有测试完成！"

tests-api:
	@echo "运行API测试..."
	@$(GO) test -v ./tests/api_tests/...

tests-cmd:
	@echo "运行命令行测试..."
	@LOG_ERROR=$(LOG_ERROR) $(GO) test $(GOFLAGS) -v ./tests/cmd_tests/...

# 默认目标现在包括Web构建
all: build build-web

# 帮助信息
help:
	@echo "可用目标:"
	@echo "  build         - 构建项目"
	@echo "  install       - 安装到 $(BIN_DIR)"
	@echo "  clean         - 清理构建文件"
	@echo "  clean-web     - 清理Web构建文件"
	@echo "  generate-api  - 生成API代码"
	@echo "  run-api       - 运行API服务器"
	@echo "  build-web     - 构建Web项目"
	@echo "  tests-all     - 运行所有测试"
	@echo "  tests-api     - 仅运行API测试"
	@echo "  tests-cmd     - 仅运行命令行测试"
	@echo "  help          - 显示此帮助信息"

# Cross compilation
build-linux:
	CGO_ENABLED=0 GOOS=linux GOARCH=amd64 $(GOBUILD) -o $(BINARY_UNIX) -v

build-windows:
	CGO_ENABLED=0 GOOS=windows GOARCH=amd64 $(GOBUILD) -o $(BINARY_NAME).exe -v

build-macos:
	GOOS=darwin GOARCH=arm64 $(GOBUILD) -o $(BINARY_MACOS) -v

build-all: build-linux build-windows build-macos

.PHONY: all build test clean run deps build-linux build-windows build-mac build-macos build-all clean-web build-web 