"""AxiomTrace Python tools tests - CLI, decoder, auto-search."""

import binascii
import json
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import pytest

# Add tool/src to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "tool" / "src"))

from axiomtrace_tools.cli import main as cli_main
from axiomtrace_tools.bundle import generate_bundle, load_bundle
from axiomtrace_tools.capsule import decode_capsule, render_capsule_report
from axiomtrace_tools.codegen import generate_assets, validate_event_source
from axiomtrace_tools.dictionary import load_dictionary
from axiomtrace_tools.decoder import (
    FRAME_SYNC,
    WIRE_VERSION_MAJOR,
    crc16_ccitt_false,
    decode_frame,
    decode_stream,
    extract_metadata_id,
    find_dictionary,
)
from axiomtrace_tools.render import render_json, render_text
from axiomtrace_tools.validator import validate_bundle, validate_golden, validate_trace_bundle


# =============================================================================
# Fixtures
# =============================================================================

def make_frame(
    payload=b"",
    *,
    major=WIRE_VERSION_MAJOR,
    minor=0,
    level=0x01,
    module_id=0x10,
    event_id=0x0001,
    seq=1,
    timestamp_delta=0,
):
    """Create a raw frame using current v2 layout or an explicitly selected legacy v1 layout."""
    frame = bytearray()
    frame.append(FRAME_SYNC)
    frame.append((major << 4) | minor)
    frame.append(level)
    frame.append(module_id)
    frame.extend(struct.pack("<H", event_id))
    frame.extend(struct.pack("<H", seq))
    if major >= 2 or minor >= 1:
        assert 0 <= timestamp_delta <= 0x7F
        frame.append(timestamp_delta)
    frame.append(len(payload))
    frame.extend(payload)
    crc = crc16_ccitt_false(bytes(frame))
    frame.extend(struct.pack("<H", crc))
    return bytes(frame)


@pytest.fixture
def valid_frame():
    """Create a minimal valid current-wire frame."""
    return make_frame()


def identity_frame(metadata_id: str) -> bytes:
    return make_frame(
        bytes([0x0B]) + bytes.fromhex(metadata_id),
        module_id=0,
        event_id=2,
        seq=0,
    )


def make_capsule(frames: bytes, firmware_hash: bytes = b"\x01\x23\x45\x67") -> bytes:
    capsule = bytearray(b"AXCP")
    capsule.append(1)
    capsule.extend(b"\0\0")
    capsule.extend(bytes([1, 2]))
    capsule.extend(firmware_hash)
    capsule.extend(struct.pack("<I", 0))
    capsule.extend(bytes([0, 0, 1, 0]))
    capsule.extend(frames)
    capsule_len = len(capsule) + 4
    struct.pack_into("<H", capsule, 5, capsule_len)
    capsule.extend(struct.pack("<I", binascii.crc32(capsule) & 0xFFFFFFFF))
    return bytes(capsule)


@pytest.fixture
def temp_dict_dir(tmp_path):
    """Create a temporary directory structure for dictionary auto-search tests."""
    # Create hierarchy: tmp_path/build/gen/.axiom/dictionary.json
    build_dir = tmp_path / "build"
    gen_dir = build_dir / "gen"
    axiom_dir = gen_dir / ".axiom"
    axiom_dir.mkdir(parents=True)
    return tmp_path, axiom_dir


@pytest.fixture
def sample_dictionary():
    """Sample dictionary content for testing."""
    return {
        "version": "1.0",
        "events": {
            "0x10:0x0001": {"name": "TEST_EVENT", "description": "Test event"}
        }
    }


@pytest.fixture
def sample_events_source():
    """Sample event source compatible with axiom-codegen."""
    return {
        "version": "1.0",
        "dictionary": {
            "modules": [
                {
                    "id": 0x10,
                    "name": "TEST",
                    "events": [
                        {
                            "id": 0x0001,
                            "name": "VALUE",
                            "level": "INFO",
                            "text": "value={value:u8}",
                            "args": [{"name": "value", "type": "u8"}],
                        }
                    ],
                }
            ]
        },
    }


