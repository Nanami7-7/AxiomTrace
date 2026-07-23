#!/usr/bin/env bash
# AxiomTrace Clean Script
# 清理项目构建产物和临时文件

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 模式标志
DRY_RUN=false
AGGRESSIVE=false

# 统计计数
CLEANED_COUNT=0
SKIPPED_COUNT=0

# 帮助信息
show_help() {
    echo -e "${GREEN}AxiomTrace Clean Script${NC}"
    echo ""
    echo "用法: bash scripts/clean.sh [选项]"
    echo ""
    echo "选项:"
    echo "  --help        显示此帮助信息"
    echo "  --dry-run     仅显示将删除的文件，不实际删除"
    echo "  --aggressive  额外清理AI工具缓存"
    echo ""
    echo "默认清理目标:"
    echo "  - build-*/ 目录（保留 build/）"
    echo "  - Testing/ 目录"
    echo "  - test_report/ 目录"
    echo "  - test_report*.md / performance_report*.md / perf_report*.md 文件（仅清理已被 .gitignore 忽略者）"
    echo "  - __pycache__/ 目录（递归）"
    echo "  - .pytest_cache/ 目录"
    echo "  - dist/axiomtrace.h 文件"
    echo "  - tool/src/axiomtrace_tools.egg-info/ 目录"
    echo "  - tool/build/ 目录"
    echo ""
    echo "aggressive模式额外清理:"
    echo "  - .codebuddy/ 目录"
    echo "  - .codex-build-gcc/ 目录"
    echo "  - .codex-plan-build/ 目录"
    echo "  - .codex-plan-build-gcc/ 目录"
    echo ""
    echo "永不删除:"
    echo "  - .git/"
    echo "  - .githooks/"
    echo "  - .sisyphus/"
    echo "  - 源代码目录 (baremetal/, cmake/, docs/, spec/, tests/, tool/)"
}

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_clean() {
    echo -e "${GREEN}[CLEAN]${NC} $1"
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $1"
}

