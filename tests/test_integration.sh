#!/bin/bash
# AxiomTrace Integration Tests
# Tests CLI, decoder, and library linking

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASSED=0
FAILED=0

# -----------------------------------------------------------------------------
# Helper functions
# -----------------------------------------------------------------------------

log_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((++PASSED))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((++FAILED))
}

run_test() {
    local name="$1"
    local cmd="$2"
    log_info "Running: $name"
    if eval "$cmd"; then
        log_pass "$name"
        return 0
    else
        log_fail "$name (exit code: $?)"
        return 1
    fi
}

# -----------------------------------------------------------------------------
# Setup
# -----------------------------------------------------------------------------

log_info "Setting up test environment..."

# Create temp directory for test files
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT

# Build directory (if exists)
BUILD_DIR="$PROJECT_ROOT/build"

log_info "Test directory: $TEST_DIR"

# -----------------------------------------------------------------------------
# Test: Python module can be imported
# -----------------------------------------------------------------------------

test_import() {
    log_info "Testing Python module import..."
    cd "$PROJECT_ROOT/tool/src"
    python3 -c "
import sys
sys.path.insert(0, '.')
from axiomtrace_tools import decoder, cli
from axiomtrace_tools.decoder import decode_frame, decode_stream, find_dictionary, crc16_ccitt_false
print('Import successful')
"
}

if test_import; then
    log_pass "Python module import"
else
    log_fail "Python module import"
fi

# -----------------------------------------------------------------------------
# Test: CLI entry point exists
# -----------------------------------------------------------------------------

test_cli_entry() {
    cd "$PROJECT_ROOT/tool/src"
    python3 -c "
import sys
sys.path.insert(0, '.')
from axiomtrace_tools.cli import main
print('CLI entry point accessible')
"
}

if test_cli_entry; then
    log_pass "CLI entry point exists"
else
    log_fail "CLI entry point exists"
fi

# -----------------------------------------------------------------------------
# Test: Decoder functions work
# -----------------------------------------------------------------------------

test_decoder_functions() {
    cd "$PROJECT_ROOT/tool/src"
    python3 -c "
import sys
import struct
sys.path.insert(0, '.')
from axiomtrace_tools.decoder import (
    FRAME_SYNC, WIRE_VERSION_MAJOR, crc16_ccitt_false,
    decode_frame, decode_stream
)

# Test CRC calculation
crc = crc16_ccitt_false(b'')
assert crc == 0xFFFF, f'CRC of empty bytes should be 0xFFFF, got {crc:04X}'

# Build a minimal valid frame
frame = bytearray()
frame.append(FRAME_SYNC)
frame.append((WIRE_VERSION_MAJOR << 4) | 0x01)  # v1.1
frame.append(0x01)  # INFO level
frame.append(0x10)  # module_id
frame.extend(struct.pack('<H', 0x0001))  # event_id
frame.extend(struct.pack('<H', 1))  # seq
frame.append(0)  # timestamp_delta=0
frame.append(0)  # payload_len
crc = crc16_ccitt_false(bytes(frame))
frame.extend(struct.pack('<H', crc))

# Decode it
result, offset = decode_frame(bytes(frame))
assert 'error' not in result, f'Decode error: {result}'
assert result['sync'] == FRAME_SYNC
assert result['level'] == 'INFO'
assert result['module_id'] == 0x10
assert result['event_id'] == 0x0001

print('Decoder functions work correctly')
"
}

if test_decoder_functions; then
    log_pass "Decoder functions work"
else
    log_fail "Decoder functions work"
fi

# -----------------------------------------------------------------------------
# Test: Auto-search find_dictionary
# -----------------------------------------------------------------------------

