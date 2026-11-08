# Migrating agent libraries to the consolidated mainline

Consumers must rebuild xllm, xllm-session, xwork and every native/SDK component together.
xllm 3.2 preserves the mainline relative-millisecond request timeout API while
adopting the product's absolute monotonic-microsecond deadline API. The public
request layout changes; old binaries cannot use the new record definition.

`xllmRequestSetTimeout` captures its total budget when Start/Complete is called.
That budget includes retries and backoff. `xllmRequestSetDeadline` accepts an
already established monotonic-microsecond deadline. The last setter chooses
which form applies. Native XRT waits still receive signed relative milliseconds.

Client profiles and hook tables are copied at publication. Call
`xllmClientSetHooks` after modifying a hook table; changing the caller's old
record does not change an active or published client snapshot. Request-specific
`request.pHooks` remains a per-call override with the documented copy lifetime.

Assistant replay retains ordered reasoning, thinking signatures and visible
text together. Use message parts rather than assuming that signed reasoning is
the only part or that all visible text is always in sContent.

The migrated xllm-session reads legacy snapshots and journals through the
schema-4 migration path. xwork uses ABI 9 and event schema 3. Uncertain mutating
calls require an explicit recovery decision; completed recoverable reads use
verified receipts. Hosts must retain their clients, sessions and callback data
until all consumers retire, using the supplied owner callbacks where needed.

The modular source tree and generated single headers are authoritative. Flat
XS/TCC SDK sources are exported by `tools/export_agent_libraries.py`; record the
source commit and production-tree hashes when rebuilding a product host.
