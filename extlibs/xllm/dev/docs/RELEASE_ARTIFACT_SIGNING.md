# Release Artifact Signing Strategy

This document defines the signing strategy for xllm release artifacts. The current repository already generates and verifies checksums; cryptographic signing is the next supply-chain hardening step before public distribution or IDE auto-update integration.

## Current Baseline

Current release tooling provides:

- `SHA256SUMS.txt` at release output root.
- `BUNDLE_SHA256SUMS.txt` inside the bundle.
- checksum verification scripts.
- release metadata verification.
- bundle extraction and artifact verification.
- downstream integration smoke from the extracted zip.

Checksums detect corruption and accidental mismatch, but they do not prove publisher identity.

## Signing Goals

Signing should provide:

- publisher identity for release zip and checksum manifest.
- tamper evidence after artifacts leave the build machine.
- automation-friendly verification for AI IDE / claw installers.
- a path for key rotation and revocation.
- no mandatory signing dependency for local developer builds.

## Recommended Phases

Phase 1: checksum-only local engineering gate.

- Keep current default behavior.
- Continue generating `SHA256SUMS.txt` and `BUNDLE_SHA256SUMS.txt`.
- Continue verifying checksums in `release-gate` and `verify-artifact`.

Phase 2: optional detached signature gate.

- Sign `SHA256SUMS.txt` with a detached signature.
- Include signature file in release output root.
- Add a verifier script that can be skipped for local unsigned builds.
- Prefer a small cross-platform verifier path such as Minisign or Sigstore when release distribution is not Windows-only.

Phase 3: Windows distribution signing.

- Authenticode-sign Windows executables, DLLs, scripts where appropriate, and the release zip if required by product distribution policy.
- Verify signatures in release gate when signing certificate material is available.
- Keep unsigned local builds supported.

Phase 4: IDE/auto-update integration.

- The product updater verifies publisher signature and checksum before installing.
- The updater pins trusted publisher/key identity.
- Release metadata records signature file names and verification policy.

## Artifact Set

At minimum, sign or cover by a signed manifest:

- `xllm-windows.zip`
- `SHA256SUMS.txt`
- `release_metadata.json`
- `xllm-windows/BUNDLE_SHA256SUMS.txt`
- verification scripts included in the bundle

If native binaries are shipped, sign them directly when platform tooling supports it.

## Key Ownership

xllm repository should not store signing private keys.

The release host owns:

- key/certificate storage.
- signing identity.
- CI secret access.
- key rotation.
- revocation and emergency release process.

The repo should only contain signing/verification scripts and public trust metadata when appropriate.

## Verification Policy

Local developer build:

- checksums required.
- signatures optional.

Internal release candidate:

- checksums required.
- detached signature recommended.
- Authenticode optional depending on distribution channel.

Public release or IDE auto-update channel:

- checksums required.
- detached signature required.
- platform signing required when distributing native executables/DLLs through OS-trusted paths.

## Future Tooling Hook

A future `build.bat sign-artifact` or `build.bat verify-signature` entry should:

- accept explicit key/certificate inputs from CI or local secure storage.
- never prompt for secrets in normal release-gate runs.
- emit machine-readable report fields into `release_metadata.json`.
- fail closed for public release mode when a required signature is missing.

Until that tooling exists, release notes must state that artifacts are checksum-verified but not publisher-signed.
