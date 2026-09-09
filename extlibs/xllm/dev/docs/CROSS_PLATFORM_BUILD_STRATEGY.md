# Linux / macOS Build Strategy

xllm is currently Windows-first. This document defines the path to Linux/macOS support without weakening the current Windows release gate.

## Current Baseline

The active build and release flow assumes:

- Windows.
- `build.bat` as the repo-root entry.
- PowerShell scripts for build orchestration, smoke matrix, eval, release, and verification.
- MinGW-w64 `gcc` on `PATH`.
- Windows release bundle layout named `xllm-windows`.
- optional Windows-built `sqlite-vec.dll`.

The C library code is intended to remain portable where practical, but the validated toolchain is Windows-first.

## Strategy

Keep Windows as the release baseline while preparing portable layers:

- Keep public C headers and implementation free of avoidable Windows-only API assumptions.
- Keep platform-specific behavior behind xrt, transport, or release tooling boundaries.
- Prefer C11-compatible code for core/session/memory.
- Preserve single-header generation as a platform-neutral artifact where possible.
- Add Linux/macOS CI only after a minimal native build script exists.

## Non-Goals For Current Phase

- No claim that Linux/macOS release artifacts are supported.
- No replacement of the Windows release gate.
- No mandatory CMake migration before the Windows bundle is stable.
- No bundled external vector DB or platform package manager dependency.

## Required Work Before Claiming Support

1. Add a platform-neutral build entry.

   Candidate paths: `build.ps1` with PowerShell Core, `build.sh`, or CMake. The first target should compile core/session/memory and a minimal smoke on Linux/macOS.

2. Audit xrt/platform dependencies.

   Confirm file IO, threads, sockets/TLS, time, mutexes, process handling, and dynamic loading paths work with the intended compiler/runtime.

3. Port smoke matrix orchestration.

   The current smoke scripts are batch/PowerShell-oriented. Linux/macOS need either PowerShell Core compatibility or shell/CMake equivalents.

4. Validate SQLite behavior.

   Confirm bundled SQLite compile, WAL behavior, busy timeout, crash consistency, and path handling on POSIX filesystems.

5. Validate optional dependencies.

   ONNX Runtime, E5 assets, sqlite-vec equivalent shared library, and proxy/network probes need platform-specific setup notes.

6. Add CI.

   Start with compile-only and small local smoke. Do not run real-provider probes by default.

7. Define release layout.

   Use platform-specific bundle names such as `xllm-linux` and `xllm-macos`; do not overload `xllm-windows`.

## Minimum Acceptance Gate

Before marking Linux/macOS as supported:

- `singlehead` equivalent succeeds.
- core/session/memory minimal smoke succeeds.
- memory SQLite ingest/search/reopen smoke succeeds.
- static-analysis equivalent runs at least syntax-only compile coverage.
- release artifact verification exists for that platform.
- docs list compiler, shell, and dependency requirements.

## Recommended Order

1. Keep Windows gate green.
2. Add Linux compile-only for `xllm.c`, `xllm-session.c`, and `xllm-memory.c`.
3. Add one core smoke and one memory SQLite smoke.
4. Add single-header generation on Linux.
5. Add macOS after Linux C/SQLite path is stable.
6. Add platform release bundles only after CI is stable.

## Risks

- PowerShell scripts may rely on Windows path and `cmd.exe` behavior.
- xrt platform code may expose compiler warnings or missing POSIX paths.
- SQLite WAL and file locking semantics differ by filesystem.
- ONNX Runtime and sqlite-vec packaging differ across platforms.
- Release signing requirements differ by OS.

Until these are addressed, README and release notes should continue to describe xllm as Windows-first.