test_find_dictionary() {
    cd "$PROJECT_ROOT/tool/src"
    python3 -c "
import sys
import os
import tempfile
import json
from pathlib import Path
sys.path.insert(0, '.')
from axiomtrace_tools.decoder import find_dictionary

# Create temp structure
with tempfile.TemporaryDirectory() as tmpdir:
    # Create dictionary.json directly in tmpdir
    d = Path(tmpdir) / 'dictionary.json'
    d.write_text(json.dumps({'version': '1.0'}))
    
    found = find_dictionary(tmpdir)
    assert found is not None, 'Should find dictionary.json'
    assert found == d, f'Expected {d}, got {found}'
    
    # Test not found
    empty_dir = Path(tmpdir) / 'empty'
    empty_dir.mkdir()
    d.unlink()
    found = find_dictionary(str(empty_dir))
    assert found is None, 'Should not find dictionary in empty dir'

print('find_dictionary works correctly')
"
}

if test_find_dictionary; then
    log_pass "find_dictionary auto-search"
else
    log_fail "find_dictionary auto-search"
fi

# -----------------------------------------------------------------------------
# Test: CLI argument parsing
# -----------------------------------------------------------------------------

test_cli_args() {
    cd "$PROJECT_ROOT/tool/src"
    python3 -c "
import sys
sys.path.insert(0, '.')
from click.testing import CliRunner
from axiomtrace_tools.cli import main

runner = CliRunner()

# Test --help
result = runner.invoke(main, ['--help'])
assert result.exit_code == 0, f'--help should exit with 0, got {result.exit_code}'
assert 'AxiomTrace' in result.output

# Test with invalid input file
result = runner.invoke(main, ['/nonexistent/file.bin'])
# Should fail or show error gracefully

print('CLI argument parsing works')
"
}

if test_cli_args; then
    log_pass "CLI argument parsing"
else
    log_fail "CLI argument parsing"
fi

# -----------------------------------------------------------------------------
# Test: Binary frame roundtrip
# -----------------------------------------------------------------------------

test_frame_roundtrip() {
    cd "$PROJECT_ROOT/tool/src"
    python3 -c "
import sys
import struct
sys.path.insert(0, '.')
from axiomtrace_tools.decoder import (
    FRAME_SYNC, WIRE_VERSION_MAJOR, crc16_ccitt_false,
    decode_frame
)

# Create frame with various payload types
frame = bytearray()
frame.append(FRAME_SYNC)
frame.append((WIRE_VERSION_MAJOR << 4) | 0x01)  # v1.1
frame.append(0x02)  # WARN level
frame.append(0xAB)  # module_id
frame.extend(struct.pack('<H', 0x1234))  # event_id
frame.extend(struct.pack('<H', 99))  # seq
frame.append(0)  # timestamp_delta=0
frame.append(0)  # payload_len
crc = crc16_ccitt_false(bytes(frame))
frame.extend(struct.pack('<H', crc))

result, _ = decode_frame(bytes(frame))
assert result['level'] == 'WARN', f'Expected WARN, got {result[\"level\"]}'
assert result['module_id'] == 0xAB
assert result['event_id'] == 0x1234
assert result['seq'] == 99

print('Frame roundtrip successful')
"
}

if test_frame_roundtrip; then
    log_pass "Binary frame roundtrip"
else
    log_fail "Binary frame roundtrip"
fi

# -----------------------------------------------------------------------------
# Test: C library linking (if build exists)
# -----------------------------------------------------------------------------

test_c_library() {
    if [ -d "$BUILD_DIR" ] && [ -f "$BUILD_DIR/libaxiomtrace.a" ]; then
        log_info "Testing C library linking..."
        # Verify the static library exists and contains expected symbols
        nm "$BUILD_DIR/libaxiomtrace.a" 2>/dev/null | grep -q "axiom_init" && \
            log_pass "C library contains axiom_init symbol" || \
            log_fail "C library missing axiom_init symbol"
    else
        log_info "C library not built, skipping library link test"
    fi
}

test_c_library

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

echo ""
echo "========================================"
echo -e "Results: ${GREEN}$PASSED passed${NC}, ${RED}$FAILED failed${NC}"
echo "========================================"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All integration tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi
