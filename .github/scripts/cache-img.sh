#!/bin/bash
# 编译缓存映像（HFS+ 稀疏磁盘映像，挂到编译中间文件的原位置；缓存只存 bcache/*.sparseimage 这一个个大文件）
#   mount    挂上；没有映像就新建空的
#   unmount  结束编译残留进程，卸下映像并 compact（收回删掉的文件占的空间），之后才能存缓存
# 需要环境变量 PROJ、UE（工作流 env 里有）；在仓库根目录运行
cd "${GITHUB_WORKSPACE:-.}" || exit 1
IMGS=("bcache/build.sparseimage|$PROJ/Intermediate/Build" "bcache/tools.sparseimage|$UE/Engine/Intermediate/Build/Mac")

case "$1" in
  mount)
    mkdir -p bcache
    for e in "${IMGS[@]}"; do
      i=${e%%|*}; m=${e#*|}; mkdir -p "$m"
      if mount | grep -q " on $m "; then echo "$m 已挂载"; continue; fi
      if [ -f "$i" ]; then echo "缓存映像 $i：$(du -h "$i" | cut -f1)"
      else hdiutil create -quiet -size 80g -type SPARSE -fs HFS+ -volname "$(basename "$i" .sparseimage)" "${i%.sparseimage}" && echo "新建空映像 $i"; fi
      if hdiutil attach "$i" -mountpoint "$m" -nobrowse -noautoopen -quiet; then
        # 新挂上的卷 Spotlight 照样会索引（开头的 mdutil -a 只管当时已经挂着的卷）
        sudo mdutil -i off "$m" >/dev/null 2>&1; touch "$m/.metadata_never_index"
      else
        echo "::warning::挂载 $i 失败，删掉它，这部分用普通目录从头编"; rm -f "$i"
      fi
    done
    df -h "$PROJ/Intermediate/Build" "$UE/Engine/Intermediate/Build/Mac" /System/Volumes/Data
    ;;
  unmount)
    # UBT / MSBuild 常驻进程、UBA、xcodebuild、没退干净的编译器还开着映像里的文件，也占着内存
    pkill -f "UnrealEngine/Engine/Binaries/ThirdParty/DotNet" || true
    pkill -f "UnrealEngine/Engine/Binaries/Mac/Uba" || true
    pkill -x xcodebuild || true
    pkill -f "XcodeDefault.xctoolchain/usr/bin/(clang|ld|swift)" || true
    sleep 3; sync; sudo purge || true
    memory_pressure | tail -1; sysctl -n vm.swapusage
    for e in "${IMGS[@]}"; do
      m=${e#*|}
      mount | grep -q " on $m " || continue
      hdiutil detach "$m" -quiet || { sleep 5; hdiutil detach "$m" -force -quiet; } || echo "::warning::卸载 $m 失败"
    done
    for e in "${IMGS[@]}"; do
      i=${e%%|*}
      [ -f "$i" ] && perl -e 'alarm 300; exec @ARGV' hdiutil compact "$i" -quiet
    done
    ls -la bcache; du -sh "$PROJ/Binaries" "$UE/Engine/Binaries/Mac" 2>/dev/null || true
    ;;
  *) echo "用法：$0 mount|unmount"; exit 2 ;;
esac
exit 0
