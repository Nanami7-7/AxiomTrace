> [English](versioning.md) | [简体中文](versioning_zh.md)

# AxiomTrace Versioning & Compatibility

## 1. Semantic Versioning

AxiomTrace follows [SemVer 2.0.0](https://semver.org/):

- **MAJOR**: Incompatible ABI or wire format changes.
- **MINOR**: Backward-compatible feature additions.
- **PATCH**: Bug fixes, documentation, performance improvements.

## 2. Wire Format Version

The `version` byte in the event header encodes `major << 4 | minor`.

- Decoders must reject unsupported **major** versions.
- **Minor** version additions must preserve the payload interpretation of the current major version.

The current emitted wire version is `v2.0` (`0x20`): normal arguments are packed according to the metadata dictionary. The decoder also supports historical `v1.0` and `v1.1` typed-payload frames.

## 3. API Stability

| API Surface                     | Stability Guarantee               |
|---------------------------------|-----------------------------------|
| `AXIOM_*` logging macros        | Stable from v0.1                  |
| `axiom_port_*` weak symbols     | Stable from v0.1                  |
| `axiom_backend_register` fn     | Stable from v0.1                  |
| Internal `_Generic` encode fns  | May change in minor releases      |
| `axiom_ring_*` internal API     | May change in minor releases      |

## 4. ABI Compatibility

- Header size and field offsets are frozen per major version.
- Wire v2 metadata suffix tags are appended after packed arguments; ordinary argument types are defined by the matching dictionary.
- Backend descriptor struct may grow at the end in minor releases; use `AXIOM_BACKEND_INIT(...)` for forward-compatible initialization. Old zero-initialized backends (size==0) are treated as legacy layout.
