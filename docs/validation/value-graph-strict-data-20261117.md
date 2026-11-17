# Strict data graph projection

`XVALUE_GRAPH_COPY_STRICT_DATA_V1` is an additive flag for
`xrtValueGraphCopyV1`, used together with `XVALUE_GRAPH_COPY_DATA_V1`.
It rejects `XVALUE_POINTER` and unhandled `XVALUE_HANDLE` with
`XERR_UNSUPPORTED`. It neither invokes opaque Clone callbacks nor retains
unsupported resources as a fallback. Existing DATA and DeepClone behavior
remains unchanged, and the configuration struct and original V1 prefix keep
their physical ABI.

Explicit handle/object adapters remain trusted projections. They recurse
through Next, sharing the enclosing traversal's memo, rollback and guards.
Adapters must produce data; the flag is not a sandbox for arbitrary adapter
code. Immutable scalar leaves may share their existing reference. Containers
are independent, with repeated child identities preserved inside the result.

The matching modular and single-header tests passed on Windows and Linux
with GCC O2. The Linux modular test also passed AddressSanitizer and UBSan.
The test covers pointer/opaque refusal, nested refusal, source preservation,
trusted adaptation with the same memo, unchanged old fallback, invalid flags,
100 failed partial traversals and the complete seven-position allocation
failure prefix. Existing graph, contract, OOM, scalar-domain and key callback
regressions also passed. External evidence is indexed in the sibling JSON.

This qualifies the native graph flag. The xlang declarative argument
contract and model defaults require separate compiler and binding validation.
