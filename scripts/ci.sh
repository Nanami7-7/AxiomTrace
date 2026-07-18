#!/usr/bin/env bash
# AxiomTrace Local CI Orchestration Script
# Mirrors the GitHub Actions CI workflow (.github/workflows/ci.yml)
# Usage: ./scripts/ci.sh [--fast|--full] [--dry-run] [--help]

set -e

# -----------------------------------------------------------------------------
# Color output functions
# -----------------------------------------------------------------------------

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_step() {
    echo -e "${YELLOW}[STEP]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

log_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# -----------------------------------------------------------------------------
# Script configuration
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Counters
STEPS_TOTAL=0
STEPS_PASSED=0
STEPS_FAILED=0
FAILED_STEPS=()

# Options
MODE="fast"  # Default mode
DRY_RUN=false
PYTHON_RUNNER_MODE="system"
AXIOM_PYTHON_CMD="${AXIOM_PYTHON:-python}"
AXIOM_UV_PYTHON_VERSION="${AXIOM_PYTHON_VERSION:-}"

if [ -z "$AXIOM_UV_PYTHON_VERSION" ] && [ -f "$PROJECT_ROOT/tool/.python-version" ]; then
    AXIOM_UV_PYTHON_VERSION="$(tr -d '\r\n' < "$PROJECT_ROOT/tool/.python-version")"
fi

run_python() {
    if [ "$PYTHON_RUNNER_MODE" = "uv" ]; then
        uv run --project tool --extra test --python "$AXIOM_UV_PYTHON_VERSION" python "$@"
    else
        "$AXIOM_PYTHON_CMD" "$@"
    fi
}

# -----------------------------------------------------------------------------
# Help function
# -----------------------------------------------------------------------------

show_help() {
    cat << 'EOF'
AxiomTrace Local CI Orchestration Script

Usage: ./scripts/ci.sh [OPTIONS]

Options:
  --fast      Run fast mode (default)
              Steps: CMake configure/build/test + Python compile/pytest/golden/CLI/amalgamate/smoke/doc-link
  --full      Run full mode (includes fast mode + GCC build + preset smoke + integration test)
  --dry-run   Show commands without executing
  --help      Show this help message

Examples:
  ./scripts/ci.sh                # Run fast mode
  ./scripts/ci.sh --fast         # Run fast mode
  ./scripts/ci.sh --full         # Run full mode
  ./scripts/ci.sh --dry-run      # Show what would be executed
  ./scripts/ci.sh --full --dry-run  # Show full mode commands

Environment:
  AXIOM_PYTHON          Override Python executable. Defaults to uv when available.
  AXIOM_PYTHON_VERSION  Python version for uv. Defaults to tool/.python-version.

Fast Mode Steps (1-10):
  1.  CMake configure (clang)
  2.  CMake build
  3.  CTest
  4.  Python compile check
  5.  pytest
  6.  Golden check
  7.  CLI validate
  8.  Amalgamate generate
  9.  Amalgamated header smoke compile
  10. Doc link check

Full Mode Steps (1-15, includes fast mode):
  11. GCC configure
  12. GCC build
  13. GCC CTest
  14. Preset smoke tests (custom/tiny/prod/field/dev)
  15. Integration test
EOF
}

# -----------------------------------------------------------------------------
# Dependency check
# -----------------------------------------------------------------------------

check_dependencies() {
    log_header "Checking Dependencies"

    local missing=()

    # Check bash
    if ! command -v bash &> /dev/null; then
        missing+=("bash")
    fi

    if [ -z "${AXIOM_PYTHON:-}" ] && command -v uv &> /dev/null; then
        PYTHON_RUNNER_MODE="uv"
        if [ -z "$AXIOM_UV_PYTHON_VERSION" ]; then
            missing+=("tool/.python-version or AXIOM_PYTHON_VERSION")
        fi
    elif ! command -v "$AXIOM_PYTHON_CMD" &> /dev/null; then
        missing+=("$AXIOM_PYTHON_CMD")
    fi

    # Check cmake
    if ! command -v cmake &> /dev/null; then
        missing+=("cmake")
    fi

    # Check ninja
    if ! command -v ninja &> /dev/null; then
        missing+=("ninja")
    fi

    # Fast mode configures and smoke-compiles with clang.
    if ! command -v clang &> /dev/null; then
        missing+=("clang")
    fi

    # Full mode adds an independent GCC build.
    if [ "$MODE" = "full" ] && ! command -v gcc &> /dev/null; then
        missing+=("gcc")
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        log_fail "Missing dependencies: ${missing[*]}"
        echo "Please install the missing dependencies and try again."
        exit 1
    fi

    if ! run_python -c "import pytest, click" &> /dev/null; then
        log_fail "Python runner is missing pytest/click. Use uv or install test dependencies."
        exit 1
    fi

    if [ "$PYTHON_RUNNER_MODE" = "uv" ]; then
        log_info "Python runner: uv project tool (Python $AXIOM_UV_PYTHON_VERSION)"
    else
        log_info "Python runner: $AXIOM_PYTHON_CMD"
    fi

    log_pass "All dependencies found"
}

# -----------------------------------------------------------------------------
# Step execution wrapper
# -----------------------------------------------------------------------------

run_step() {
    local step_name="$1"
    local step_cmd="$2"

    STEPS_TOTAL=$((STEPS_TOTAL + 1))
    log_step "Step $STEPS_TOTAL: $step_name"

    if [ "$DRY_RUN" = true ]; then
        echo -e "  ${YELLOW}[DRY-RUN]${NC} $step_cmd"
        STEPS_PASSED=$((STEPS_PASSED + 1))
        return 0
    fi

    local start_time=$SECONDS

    if eval "$step_cmd"; then
        local duration=$((SECONDS - start_time))
        log_pass "$step_name (${duration}s)"
        STEPS_PASSED=$((STEPS_PASSED + 1))
        return 0
    else
        local exit_code=$?
        local duration=$((SECONDS - start_time))
        log_fail "$step_name (exit code: $exit_code, ${duration}s)"
        STEPS_FAILED=$((STEPS_FAILED + 1))
        FAILED_STEPS+=("$step_name")
        exit 1
    fi
}

# -----------------------------------------------------------------------------
# Fast mode steps (1-9)
# -----------------------------------------------------------------------------

run_fast_mode() {
    log_header "Fast Mode"

    # Step 1: CMake configure (clang)
    run_step "CMake configure (clang)" \
        "cmake -S . -B build-local -G Ninja -DAXIOM_BUILD_TESTS=ON -DAXIOM_BUILD_EXAMPLES=ON -DCMAKE_C_COMPILER=clang"

    # Step 2: CMake build
    run_step "CMake build" \
        "cmake --build build-local --parallel"

    # Step 3: CTest
    run_step "CTest" \
        "ctest --test-dir build-local --output-on-failure"

    # Step 4: Python compile check
    run_step "Python compile check" \
        "run_python -m compileall -q tool/src tests"

    # Step 5: pytest
    run_step "pytest" \
        "run_python -m pytest -q"

    # Step 6: Golden check
    run_step "Golden check" \
        "run_python tool/golden/update_golden.py --check"

    # Step 7: CLI validate
    run_step "CLI validate" \
        "PYTHONPATH=tool/src run_python -m axiomtrace_tools.cli validate --golden tool/golden"

    # Step 8: Generate the release header.
    # dist/axiomtrace.h is an ignored generated artifact, so git diff
    # cannot prove it is current. The next step compiles the generated output.
    run_step "Amalgamate generate" \
        "run_python tool/scripts/amalgamate.py && test -s dist/axiomtrace.h"

    # Step 9: Smoke compile generated header
    run_step "Amalgamated header smoke compile" \
        "cat > /tmp/axiomtrace_smoke.c <<'SMOKE_EOF'
#define AXIOMTRACE_IMPLEMENTATION
#include \"axiomtrace.h\"
int main(void) {
    axiom_init();
    AX_EVT(INFO, 0x01u, 0x0001u);
    AX_EVT(INFO, 0x01u, 0x0002u, (uint8_t)1u);
    AX_FAULT(0x02u, 0x0003u);
    (void)axiom_capsule_commit();
    axiom_flush();
    return 0;
}
SMOKE_EOF
clang -std=gnu11 -Wall -Wextra -Werror -Idist /tmp/axiomtrace_smoke.c -o /tmp/axiomtrace_smoke && /tmp/axiomtrace_smoke"

    # Step 10: Doc link check
    run_step "Doc link check" \
        "run_python .githooks/check_doc_links.py"
}

# -----------------------------------------------------------------------------
# Full mode steps (11-15)
# -----------------------------------------------------------------------------

run_full_mode() {
    log_header "Full Mode (additional steps)"

    # Step 10: GCC configure
    run_step "GCC configure" \
        "cmake -S . -B build-local-gcc -G Ninja -DAXIOM_BUILD_TESTS=ON -DAXIOM_SELFTEST=ON -DCMAKE_C_COMPILER=gcc"

    # Step 11: GCC build
    run_step "GCC build" \
        "cmake --build build-local-gcc --parallel"

    # Step 12: GCC CTest
    run_step "GCC CTest" \
        "ctest --test-dir build-local-gcc --output-on-failure"

    # Step 13: Preset smoke tests
    log_step "Step $((STEPS_TOTAL + 1)): Preset smoke tests"
    STEPS_TOTAL=$((STEPS_TOTAL + 1))

    if [ "$DRY_RUN" = true ]; then
        for preset in custom tiny prod field dev; do
            echo -e "  ${YELLOW}[DRY-RUN]${NC} cmake -S . -B build-preset-$preset -G Ninja -DAXIOM_PRESET=$preset -DAXIOM_BUILD_TESTS=OFF -DAXIOM_BUILD_EXAMPLES=ON -DCMAKE_C_COMPILER=clang"
            echo -e "  ${YELLOW}[DRY-RUN]${NC} cmake --build build-preset-$preset --parallel"
        done
        STEPS_PASSED=$((STEPS_PASSED + 1))
    else
        local start_time=$SECONDS
        local preset_failed=false

        for preset in custom tiny prod field dev; do
            log_info "  Building preset: $preset"

            if ! cmake -S . -B "build-preset-$preset" -G Ninja \
                -DAXIOM_PRESET="$preset" \
                -DAXIOM_BUILD_TESTS=OFF \
                -DAXIOM_BUILD_EXAMPLES=ON \
                -DCMAKE_C_COMPILER=clang; then
                log_fail "  Preset $preset configure failed"
                preset_failed=true
                break
            fi

            if ! cmake --build "build-preset-$preset" --parallel; then
                log_fail "  Preset $preset build failed"
                preset_failed=true
                break
            fi

            log_pass "  Preset $preset"
        done

        local duration=$((SECONDS - start_time))

        if [ "$preset_failed" = true ]; then
            log_fail "Preset smoke tests (${duration}s)"
            STEPS_FAILED=$((STEPS_FAILED + 1))
            FAILED_STEPS+=("Preset smoke tests")
            exit 1
        else
            log_pass "Preset smoke tests (${duration}s)"
            STEPS_PASSED=$((STEPS_PASSED + 1))
        fi
    fi

    # Step 15: Integration test
    run_step "Integration test" \
        "bash ./tests/test_integration.sh"
}

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

print_summary() {
    log_header "CI Summary"

    echo -e "Total steps:  $STEPS_TOTAL"
    echo -e "Passed:       ${GREEN}$STEPS_PASSED${NC}"
    echo -e "Failed:       ${RED}$STEPS_FAILED${NC}"

    if [ $STEPS_FAILED -gt 0 ]; then
        echo -e "\n${RED}Failed steps:${NC}"
        for step in "${FAILED_STEPS[@]}"; do
            echo -e "  - $step"
        done
        echo -e "\n${RED}CI FAILED${NC}"
        exit 1
    else
        echo -e "\n${GREEN}CI PASSED${NC}"
        exit 0
    fi
}

# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --fast)
                MODE="fast"
                shift
                ;;
            --full)
                MODE="full"
                shift
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # Header
    echo -e "${BLUE}"
    echo "  ╔═══════════════════════════════════════════════════════════╗"
    echo "  ║           AxiomTrace Local CI Orchestration              ║"
    echo "  ╚═══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"

    log_info "Mode: $MODE"
    log_info "Dry run: $DRY_RUN"
    log_info "Project root: $PROJECT_ROOT"

    # Change to project root
    cd "$PROJECT_ROOT"

    # Check dependencies
    check_dependencies

    # Run mode
    case $MODE in
        fast)
            run_fast_mode
            ;;
        full)
            run_fast_mode
            run_full_mode
            ;;
    esac

    # Print summary
    print_summary
}

# Run main
main "$@"
