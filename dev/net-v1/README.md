# XNET V1 Freeze Snapshot

## Purpose

This directory is the frozen reference snapshot for the pre-rewrite xrt
network stack.

It exists so the `xnet-v2` rewrite can move forward without losing:

- the legacy transport implementation
- the old public network shape
- the legacy tests used as behavioral reference
- the sample programs used to compare ergonomics and behavior

## Freeze Metadata

- Freeze date: `2026-03-12`
- Source commit: `64be42690871b3d635e8f459932165fc16b33f70`
- Freeze mode: copy-based snapshot
- Rewrite spec: [../XNET_V2_SPEC.md](/D:/Git/xrt/dev/XNET_V2_SPEC.md)

## Snapshot Policy

- This snapshot is reference-only.
- Do not evolve `net-v1` here as part of the `xnet-v2` rewrite.
- If legacy behavior needs to be understood, diff against these files instead
  of relying on memory.
- If additional notes are needed, add notes or logs here instead of editing the
  copied implementation files.

## Included Content

- `lib/`: copied legacy network implementation headers
- `test/`: copied legacy network tests
- `docs/`: copied legacy network API docs
- `examples/network/`: copied legacy network samples
- `snapshot/`: copied top-level integration snapshots (`xrt.h`, `xrt.c`,
  `test.c`)

## Exclusions

- generated objects and binaries should not be preserved in this snapshot
- the older `dev/net/` experimental code is not part of this freeze snapshot

## Notes

- This freeze records the legacy system as it existed before `xnet-v2`
  implementation work began.
- The active build still points at the current live tree until `xnet-v2`
  replaces it.