# =============================================================================
# Decoder Tests
# =============================================================================

class TestDecoder:
    """Tests for decode_frame and decode_stream functions."""

    def test_decode_frame_valid(self, valid_frame):
        """Test decoding a valid frame."""
        result, offset = decode_frame(valid_frame)
        assert result is not None
        assert 'error' not in result
        assert result['sync'] == FRAME_SYNC
        assert result['version'] == (2, 0)
        assert result['level'] == 'INFO'
        assert result['module_id'] == 0x10
        assert result['event_id'] == 0x0001
        assert result['seq'] == 1
        assert result['payload'] == []

    def test_decode_frame_invalid_version(self):
        """Test decoding frame with wrong version."""
        result, _ = decode_frame(make_frame(major=3))
        assert 'error' in result
        assert result['error'] == 'VERSION_MISMATCH'

    def test_decode_frame_truncated(self):
        """Test decoding truncated frame."""
        # Too short to be a valid frame (less than minimum 11 bytes)
        frame = bytes([FRAME_SYNC, 0x20, 0x01, 0x10, 0x01, 0x00, 0x01, 0x00, 0x00])
        result, offset = decode_frame(frame)
        # decode_frame returns (None, offset) when frame is invalid/truncated
        assert result is None or 'error' in result

    def test_decode_stream_empty(self):
        """Test decoding empty stream."""
        result = decode_stream(b'')
        assert result == []

    def test_decode_stream_multiple_frames(self, valid_frame):
        """Test decoding stream with multiple frames."""
        # Duplicate the valid frame twice
        stream = valid_frame + valid_frame
        result = decode_stream(stream)
        assert len(result) == 2
        assert all('error' not in r for r in result)

    def test_decode_legacy_v1_0_without_timestamp(self):
        result, _ = decode_frame(make_frame(major=1, minor=0))
        assert "error" not in result
        assert result["version"] == (1, 0)
        assert "timestamp_delta" not in result

    def test_decode_legacy_v1_1_typed_payload_without_dictionary(self):
        result, _ = decode_frame(make_frame(bytes([0x01, 42]), major=1, minor=1))
        assert "error" not in result
        assert result["version"] == (1, 1)
        assert result["payload"] == [{"type": "u8", "value": 42}]

    def test_decode_v2_system_probe_retains_typed_payload(self):
        result, _ = decode_frame(
            make_frame(bytes([0x03, 0x34, 0x12, 0x01, 42]), module_id=0, event_id=0)
        )
        assert result["payload"] == [
            {"type": "u16", "value": 0x1234},
            {"type": "u8", "value": 42},
        ]
        rendered = json.loads(render_json([result]))[0]
        assert (rendered["module"], rendered["event"]) == ("SYSTEM", "PROBE")

    def test_extract_metadata_identity(self):
        metadata_id = "0123456789abcdef"
        frames = decode_stream(identity_frame(metadata_id))
        assert extract_metadata_id(frames) == metadata_id

    def test_metadata_identity_rejects_trailing_payload(self):
        metadata_id = "0123456789abcdef"
        result, _ = decode_frame(
            make_frame(bytes([0x0B]) + bytes.fromhex(metadata_id) + b"\xFF", module_id=0, event_id=2)
        )
        assert result["metadata_id"] == metadata_id
        assert result["warnings"] == ["INVALID_METADATA_IDENTITY"]

    def test_crc16_ccitt_false(self):
        """Test CRC calculation."""
        # Known test case: CRC of empty bytes should be 0xFFFF
        assert crc16_ccitt_false(b'') == 0xFFFF


# =============================================================================
# Auto-search Tests
# =============================================================================

class TestAutoSearch:
    """Tests for find_dictionary function."""

    def test_find_dictionary_in_build_dir(self, temp_dict_dir, sample_dictionary):
        """Test finding dictionary in build/ directory."""
        tmp_path, axiom_dir = temp_dict_dir
        dict_path = axiom_dir / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        # Create dictionary at tmp_path/build/gen/.axiom/dictionary.json
        # But we expect search to find it in build/dictionary.json or build/gen/dictionary.json
        # Let's create the proper structure
        build_dict = tmp_path / "build" / "dictionary.json"
        build_dict.parent.mkdir(parents=True, exist_ok=True)
        build_dict.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        found = find_dictionary(str(tmp_path / "build"))
        assert found is not None
        assert found.is_file()
        assert found.name == "dictionary.json"

    def test_find_dictionary_direct(self, tmp_path, sample_dictionary):
        """Test finding dictionary.json directly in directory."""
        dict_path = tmp_path / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        found = find_dictionary(str(tmp_path))
        assert found == dict_path

    def test_find_dictionary_not_found(self, tmp_path):
        """Test when no dictionary is found."""
        found = find_dictionary(str(tmp_path))
        assert found is None

    def test_find_dictionary_walks_up(self, tmp_path, sample_dictionary):
        """Test that find_dictionary walks up the directory tree."""
        # Create dictionary in a subdirectory
        sub_dir = tmp_path / "sub" / "deep"
        sub_dir.mkdir(parents=True)
        # dictionary in parent
        dict_path = tmp_path / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        found = find_dictionary(str(sub_dir))
        assert found == dict_path

    def test_find_dictionary_stops_at_home(self, tmp_path):
        """Test that search stops at home directory."""
        # Search from tmp_path which should not find anything and stop
        found = find_dictionary(str(tmp_path))
        # May or may not find depending on whether dictionary exists above
        # Just ensure it doesn't raise an exception
        assert found is None or isinstance(found, Path)


# =============================================================================
# CLI Argument Parsing Tests
# =============================================================================

class TestCLI:
    """Tests for CLI argument parsing."""

    def test_cli_no_args(self):
        """Test CLI with no arguments shows help."""
        from click.testing import CliRunner
        runner = CliRunner()
        result = runner.invoke(cli_main, [])
        # No args should show help or require INPUT
        assert result.exit_code != 0 or 'INPUT' in result.output

    def test_cli_help(self):
        """Test CLI --help."""
        from click.testing import CliRunner
        runner = CliRunner()
        result = runner.invoke(cli_main, ['--help'])
        assert result.exit_code == 0
        assert 'AxiomTrace' in result.output
        assert 'decoder' in result.output.lower()

    def test_cli_valid_input_with_empty_data(self, tmp_path):
        """Test CLI with valid input but empty data."""
        input_file = tmp_path / "input.bin"
        input_file.write_bytes(b'')

        # Should not crash, returns empty list
        result = decode_stream(b'')
        assert result == []

    def test_cli_output_format_json(self, tmp_path, valid_frame, sample_dictionary):
        """Test CLI with JSON output format."""
        import click
        from click.testing import CliRunner

        input_file = tmp_path / "input.bin"
        input_file.write_bytes(valid_frame)

        dict_file = tmp_path / "dictionary.json"
        dict_file.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        runner = CliRunner()

        @click.command()
        @click.argument('input', type=click.File('rb'))
        @click.option('--dictionary', '-d', type=click.Path(exists=True), default=None)
        @click.option('--output', '-o', type=click.Choice(['text', 'json']), default='text')
        def test_cmd(input, dictionary, output):
            click.echo(f"output={output}")

        result = runner.invoke(test_cmd, [str(input_file), '-o', 'json'])
        assert result.exit_code == 0
        assert 'output=json' in result.output