log_dry() {
    echo -e "${YELLOW}[DRY-RUN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 安全删除函数
safe_remove() {
    local target="$1"
    local description="$2"

    if [ -e "$target" ]; then
        if [ "$DRY_RUN" = true ]; then
            log_dry "将删除: $description ($target)"
            ((CLEANED_COUNT++)) || true
        else
            rm -rf "$target"
            log_clean "已删除: $description"
            ((CLEANED_COUNT++)) || true
        fi
    else
        log_skip "不存在: $description"
        ((SKIPPED_COUNT++)) || true
    fi
}

# 递归清理目录
clean_recursive() {
    local pattern="$1"
    local description="$2"

    local found=false
    while IFS= read -r -d '' target; do
        found=true
        safe_remove "$target" "$description"
    done < <(find "$PROJECT_ROOT" -type d -name "$pattern" -print0 2>/dev/null || true)

    if [ "$found" = false ]; then
        log_skip "未找到: $description"
        ((SKIPPED_COUNT++)) || true
    fi
}

# 递归清理被 .gitignore 标记的生成报告文件
clean_ignored_files() {
    local pattern="$1"
    local description="$2"

    local found=false
    while IFS= read -r -d '' target; do
        found=true
        if command -v git >/dev/null 2>&1 && git -C "$PROJECT_ROOT" check-ignore -q "$target"; then
            safe_remove "$target" "$description"
        else
            log_skip "非忽略文件，保留: $description ($target)"
            ((SKIPPED_COUNT++)) || true
        fi
    done < <(find "$PROJECT_ROOT" -type f -name "$pattern" -print0 2>/dev/null || true)

    if [ "$found" = false ]; then
        log_skip "未找到: $description"
        ((SKIPPED_COUNT++)) || true
    fi
}

# 清理build-*目录（保留build/）
clean_build_dirs() {
    log_info "清理 build-* 目录..."

    local found=false
    for dir in "$PROJECT_ROOT"/build-*/; do
        if [ -d "$dir" ]; then
            found=true
            local dirname=$(basename "$dir")
            safe_remove "$dir" "构建目录 $dirname"
        fi
    done

    if [ "$found" = false ]; then
        log_skip "未找到 build-* 目录"
        ((SKIPPED_COUNT++)) || true
    fi
}

# 主清理函数
clean_default() {
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  AxiomTrace 默认清理${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""

    # 1. 清理 build-* 目录（保留 build/）
    clean_build_dirs

    # 2. 清理 Testing/ 目录
    log_info "清理 Testing/ 目录..."
    safe_remove "$PROJECT_ROOT/Testing" "Testing 目录"

    # 3. 清理 test_report/ 目录
    log_info "清理 test_report/ 目录..."
    safe_remove "$PROJECT_ROOT/test_report" "test_report 目录"

    # 4. 清理生成报告文件（仅限 .gitignore 覆盖的文件）
    log_info "清理生成报告文件..."
    clean_ignored_files "test_report*.md" "test_report Markdown 文件"
    clean_ignored_files "performance_report*.md" "performance_report Markdown 文件"
    clean_ignored_files "perf_report*.md" "perf_report Markdown 文件"

    # 5. 清理 __pycache__/ 目录（递归）
    log_info "清理 __pycache__/ 目录..."
    clean_recursive "__pycache__" "__pycache__ 目录"

    # 6. 清理 .pytest_cache/ 目录
    log_info "清理 .pytest_cache/ 目录..."
    safe_remove "$PROJECT_ROOT/.pytest_cache" ".pytest_cache 目录"

    # 7. 清理生成的发布头文件
    log_info "清理 dist/axiomtrace.h 文件..."
    safe_remove "$PROJECT_ROOT/dist/axiomtrace.h" "发布头文件"

    # 8. 清理 tool/src/axiomtrace_tools.egg-info/ 目录
    log_info "清理 tool/src/axiomtrace_tools.egg-info/ 目录..."
    safe_remove "$PROJECT_ROOT/tool/src/axiomtrace_tools.egg-info" "egg-info 目录"

    # 9. 清理 tool/build/ 目录
    log_info "清理 tool/build/ 目录..."
    safe_remove "$PROJECT_ROOT/tool/build" "tool/build 目录"
}

# aggressive模式额外清理
clean_aggressive() {
    echo ""
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}  AxiomTrace Aggressive 清理${NC}"
    echo -e "${YELLOW}========================================${NC}"
    echo ""

    log_info "清理 .codebuddy/ 目录..."
    safe_remove "$PROJECT_ROOT/.codebuddy" ".codebuddy 目录"

    log_info "清理 .codex-build-gcc/ 目录..."
    safe_remove "$PROJECT_ROOT/.codex-build-gcc" ".codex-build-gcc 目录"

    log_info "清理 .codex-plan-build/ 目录..."
    safe_remove "$PROJECT_ROOT/.codex-plan-build" ".codex-plan-build 目录"

    log_info "清理 .codex-plan-build-gcc/ 目录..."
    safe_remove "$PROJECT_ROOT/.codex-plan-build-gcc" ".codex-plan-build-gcc 目录"
}

# 显示摘要
show_summary() {
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  清理完成${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "已清理: ${GREEN}$CLEANED_COUNT${NC} 项"
    echo -e "跳过:   ${YELLOW}$SKIPPED_COUNT${NC} 项"
    echo ""
}

# 主函数
main() {
    # 解析命令行参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help)
                show_help
                exit 0
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --aggressive)
                AGGRESSIVE=true
                shift
                ;;
            *)
                log_error "未知选项: $1"
                echo "使用 --help 查看帮助"
                exit 1
                ;;
        esac
    done

    # 显示模式信息
    if [ "$DRY_RUN" = true ]; then
        log_info "运行模式: DRY-RUN（仅显示，不删除）"
    fi

    if [ "$AGGRESSIVE" = true ]; then
        log_info "运行模式: AGGRESSIVE（包含AI工具缓存）"
    fi

    # 切换到项目根目录
    cd "$PROJECT_ROOT"

    # 执行清理
    clean_default

    if [ "$AGGRESSIVE" = true ]; then
        clean_aggressive
    fi

    # 显示摘要
    show_summary
}

# 运行主函数
main "$@"
