# Exact identifier comparison keys

Select `XRPC_MODULE_XRPC_ID`. `xrpcIdKeySize` and `xrpcIdKeyWrite` independently
validate a complete JSON string, number or null and produce an opaque binary
comparison key in caller-owned storage. No pending map, request, future or
transport is created. Retain the original raw ID separately for wire encoding;
the key is neither JSON nor a persistent interchange format.

The [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification) permits
string/number/null IDs and requires responses to preserve their value. xrpc's
correlation policy uses decoded string byte equality and **exact decimal numeric
equality**. Therefore `1`, `1.0` and `1e0` have equal keys, all signed zeros have
equal keys, while number `1`, string `"1"` and null stay in distinct domains.
String NUL/escapes and surrogate pairs decode exactly; there is no Unicode
normalization or case folding. Fractional IDs remain supported despite the
protocol's interoperability recommendation. Null is explicit and distinct from
missing ID; runtimes still need a policy for unknown-id error responses.

No number is converted to int64 or double, no exponent is saturated, and no
power of ten is expanded. A nonzero decimal becomes significant digits plus an
exact signed decimal exponent. Leading/trailing mantissa zeros, the fractional
digit count and arbitrary-size signed exponents are accounted for with decimal
addition/subtraction. Only the bounded byte-count offset uses size_t. A fixed
small digit array and right-to-left arithmetic measure and emit the exponent in
linear passes, even across native integer overflow boundaries. Zero does not
need exponent materialization. This avoids rounding, overflow-to-infinity and
underflow-to-zero collisions. The grammar/number interoperability context is
[RFC 8259 section 6](https://www.rfc-editor.org/rfc/rfc8259.html#section-6).

Defaults cap complete input and decoded string bytes at 4096, and key bytes at
8192. These independent finite bounds are copied from config. Unknown types,
malformed JSON/UTF-8/unpaired surrogates, trailing data, limits and diagnostic OOM
are local failures; this leaf generates no wire error. Measurement and Write
preflight separately. Size outputs and output buffers remain unchanged on every
failure. Key/output-size ranges cannot overlap input/config/each other. Success
writes exactly the measured bytes without a terminator and preserves ambient
error. Keep inputs alive and unchanged during the synchronous call.

The ID leaf owns no locks, threads, retained views or ordinary-path heap storage.
The caller supplies key storage. Cold diagnostics may allocate; existing parser
and error-reference bookkeeping still occurs and can synchronize.
Input is scanned by the public raw JSON scanner. String measurement/emission
each traverse the validated token through the shared private string iterator;
Unicode escapes use public scalar encoding. Numeric analysis scans the mantissa,
counts significant digits and measures signed exponent arithmetic; emission
copies digits and repeats bounded exponent arithmetic. A separate Size call adds
its own validation/measurement. These passes, buffer writes and diagnostic
bookkeeping are real costs, not a zero-overhead claim. The key format is opaque,
not numerically sorted and may change with an incompatible implementation version.

The [example](../examples/id/main.c) shows equivalent huge-exponent and embedded
NUL string spellings. Workloads separately measure escaped string, ordinary
decimal and thousand-digit-exponent KeyWrite. Throughput excludes per-ID clocks;
a separate latency phase includes two xrtTimer calls. Correlation maps, scheduling,
pending deadlines/cancellation, duplicate-response policy and transport/server
performance remain required later work.

`python tools/test_xrpc_id_semantics.py` builds a standalone ID-only C probe and
checks 20000 deterministic pairs generated with independent Python arbitrary-
precision exponent arithmetic, including unequal controls. Use `--sanitize` on
a compiler supporting ASan/UBSan. Each run pins source/compiler/binary hashes,
generated input and actual results under out/service-extensions/xrpc-id/. This
adds exponent/scale coverage beyond the unit tests' bounded rational oracle.
