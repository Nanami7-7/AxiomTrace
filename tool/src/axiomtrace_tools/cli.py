"""AxiomTrace command line tools."""

from __future__ import annotations

from pathlib import Path

import click

from axiomtrace_tools.bundle import find_bundle, find_bundle_in_store, generate_bundle, load_bundle
from axiomtrace_tools.codegen import generate_assets, load_event_source, validate_event_source
from axiomtrace_tools.decoder import decode_stream, extract_metadata_id, find_dictionary
from axiomtrace_tools.dictionary import load_dictionary
from axiomtrace_tools.render import render_json, render_jsonl, render_raw_json, render_text
from axiomtrace_tools.validator import (
    validate_bundle,
    validate_dictionary_file,
    validate_golden,
    validate_trace_bundle,
)


OUTPUT_FORMATS = ["text", "json", "jsonl", "raw"]

BANNER = r"""
    _              _                 _____
   / \\   __  __  _(_)  ___   _ __   |_   _| _ __  __ _   ___  ___
  / _ \\  \\ \\/ / (_)_  / _ \\ | '_ \\    | |  | '__|/ _` | / __|/ _ \\\\
 / ___ \\  >  <   | | | (_) || | | |   | |  | |  | (_| || (__|  __/
/_/   \\_\\/_/\\_\\  |_|  \\___/ |_| |_|   |_|  |_|   \\__,_| \\___|\\___|
  AxiomTrace host-side tools
"""

def echo_success(msg: str, err: bool = False):
    click.secho(f"# SUCCESS: {msg}", fg="green", bold=True, err=err)

def echo_info(msg: str, err: bool = False):
    click.secho(f"# INFO: {msg}", fg="blue", bold=True, err=err)

def echo_warning(msg: str, err: bool = False):
    click.secho(f"# WARNING: {msg}", fg="yellow", bold=True, err=err)

