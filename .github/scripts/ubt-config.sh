#!/bin/bash
# 写 UBT 配置 Engine/Saved/UnrealBuildTool/BuildConfiguration.xml：$1 = 最多同时跑几个编译 / 链接
# - 7GB 内存同时跑 3 个编译再加主程序链接会把构建机压垮（2026-11 第 4 次构建失联）：编译阶段 2 个，只剩链接时 1 个
# - bAllowUBAExecutor=false（UE 官方保留的传统模式）：UBA 默认把自己的库插进每个编译 / 链接进程，拦截文件读写、
#   把输入文件映射进共享内存；第 12、14 次都在链接游戏主程序时卡死失联，这台小机器上少这一层
mkdir -p "$UE/Engine/Saved/UnrealBuildTool"
printf '%s\n' '<?xml version="1.0" encoding="utf-8" ?>' \
  '<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">' \
  '  <BuildConfiguration>' \
  "    <MaxParallelActions>${1:-2}</MaxParallelActions>" \
  '    <bAllowUBAExecutor>false</bAllowUBAExecutor>' \
  '  </BuildConfiguration>' \
  '</Configuration>' > "$UE/Engine/Saved/UnrealBuildTool/BuildConfiguration.xml"
echo "UBT：最多并行 ${1:-2} 个，不用 UBA 插入"
