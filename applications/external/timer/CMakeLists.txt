include_directories(toolchain/arm64-darwin/arm-none-eabi/include/c++/12.3.1/parallel)
cmake_minimum_required(VERSION 3.13)

project(timer_app C)

# Flipper SDK の CMake モジュールを読み込む（環境変数 FLIPPER_SDK が設定されている前提）
include($ENV{FLIPPER_SDK}/cmake/FlipperSDK.cmake)

# flipper_app() マクロを利用してアプリケーションを定義
flipper_app(timer_app
    SOURCES timer_app.c
)
