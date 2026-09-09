# Provider Adapter Boundary

This document defines how provider adapter extensibility should evolve without making xllm core own plugin lifecycle policy.

## Current Extension Point

xllm already exposes an adapter boundary through:

- `xllm_adapter`
- `xllm_register_adapter`
- `xllm_profile.sAdapter`
- unified `xllm_request`, `xllm_response`, `xllm_error`, tool, multimodal, stream, trace, and capability models

This is sufficient for host-registered adapters. A downstream product can compile an adapter in its own module, create an `xllm_adapter`, and register it on the runtime before registering profiles.

## Recommended Boundary

xllm core should own:

- the adapter callback ABI.
- request/response/error/tool/content structs passed across the adapter boundary.
- provider capability flags and validation behavior.
- provider trace event names and diagnostic payload expectations.
- bundled adapters that are broadly useful and covered by smoke/probe baselines.

xllm core should not own by default:

- dynamic library discovery.
- plugin manifests, marketplace metadata, trust policy, or signature verification.
- sandboxing or process isolation for third-party adapters.
- provider credential vaulting beyond profile/auth fields.
- product-specific routing, tenant policy, or paid-provider selection.
- hot-reload of adapters in a running IDE.

Those concerns belong to xwork or the host product because they depend on product trust, workspace policy, UI, and deployment model.

## When To Add A Bundled Adapter

Add a provider adapter to xllm core only when all conditions are true:

- it maps cleanly to the shared xllm request/response/tool/error model.
- it has deterministic local smoke coverage or real-provider probe coverage.
- unsupported capabilities can be rejected locally or recorded in the capability matrix.
- the adapter is generally reusable outside one product.
- it does not require product-specific account, workspace, marketplace, or consent flows.

If these conditions are not true, implement the adapter as host-owned code that registers through `xllm_register_adapter`.

## Future Plugin Host Shape

If a plugin system is needed later, keep it outside the core runtime:

1. xwork or the host discovers and validates plugin packages.
2. the host loads the plugin through its chosen security model.
3. the plugin exposes a small C factory that returns `xllm_adapter` metadata and callbacks.
4. the host registers the adapter with `xllm_register_adapter`.
5. xllm continues to see only normal adapters and profiles.

This keeps the xllm ABI stable and avoids coupling model invocation primitives to IDE/plugin marketplace policy.

## Compatibility Rules

- New adapter callback fields should be appended to `xllm_adapter` and remain optional where possible.
- Existing callback semantics must stay stable after 1.0.
- Provider-specific escape hatches should use `tVendorExtra` before adding product-specific public fields.
- Capability additions must update `docs\PROVIDER_CAPABILITY_MATRIX.md` and `docs\provider_capabilities.json` when they affect bundled adapters.
- A host-owned adapter should still emit normalized `xllm_error` values and provider trace events so AI IDE / claw can remain provider-agnostic.
