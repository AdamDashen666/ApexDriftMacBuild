#!/bin/bash
# 统计两个编译中间目录里的目标文件 .o：总数、0 字节的个数。UBT 把 0 字节的 .o 当成没编过，打包时会重编
# （第 24、25 次打包阶段 UAT 又重编了 12 / 76 个模块，编到快结束时构建机失联；第 21 次同样流程只剩链接）。
# $1 = 标签。公开日志只打印数量；0 字节文件的清单写进 $OUT/logs/objs-<标签>.txt，随详细日志传私有仓库
LIST="$OUT/logs/objs-$1.txt"
mkdir -p "$OUT/logs"; : > "$LIST"
for e in "游戏|$PROJ/Intermediate/Build" "引擎工具|$UE/Engine/Intermediate/Build/Mac"; do
  n=${e%%|*}; d=${e#*|}
  [ -d "$d" ] || { echo "$1：${n}中间目录不存在"; continue; }
  ALL=$(find "$d" -name '*.o' -type f 2>/dev/null | wc -l | tr -d ' ')
  ZERO=$(find "$d" -name '*.o' -type f -size 0 2>/dev/null | tee -a "$LIST" | wc -l | tr -d ' ')
  echo "$1：${n} .o 共 ${ALL} 个，0 字节 ${ZERO} 个"
done
exit 0
