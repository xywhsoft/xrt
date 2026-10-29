# Envelope and synchronous dispatch contracts

These leaves implement JSON-RPC 2.0 document/envelope decoding, encoding and
synchronous request dispatch on public XRT APIs. The [full product scope](../../../docs/development/service-extensions-plan.md)
remains in force; no async runtime/transport completion is implied.

| Contract | Verification |
| --- | --- |
| Calls/notifications/responses | Handwritten descriptor assertions in request/response/duplex modes |
| Lossless ids and payloads | Huge integers/exponents, fractional ids, explicit null vs missing, NUL string escapes, complete array/object slices |
| Protocol header interpretation | Escaped key/version names, semantic duplicate aliases, case-sensitive comparisons and reserved method prefix |
| Batch shape | Mixed valid/invalid items, scalar roots, nested arrays, empty-batch single error |
| Error objects | Required integer code/string message, nested data, duplicate error fields and huge exponent handling |
| Publication boundary | Malformed tail/batch discards all descriptors; Document.Count is zero on parse failure |
| Local errors | Independent capacity/message/byte/depth/value/item/string/id/method limits and API span/alias rejection |
| Allocation and lifetime | 10,000 repeated decodes with unchanged allocation counters/error identity; borrowed views; one-byte quota and diagnostic OOM classification |
| Property/adversarial coverage | 10,000 generated mixed batches, 10,000 exact rational integer-code cases, 20,000 raw/mutated documents with bounded views and publication checks |
| Distribution | Modular/single tests, example, independent static/shared consumer and dedicated performance/size profiles |

Document is unchanged on LOCAL_ERROR. Scratch is deliberately not an atomic
output: it may contain partial descriptors on any failure and must be discarded.
DONE may contain INVALID entries; runtimes must not execute them. Valid
notifications must never produce responses, including handler errors.

| Encode contract | Verification |
| --- | --- |
| Requests/notifications/results/errors | Exact wire fixtures, explicit null/absent, required/unused fields, request/response batch role checks |
| Escaping and exact raw values | Embedded-NUL/control/UTF-8/quote/backslash text; huge/fractional/escaped ids; preserved raw whitespace and payload duplicate keys |
| Independent output oracle | 10,000 deterministic generated request/response batches with a test-only byte quote oracle; modular round trips also classify every message |
| Invalid local input | Malformed JSON/trailing injection, wrong field types, nonintegral error codes, invalid UTF-8/surrogates and late batch failures |
| Whole-document limits | Byte, depth, string/key, total value, per-container item, message, id and encoded-method caps, with exact-boundary passes |
| Atomicity and aliases | Every capacity below required size; invalid/wrapping spans; vector/config/each live-view/wire/size overlap; 10,000 raw/mutated JSON cases |
| Allocation/OOM | 10,000 Size+Write repetitions preserve allocation counters/ambient error; success under one-byte quota; diagnostic OOM keeps all outputs unchanged |
| Distribution | Standalone encode module/single test/example, root modular round trips, independent consumer and dedicated performance/size profiles |

Encoding emits complete nonempty documents only; it does not decide response
eligibility or suppress handler errors for notifications. Callers send nothing
for zero responses. Raw values are not trusted: every Write preflights them
even after a successful Size. The inputs must remain unchanged throughout the
synchronous call; there are no user callbacks between validation and writing.
The first pass is the failure boundary. Validated emission needs no allocation,
raw parse or external callbacks. See the README for the actual extra text scans
and diagnostic reference operations.

| Synchronous dispatch contract | Verification |
| --- | --- |
| Full validation before execution | Malformed tails after valid calls and 1000 generated batch tails invoke no business callback |
| Standard errors and exact IDs | Handwritten error/result/batch fixtures, huge/fractional/negative-zero/NUL-escaped ids, semantic reserved prefix, explicit null |
| Notification suppression | Known/unknown/reserved/error handlers and intentionally inaccessible suppressed payloads; all-notification batches emit nothing |
| Callback lifetime and reentry | Stack response consumed before overwrite, independent nested dispatch, duplicate replies and sink reentry rejected |
| Sticky local failure and partial progress | Invalid output/integral code, false return after reply, rejected sink with stale ambient error, no later business callback, accepted output counts |
| Explicit local abort | No wire error before/after reply or for notifications; sink abort/repeated abort, explicit/current diagnostic ownership, NULL ignores stale errors, diagnostic OOM and package consumers |
| Bounded admission/output | Scratch, input/message/reply caps before callbacks; exact aggregate bytes including brackets/commas; per-object bounds and notification-only zero output |
| API ownership | Invalid configs/spans, scratch/input/config/info overlap, NULL callbacks/outputs; no retained request/reply context |
| Independent property oracle | 10000 generated mixed batches with handwritten expected response construction |
| Allocation and diagnostics | 10000 repeated ordinary calls with unchanged allocation counters/ambient error; handler diagnostic isolation; one-byte quota and parse/reply diagnostic OOM |
| Distribution | Dedicated modular/single tests, example, package consumer and performance/size profiles |

The benchmark observes decoded fields and checks a completion checksum. It is
a workload smoke until a pinned baseline and end-to-end runtime exist. Current
platform evidence and remaining gates are in the [progress ledger](../../../docs/development/service-extensions-progress.md).