def echo_error(msg: str, err: bool = False):
    click.secho(f"# ERROR: {msg}", fg="red", bold=True, err=err)


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.argument("input", type=click.File("rb"))
@click.option("--dictionary", "-d", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Path to dictionary.json. Kept for compatibility; --bundle is preferred.")
@click.option("--bundle", type=click.Path(exists=True), default=None,
              help="Path to axiomtrace-bundle directory or manifest.json.")
@click.option("--bundle-store", type=click.Path(exists=True, file_okay=False), default=None,
              help="Directory containing metadata-id keyed bundles.")
@click.option("--format", "output_format", type=click.Choice(OUTPUT_FORMATS), default=None,
              help="Output format.")
@click.option("--output", "-o", "legacy_output", type=click.Choice(OUTPUT_FORMATS), default=None,
              help="Output format compatibility alias.")
@click.option("--output-file", type=click.Path(dir_okay=False), default=None,
              help="Write rendered output to a file instead of stdout.")
def decoder_main(input, dictionary, bundle, bundle_store, output_format, legacy_output, output_file):
    """AxiomTrace binary frame decoder."""
    selected_format = output_format or legacy_output or "text"
    raw_bytes = input.read()
    frames = decode_stream(raw_bytes)
    if selected_format == "raw":
        rendered = render_raw_json(frames)
        _emit_output(rendered, output_file)
        return

    trace_metadata_id = extract_metadata_id(frames)
    active_bundle = _resolve_bundle(bundle, bundle_store, trace_metadata_id, dictionary)
    active_dictionary = None
    active_metadata_id = trace_metadata_id
    active_source_map = None

    if active_bundle:
        try:
            loaded_bundle = validate_bundle(active_bundle)
            active_dictionary = loaded_bundle.load_dictionary()
            frames = decode_stream(raw_bytes, active_dictionary)
            validate_trace_bundle(frames, loaded_bundle)
        except ValueError as exc:
            raise click.ClickException(str(exc)) from exc
        active_metadata_id = loaded_bundle.metadata_id
        active_source_map = loaded_bundle.load_source_map()
        echo_info(f"Using metadata bundle: {loaded_bundle.root}", err=True)
    else:
        dict_path = Path(dictionary) if dictionary else find_dictionary()
        if dict_path and dict_path.is_file():
            active_dictionary = load_dictionary(dict_path)
            frames = decode_stream(raw_bytes, active_dictionary)
            echo_info(f"Using dictionary: {dict_path}", err=True)
        else:
            raise click.ClickException(
                "semantic output requires a metadata bundle or dictionary; use --format raw for structural decode"
            )

    rendered = _render(frames, selected_format, active_dictionary, active_metadata_id, active_source_map)
    _emit_output(rendered, output_file)


def _emit_output(rendered: str, output_file: str | None) -> None:
    if output_file:
        Path(output_file).write_text(rendered + ("\n" if rendered else ""), encoding="utf-8")
    elif rendered:
        click.echo(rendered)


def _resolve_bundle(
    bundle: str | None,
    bundle_store: str | None,
    metadata_id: str | None,
    dictionary: str | None,
) -> Path | None:
    if bundle:
        return Path(bundle)
    if bundle_store:
        if not metadata_id:
            raise click.ClickException("--bundle-store requires a trace containing a metadata identity event")
        found = find_bundle_in_store(bundle_store, metadata_id)
        if not found:
            raise click.ClickException(f"no bundle found for metadata id {metadata_id}")
        return found
    if dictionary:
        return None
    return find_bundle()


def _render(frames, output_format, dictionary, metadata_id, source_map):
    if output_format == "json":
        return render_json(frames, dictionary, metadata_id, source_map)
    if output_format == "jsonl":
        return render_jsonl(frames, dictionary, metadata_id, source_map)
    return render_text(frames, dictionary, metadata_id, source_map)


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.option("--events", "--input", "events", required=True, type=click.Path(exists=True, dir_okay=False),
              help="Path to events.yaml/events.json.")
@click.option("--out", "--output-dir", "out", required=True, type=click.Path(file_okay=False),
              help="Output directory for generated headers and dictionary.json.")
@click.option("--source-id-map", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Stable per-source IDs used by file_id location mode.")
@click.option("--location-mode", type=click.Choice(["none", "hash", "file_id"]), default="none",
              help="Location metadata mode represented by generated assets.")
@click.option("--location-function", is_flag=True,
              help="Include function hashes in hash-mode metadata identity.")
@click.option("--check", is_flag=True, help="Validate only; do not write generated files.")
def codegen_main(events, out, source_id_map, location_mode, location_function, check):
    """Generate C headers and dictionary.json from event definitions."""
    generated = generate_assets(
        events,
        out,
        check_only=check,
        source_id_map=source_id_map,
        location_mode=location_mode,
        location_function=location_function,
    )
    if check:
        echo_success("AxiomTrace codegen check passed")
        return
    for name, path in generated.items():
        echo_success(f"Generated {name}: {path}")


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
def bundle_main():
    """AxiomTrace metadata bundle commands."""


@bundle_main.command("generate")
@click.option("--events", required=True, type=click.Path(exists=True, dir_okay=False),
              help="Path to events.yaml/events.json.")
@click.option("--elf", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Optional firmware ELF artifact.")
@click.option("--map", "map_file", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Optional firmware map artifact.")
@click.option("--compile-db", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Optional compile_commands.json for source_map.json.")
@click.option("--source-id-map", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Configured source IDs for deterministic file_id mode.")
@click.option("--out", required=True, type=click.Path(file_okay=False),
              help="Output axiomtrace-bundle directory.")
@click.option("--firmware-name", default=None, help="Firmware name for manifest/build_info.")
@click.option("--firmware-version", default=None, help="Firmware version for manifest/build_info.")
@click.option("--location-mode", type=click.Choice(["none", "hash", "file_id"]), default=None,
              help="Location mode; inferred from sources when omitted.")
@click.option("--location-function", is_flag=True,
              help="Include function hashes in hash-mode metadata identity.")
def bundle_generate(
    events, elf, map_file, compile_db, source_id_map, out, firmware_name, firmware_version,
    location_mode, location_function,
):
    """Generate a standard axiomtrace-bundle."""
    generated = generate_bundle(
        events=events,
        out=out,
        elf=elf,
        compile_db=compile_db,
        source_id_map=source_id_map,
        map_file=map_file,
        firmware_name=firmware_name,
        firmware_version=firmware_version,
        location_mode=location_mode,
        location_function=location_function,
    )
    for name, path in generated.items():
        echo_success(f"Bundled {name}: {path}")


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.option("--bundle", type=click.Path(exists=True), default=None,
              help="Bundle directory or manifest.json to validate.")
@click.option("--dictionary", "-d", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Dictionary to validate when no bundle is provided.")
@click.option("--events", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Event source to validate.")
@click.option("--golden", type=click.Path(exists=True, file_okay=False), default=None,
              help="Golden directory containing frames and expected raw JSON.")
@click.option("--trace", type=click.Path(exists=True, dir_okay=False), default=None,
              help="Trace to validate against --bundle metadata identity and payload schema.")
def validate_main(bundle, dictionary, events, golden, trace):
    """Validate event metadata and bundle wiring."""
    try:
        loaded = None
        if bundle:
            loaded = validate_bundle(bundle)
            echo_success(f"Bundle validation PASSED: {loaded.root}")
        if dictionary:
            validate_dictionary_file(dictionary)
            echo_success(f"Dictionary validation PASSED: {dictionary}")
        if events:
            validate_event_source(load_event_source(events))
            echo_success(f"Event definitions validation PASSED: {events}")
        if golden:
            checked = validate_golden(golden)
            echo_success(f"Golden vectors validation PASSED: {len(checked)} frame(s) matches expected JSON outputs")
        if trace:
            if loaded is None:
                raise ValueError("--trace requires --bundle")
            active_dict = loaded.load_dictionary()
            validate_trace_bundle(decode_stream(Path(trace).read_bytes(), dictionary=active_dict), loaded)
            echo_success(f"Trace matches bundle metadata identity & payload schema: {trace}")
        if not any([bundle, dictionary, events, golden, trace]):
            raise ValueError("nothing to validate")
    except ValueError as exc:
        raise click.ClickException(str(exc)) from exc


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
@click.pass_context
def tool_main(ctx):
    """AxiomTrace tool multiplexer."""
    if ctx.invoked_subcommand is None:
        click.echo(BANNER)


tool_main.add_command(decoder_main, "decode")
tool_main.add_command(codegen_main, "codegen")
tool_main.add_command(bundle_main, "bundle")
tool_main.add_command(validate_main, "validate")


# Backward-compatible import used by existing tests.
main = decoder_main


if __name__ == "__main__":
    tool_main()