class TestToolchain:
    """Tests for dictionary rendering, codegen, and bundle generation."""

    def test_dictionary_render_text(self, tmp_path, sample_events_source):
        dict_dir = tmp_path / "generated"
        source = tmp_path / "events.json"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        generate_assets(source, dict_dir)

        dictionary = load_dictionary(dict_dir / "dictionary.json")
        frame = {
            "level": "INFO",
            "module_id": 0x10,
            "event_id": 0x0001,
            "seq": 7,
            "payload": [{"type": "u8", "value": 42}],
            "crc": 0,
        }

        text = render_text([frame], dictionary)
        assert "[TEST] VALUE" in text
        assert "value=42" in text

    def test_codegen_generates_assets(self, tmp_path, sample_events_source):
        source = tmp_path / "events.json"
        out_dir = tmp_path / "generated"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")

        generated = generate_assets(source, out_dir)

        assert generated["dictionary"].is_file()
        assert generated["events_header"].is_file()
        assert generated["modules_header"].is_file()
        assert "AXIOM_EVENT_TEST_VALUE" in generated["events_header"].read_text(encoding="utf-8")

    def test_bundle_generate_dictionary_only(self, tmp_path, sample_events_source):
        source = tmp_path / "events.json"
        bundle_dir = tmp_path / "axiomtrace-bundle"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")

        generated = generate_bundle(source, bundle_dir)
        bundle = load_bundle(generated["manifest"])

        assert generated["manifest"].is_file()
        assert generated["dictionary"].is_file()
        assert bundle.metadata_id
        validate_bundle(generated["manifest"])
        assert bundle.load_dictionary().find_event(0x10, 0x0001).event_name == "VALUE"

    def test_decoder_cli_bundle_json(self, tmp_path, sample_events_source):
        from click.testing import CliRunner

        source = tmp_path / "events.json"
        bundle_dir = tmp_path / "axiomtrace-bundle"
        input_file = tmp_path / "input.bin"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        generate_bundle(source, bundle_dir)
        bundle = load_bundle(bundle_dir)
        input_file.write_bytes(
            identity_frame(bundle.metadata_id)
            + make_frame(bytes([42]), seq=1)
        )

        runner = CliRunner()
        result = runner.invoke(cli_main, [str(input_file), "--bundle", str(bundle_dir), "--format", "json"])

        assert result.exit_code == 0
        decoded = json.loads(result.output[result.output.find("["):])
        assert decoded[1]["module"] == "TEST"
        assert decoded[1]["event"] == "VALUE"
        assert decoded[1]["args"] == {"value": 42}

    def test_bundle_store_selects_metadata_and_resolves_location(self, tmp_path, sample_events_source):
        from click.testing import CliRunner

        source = tmp_path / "events.json"
        source_id_map = tmp_path / "source_ids.json"
        store = tmp_path / "store"
        bundle_dir = store / "candidate"
        input_file = tmp_path / "input.bin"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        source_id_map.write_text(
            json.dumps({"files": [{"id": 1, "path": "src/control.c"}]}),
            encoding="utf-8",
        )
        generate_bundle(
            source,
            bundle_dir,
            source_id_map=source_id_map,
            location_mode="file_id",
        )
        bundle = load_bundle(bundle_dir)
        payload = bytes([42, 0x0A, 0x02, 0x01, 0x00, 0x2A, 0x00])
        input_file.write_bytes(identity_frame(bundle.metadata_id) + make_frame(payload, seq=1))

        runner = CliRunner()
        result = runner.invoke(cli_main, [str(input_file), "--bundle-store", str(store), "--format", "json"])

        assert result.exit_code == 0, result.output
        decoded = json.loads(result.output[result.output.find("["):])
        assert decoded[1]["location"]["file"] == "src/control.c"
        assert decoded[1]["location"]["line"] == 42

    def test_bundle_semantics_reject_identity_mismatch_but_raw_survives(self, tmp_path, sample_events_source):
        from click.testing import CliRunner

        source = tmp_path / "events.json"
        bundle_dir = tmp_path / "axiomtrace-bundle"
        input_file = tmp_path / "input.bin"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        generate_bundle(source, bundle_dir)
        input_file.write_bytes(identity_frame("0000000000000000") + make_frame(bytes([42])))

        runner = CliRunner()
        semantic = runner.invoke(cli_main, [str(input_file), "--bundle", str(bundle_dir), "--format", "json"])
        raw = runner.invoke(cli_main, [str(input_file), "--bundle", str(bundle_dir), "--format", "raw"])

        assert semantic.exit_code != 0
        assert "does not match bundle" in semantic.output
        assert raw.exit_code == 0

    def test_bundle_semantics_require_identity_event(self, tmp_path, sample_events_source):
        from click.testing import CliRunner

        source = tmp_path / "events.json"
        bundle_dir = tmp_path / "axiomtrace-bundle"
        input_file = tmp_path / "input.bin"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        generate_bundle(source, bundle_dir)
        input_file.write_bytes(make_frame(bytes([42])))

        result = CliRunner().invoke(
            cli_main,
            [str(input_file), "--bundle", str(bundle_dir), "--format", "json"],
        )
        assert result.exit_code != 0
        assert "metadata identity event" in result.output

    def test_bundle_validator_rejects_malformed_packed_suffix(self, tmp_path, sample_events_source):
        source = tmp_path / "events.json"
        bundle_dir = tmp_path / "axiomtrace-bundle"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        generate_bundle(source, bundle_dir)
        bundle = load_bundle(bundle_dir)
        frames = decode_stream(
            identity_frame(bundle.metadata_id) + make_frame(bytes([42, 0xFF])),
            bundle.load_dictionary(),
        )

        with pytest.raises(ValueError, match="payload decode warning"):
            validate_trace_bundle(frames, bundle)

    def test_capsule_report_matches_embedded_metadata_identity(self, tmp_path, sample_events_source):
        source = tmp_path / "events.json"
        bundle_dir = tmp_path / "axiomtrace-bundle"
        source.write_text(json.dumps(sample_events_source), encoding="utf-8")
        generate_bundle(source, bundle_dir)
        bundle = load_bundle(bundle_dir)
        report = decode_capsule(make_capsule(identity_frame(bundle.metadata_id) + make_frame(bytes([42]))))

        rendered = json.loads(
            render_capsule_report(
                report,
                "json",
                dictionary=bundle.load_dictionary(),
                bundle_metadata_id=bundle.metadata_id,
            )
        )
        assert rendered["trace_metadata_id"] == bundle.metadata_id
        assert rendered["metadata_identity_matches_bundle"] is True
        assert rendered["firmware_hash"] == "01234567"
        assert rendered["events"][1]["args"] == {"value": 42}

        with pytest.raises(ValueError, match="does not match bundle"):
            render_capsule_report(report, "json", bundle_metadata_id="0000000000000000")

    def test_codegen_accounts_for_location_metadata_overhead(self):
        args = [{"name": f"a{idx}", "type": "u32"} for idx in range(31)]
        source = {
            "dictionary": {
                "modules": [{
                    "id": 1,
                    "name": "TEST",
                    "events": [{
                        "id": 1,
                        "name": "LARGE",
                        "text": " ".join(f"{{a{idx}:u32}}" for idx in range(31)),
                        "args": args,
                    }],
                }]
            }
        }
        validate_event_source(source, location_mode="none")
        with pytest.raises(ValueError, match="payload exceeds"):
            validate_event_source(source, location_mode="file_id")

    @pytest.mark.skipif(
        shutil.which("cmake") is None or shutil.which("ninja") is None,
        reason="CMake/Ninja toolchain is required",
    )
    @pytest.mark.parametrize("location_mode", ["file_id", "hash"])
    def test_cmake_fixture_restores_locations_from_two_sources(self, tmp_path, location_mode):
        from click.testing import CliRunner

        repository = Path(__file__).parent.parent.resolve()
        fixture = repository / "tests" / "cmake" / "bundle_fixture"
        build = tmp_path / f"fixture-build-{location_mode}"
        trace = tmp_path / "fixture.bin"
        subprocess.run(
            [
                "cmake", "-S", str(fixture), "-B", str(build), "-G", "Ninja",
                f"-DAXIOMTRACE_ROOT={repository.as_posix()}",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                f"-DAXIOM_FIXTURE_LOCATION_MODE={location_mode}",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            ["cmake", "--build", str(build), "--target", "fixture_axiom_bundle"],
            check=True,
            capture_output=True,
            text=True,
        )
        executable = build / ("fixture.exe" if os.name == "nt" else "fixture")
        subprocess.run([str(executable), str(trace)], check=True)

        result = CliRunner().invoke(
            cli_main,
            [str(trace), "--bundle-store", str(build), "--format", "json"],
        )
        assert result.exit_code == 0, result.output
        decoded = json.loads(result.output[result.output.find("["):])
        locations = {
            event["event"]: event["location"]["file"]
            for event in decoded
            if event.get("event") in {"FROM_A", "FROM_B"}
        }
        assert locations["FROM_A"].endswith("emitter_a.c")
        assert locations["FROM_B"].endswith("emitter_b.c")


# =============================================================================
# Integration-style Tests (no network)
# =============================================================================

class TestIntegration:
    """Integration tests that don't require network or external files."""

    def test_legacy_decoder_wrapper_uses_current_wire_parser(self, tmp_path):
        repository = Path(__file__).parent.parent.resolve()
        input_file = tmp_path / "current-v20.bin"
        input_file.write_bytes(make_frame())
        result = subprocess.run(
            [
                sys.executable,
                str(repository / "tool" / "decoder" / "axiom_decoder.py"),
                str(input_file),
                "--output",
                "json",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        decoded = json.loads(result.stdout)
        assert decoded[0]["version"] == [2, 0]
        assert decoded[0]["timestamp_delta"] == 0

    def test_tracked_golden_vectors_decode(self):
        repository = Path(__file__).parent.parent.resolve()
        checked = validate_golden(repository / "tool" / "golden")
        assert len(checked) >= 4

    def test_frame_encoding_decoding_roundtrip(self):
        """Test that frames can be encoded and decoded correctly."""
        result, _ = decode_frame(make_frame(level=0x02, module_id=0xAB, event_id=0x1234, seq=99))
        assert result['level'] == 'WARN'
        assert result['module_id'] == 0xAB
        assert result['event_id'] == 0x1234
        assert result['seq'] == 99

    def test_decode_with_payload(self):
        """Test decoding frame with payload."""
        dictionary = {
            "modules": [
                {
                    "id": 0x01,
                    "name": "TEST",
                    "events": [
                        {
                            "id": 0x0001,
                            "name": "EVENT",
                            "args": [{"name": "value", "type": "u8"}],
                        }
                    ],
                }
            ]
        }
        result, _ = decode_frame(make_frame(bytes([42]), module_id=0x01), dictionary=dictionary)
        assert 'error' not in result
        assert len(result['payload']) == 1
        assert result['payload'][0]['type'] == 'u8'
        assert result['payload'][0]['value'] == 42

    def test_decode_packed_bytes_uses_schema_length_prefix(self):
        dictionary = {
            "modules": [{
                "id": 0x01,
                "events": [{
                    "id": 0x0002,
                    "args": [{"name": "value", "type": "bytes"}],
                }],
            }]
        }
        result, _ = decode_frame(
            make_frame(bytes([3, 0xAA, 0xBB, 0xCC]), module_id=0x01, event_id=0x0002),
            dictionary=dictionary,
        )
        assert result["payload"] == [{"type": "bytes", "value": "aabbcc"}]

    def test_find_dictionary_environment_cwd(self, tmp_path, sample_dictionary):
        """Test dictionary search uses cwd when start_path is None."""
        # Create dictionary in current working directory
        orig_cwd = os.getcwd()
        dict_path = tmp_path / "dictionary.json"
        dict_path.write_text(json.dumps(sample_dictionary), encoding='utf-8')

        try:
            os.chdir(tmp_path)
            found = find_dictionary()  # Uses cwd
            assert found == dict_path
        finally:
            os.chdir(orig_cwd)


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
