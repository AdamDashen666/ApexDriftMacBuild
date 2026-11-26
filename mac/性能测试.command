#!/bin/bash
# APEX DRIFT 性能测试（Mac）
# 游戏会自动开车跑几张图、换几档画质，记录帧率并各截一张图，全程约 13 分钟。
# 测试期间别碰鼠标和键盘。测试不会改你的存档和成绩，画质只在测试时临时切换。
# 结果放在桌面 “ApexDrift性能测试-日期” 文件夹里，把整个文件夹压缩后发给开发者。
# 注意：macOS 自带的 bash 是 3.2，变量后面紧跟中文时一律写成 ${VAR}。

set -u
cd "$(dirname "$0")" || exit 1

APP=""
for a in "/Applications/APEX DRIFT.app" "$HOME/Applications/APEX DRIFT.app" "$(pwd)/APEX DRIFT.app"; do
  if [ -d "$a" ]; then APP="$a"; break; fi
done
if [ -z "$APP" ]; then
  echo "没找到 APEX DRIFT.app。请先把它拖进 “应用程序” 文件夹，再运行这个测试。"
  read -r -p "按回车关闭 " _
  exit 1
fi

# 从网上下载的 App 带着隔离标记，没先正常打开过一次的话，直接启动会被系统拦住
if xattr -p com.apple.quarantine "$APP" >/dev/null 2>&1; then
  echo "这个 App 还没有通过 macOS 的首次打开确认（带着下载隔离标记）。"
  echo "可以先按《打开方法.txt》正常打开一次游戏，再回来运行测试；"
  read -r -p "或者输入 y 现在解除隔离标记并继续（等同于《打开方法》里的终端命令）： " ANS
  if [ "${ANS}" = "y" ] || [ "${ANS}" = "Y" ]; then
    xattr -dr com.apple.quarantine "$APP" || { echo "解除失败，请按《打开方法.txt》操作。"; read -r -p "按回车关闭 " _; exit 1; }
  else
    exit 0
  fi
fi

EXE=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist")
BIN="$APP/Contents/MacOS/${EXE}"
OUT="$HOME/Desktop/ApexDrift性能测试-$(date +%m%d-%H%M)"
mkdir -p "$OUT"
MARK="$OUT/.start"
touch "$MARK"

{
  echo "芯片：$(sysctl -n machdep.cpu.brand_string)"
  echo "内存：$(( $(sysctl -n hw.memsize) / 1073741824 )) GB"
  echo "系统：macOS $(sw_vers -productVersion)（$(sw_vers -buildVersion)）"
  echo "App：$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP/Contents/Info.plist" 2>/dev/null)"
  echo "电源：$(pmset -g batt | head -1 | sed "s/.*'\(.*\)'.*/\1/")"
  echo "低电量模式：$(pmset -g | awk '/lowpowermode/{print $2}')"
  echo
  system_profiler SPDisplaysDataType 2>/dev/null | grep -E "Chipset Model|Total Number of Cores|Metal|Resolution|Refresh Rate|UI Looks like|Main Display"
} > "$OUT/系统信息.txt"

tier_name() {
  case "$1" in
    0) echo "极速" ;;
    1) echo "均衡" ;;
    2) echo "高画质" ;;
    3) echo "光追电影级" ;;
  esac
}

# 每一项：地图 画质档 秒数 截图间隔
RUNS="TrainingPark 1 90 60
FujiSakura 1 90 60
MomijiTouge 1 90 60
KuroshioCircuit 1 90 60
FujiSakura 0 60 45
FujiSakura 2 60 45
FujiSakura 3 60 45"
TOTAL=$(echo "$RUNS" | wc -l | tr -d ' ')

echo "开始测试，共 ${TOTAL} 项，约 13 分钟。测试期间请别碰鼠标和键盘。"
echo "插着电源测更准；想中途停止，在这个窗口按 Control+C。"
echo
N=0
echo "$RUNS" | while read -r MAP TIER SECS SHOT; do
  N=$((N + 1))
  LABEL="macperf_${MAP}_t${TIER}"
  echo "[${N}/${TOTAL}] ${MAP}，画质 $(tier_name "$TIER")，${SECS} 秒……"
  T0=$(date +%s)
  "$BIN" -ApexBench -ApexBenchAuto -ApexBenchSecs="${SECS}" -ApexBenchShots="${SHOT}" \
    -ApexMap="${MAP}" -ApexTier="${TIER}" -ApexBenchLabel="${LABEL}" </dev/null >/dev/null 2>&1 &
  P=$!
  LIMIT=$((SECS + 180))
  i=0
  while kill -0 $P 2>/dev/null && [ $i -lt $LIMIT ]; do sleep 1; i=$((i + 1)); done
  if kill -0 $P 2>/dev/null; then
    kill $P 2>/dev/null; sleep 3; kill -9 $P 2>/dev/null
    echo "    超过 ${LIMIT} 秒没结束，已关闭"
  else
    wait $P; echo "    完成，用时 $(( $(date +%s) - T0 )) 秒"
  fi
  sleep 2
done

# 收集结果：只拿这次测试新生成的文件
find "$HOME/Library" -path "*ApexDrift*" -name "macperf_*" -type f -newer "$MARK" 2>/dev/null | while read -r f; do
  cp "$f" "$OUT/"
done
find "$HOME/Library/Logs/DiagnosticReports" -name "*ApexDrift*" -newer "$MARK" -exec cp {} "$OUT/" \; 2>/dev/null
rm -f "$MARK"

# 汇总表：plutil 能直接读 JSON
SUM="$OUT/结果汇总.txt"
printf '%-16s %-10s %-11s %8s %8s %8s %10s\n' "地图" "画质" "渲染尺寸" "平均帧率" "1%低帧" "GPU毫秒" "分辨率比例" > "$SUM"
for f in "$OUT"/macperf_*.json; do
  [ -f "$f" ] || continue
  B=$(basename "$f" .json)
  MAP=$(echo "$B" | sed -E 's/^macperf_(.*)_t[0-9]$/\1/')
  TIER=$(echo "$B" | sed -E 's/.*_t([0-9])$/\1/')
  get() { plutil -extract "$1" raw -o - "$f" 2>/dev/null || echo "-"; }
  printf '%-16s %-10s %-11s %8s %8s %8s %10s\n' "$MAP" "$(tier_name "$TIER")" "$(get viewport)" "$(get avgFps)" "$(get low1Fps)" "$(get avgGpuMs)" "$(get avgResFraction)" >> "$SUM"
done
COUNT=$(ls "$OUT"/macperf_*.json 2>/dev/null | wc -l | tr -d ' ')

echo
cat "$SUM"
echo
if [ "$COUNT" = "0" ]; then
  echo "没有生成任何结果，游戏可能没能启动。文件夹里如果有 .ips 崩溃报告，也请一起发给开发者。"
else
  echo "完成 ${COUNT}/${TOTAL} 项。结果在：${OUT}"
fi
echo "把这个文件夹压缩（右键 → 压缩）后发给开发者即可。"
open "$OUT"
read -r -p "按回车关闭 " _
