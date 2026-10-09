#!/usr/bin/env bash
# SO-ARM_NanoCam 一键清理脚本
# 清理运行/编译产生的缓存与临时文件，保留源码、标定、文档。
#
# 用法:
#   ./clean.sh           # 正常清理（会列出将删除的内容并确认）
#   ./clean.sh --yes     # 跳过确认直接清理
#   ./clean.sh --dry-run # 只列出将删除的内容，不实际删除
#
# 说明:
#   - 删除 .pio 构建缓存（重新编译时会自动重建，约需几分钟）
#   - 删除 __pycache__ / .pyc / .DS_Store / __MACOSX / *.log / logs/ 等
#   - 保留: 源码 (src/ boards/ lib/ tools/) 标定 (cali/) 文档 (docs/ *.md)
#   - 保留: start_soarm_demo.sh 等脚本与配置
#   - 注意: lib/microros/ 是编译固件必需的 micro-ROS 静态库+头文件，不清理

set -Eeuo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

MODE="run"
case "${1:-}" in
    --yes)     MODE="yes" ;;
    --dry-run) MODE="dry" ;;
    "" )       MODE="run" ;;
    *) echo "用法: $0 [--yes | --dry-run]" >&2; exit 2 ;;
esac

# 收集待清理路径（仅当存在时才列出）
TARGETS=()
add() {
    # 不用 `[[ -e ]] && arr+=()`：set -e 下短路会非零退出
    if [[ -e "$1" ]]; then
        TARGETS+=("$1")
    fi
}

add "firmware/nanocam_soarm/.pio"           # PlatformIO 构建缓存 (最大项)
add "logs"                                  # Agent 运行日志目录

# find 结果用进程替换累积到数组（避免子 shell 导致数组不累积）
collect() {
    local patt=$1 flag=$2
    while IFS= read -r p; do
        add "$p"
    done < <(find . -type d "$flag" -name "$patt" -not -path "./.git/*" -not -path "*/node_modules/*" 2>/dev/null)
}
collect "__pycache__" "d"
collect "__MACOSX" "d"
collect "*.pyc" "f"
collect ".DS_Store" "f"
collect "*.log" "f"

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "✅ 没有发现需要清理的缓存。"
    exit 0
fi

# 统计总大小
total_bytes=0
declare -A SIZES
for t in "${TARGETS[@]}"; do
    b=$(du -sk "$t" 2>/dev/null | awk '{print $1}')
    SIZES["$t"]=$b
    total_bytes=$((total_bytes + b))
done

human() {
    local kb=$1
    if   (( kb >= 1048576 )); then printf "%.1f GB" "$(awk "BEGIN{print $kb/1048576}")"
    elif (( kb >= 1024 ));    then printf "%.1f MB" "$(awk "BEGIN{print $kb/1024}")"
    else printf "%d KB" "$kb"; fi
}

echo "=== 将清理以下缓存/临时文件 ==="
for t in "${TARGETS[@]}"; do
    printf "  %-55s %s\n" "$t" "$(human "${SIZES[$t]}")"
done
echo "----------------------------------"
echo "  合计: $(human "$total_bytes")"
echo

if [[ "$MODE" == "dry" ]]; then
    echo "(--dry-run 模式，未删除任何文件)"
    exit 0
fi

if [[ "$MODE" == "run" ]]; then
    echo -n "确认删除以上内容? [y/N] "
    read -r ans
    [[ "$ans" =~ ^[Yy]$ ]] || { echo "已取消。"; exit 1; }
fi

for t in "${TARGETS[@]}"; do
    rm -rf -- "$t"
    echo "已删除: $t"
done

echo
echo "✅ 清理完成，释放 $(human "$total_bytes")。"
echo "下次 pio run 会自动重建 .pio 构建缓存（需要几分钟）。"
