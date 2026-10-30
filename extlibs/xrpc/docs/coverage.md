# Envelope and synchronous dispatch contracts

These leaves implement JSON-RPC 2.0 document/envelope decoding, encoding and
synchronous request dispatch and serialized pending-call admission/removal on public XRT APIs. The [full product scope](../../../docs/development/service-extensions-plan.md)
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

| Method index contract | Verification |
| --- | --- |
| Semantic equality | Every Unicode scalar in raw UTF-8 and escaped BMP/surrogate spelling, embedded NUL, all simple escapes, case and distinct normalization forms |
| Immutable lookup | Freed/mutated registration names/vector, 4096 unsorted methods, explicit 32-bit fingerprint collision pair and empty table |
| Complete validation | Invalid UTF-8/surrogates, trailing tokens, scalar/container type errors and empty-index malformed tails preserve binding output |
| Bounded construction/query | Exact and rejected name/total/count/owned/raw bounds, duplicate/reserved/NULL registration and invalid argument spans |
| Dispatch integration | Escaped-name call preserves huge ID; local query limit aborts calls/notifications before handler execution and emits no method error |
| Allocation/lifetime | 10000 hit/miss loops preserve allocation counters/ambient identity, one-byte lookup quota, diagnostic OOM and 511 constructor quota windows |
| Concurrent ownership | Four retained-reader threads, 80000 lookups after the creator releases its reference, final release after physical reader completion |

| Identifier key contract | Verification |
| --- | --- |
| Exact decimal equality | Literal equivalent/unequal spellings, signed zeros, fractional positions, 2^53 neighbors, overflow/underflow and native-integer-boundary exponents |
| Arbitrary exponent arithmetic | Long positive carry/negative borrow and sign/offset cancellation without materializing powers of ten |
| String/type domains | All Unicode scalars against independent UTF-8 oracle, all escapes/NUL, case/normalization distinctions and null/string/number separation |
| Independent rational properties | 20000 exact small rational comparisons plus equivalent coefficient/exponent shifts |
| Bounds/publication | Input/string/key bounds, exact capacity/canaries and every shorter capacity, invalid grammar/types/configs and input/config/output-size aliases |
| Allocation/errors | 10000 ordinary string/large-number loops preserve allocation counts and ambient identity, one-byte quota and diagnostic OOM |

The benchmark observes decoded fields and checks a completion checksum. It is
a workload smoke until a pinned baseline and end-to-end runtime exist. Current
platform evidence and remaining gates are in the [progress ledger](../../../docs/development/service-extensions-progress.md).

| Pending-call table contract | Verification |
| --- | --- |
| Exact IDs and key ownership | Numeric/string/null separation, equivalent decimal/huge exponents, embedded NUL escapes, overwritten raw input |
| Single removal and local ABA | Response vs stale cancellation/expiry, duplicate responses, reused slots with never-reused serials |
| Admission and deadline order | Full/duplicate precedence, exact due ticks, equal-deadline serial order, never deadlines and shutdown draining |
| Independent operation model | 100000 public API operations against a linear reference model, checking outputs, counts and earliest deadline after every operation |
| Bounded structure | Ascending/descending/permuted 4096-entry mutation; separate 50000 private AVL/heap/free-list invariant checks and forced 64-bit serial exhaustion |
| Failure publication and bounds | Config/owned-byte/key/string/input limits, malformed IDs even full, aliases, missing/error outputs unchanged |
| Allocation/OOM | 10000 steady cycles with unchanged allocation counters/ambient diagnostic, constructor OOM and diagnostic OOM under one-byte quota |

The pending table requires one serialized owner. It does not arbitrate concurrent
access internally or classify response envelopes, and local tokens cannot resolve
remote late responses after wire ID reuse. Response-batch preflight and native
Future completion are covered below; runtime/connection ownership, async
scheduling and transport recovery remain pending.

| Response ownership and correlation contract | Verification |
| --- | --- |
| Immutable owned document | Complete response-mode validation, all descriptor views rebased, original input freed/overwritten, raw huge IDs and embedded-NUL payloads |
| No partial document publication | Malformed/invalid late items, request/scalar/empty batch, exact/rejected capacities and byte limits |
| Strong-reference lifetime | Extra retained refs, creator release before four readers finish 80000 reads, separate forced-overflow private probe |
| Complete correlation preflight | Late bad ID/key/capacity/diagnostic/allocation failures preserve all pending calls and final metadata |
| Independent outcomes | 10000 public pending-batch linear-model operations plus 10000 receive batches with matched/repeated/unknown IDs and observed call tokens |
| Costs and errors | One snapshot allocation per document, zero additional table allocations, ordinary ambient-error identity, one-byte OOM and alias checks |

Response snapshots own payload bytes; resolution User pointers remain borrowed.
A serialized owner completes calls after receive returns. Aggregate retained-result
quotas, connection epochs, executors, transport framing and async shutdown remain
runtime work. No end-to-end performance or final production acceptance follows.

| Native call/Future contract | Verification |
| --- | --- |
| Typed terminal/knowledge | Every status, sent/unsent/replied combinations, remote errors, late response after deadline, explicit retained cause vs stale ambient diagnostic |
| Cooperation and deadline | Future cancellation wakes once without fabricated terminal state; raw-token requests require owner polling, owner Poll, immediate/forever deadline and deadline priority |
| Ownership and publication | Independent result/context lifetime, dropped observer preserves work, uncompleted Destroy closes, retained payload after source/producer release |
| Notification retirement | Reentrant finish/destroy and destroy-in-progress callbacks, ambient identity after notification/context-drop diagnostics, controlled suspended cancel callback and captured-reference retirement |
| Failure boundaries | 4096 constructor quota prefixes, success-only context transfer, no leaked budget bytes, invalid kind/index/aliases and preallocated finish under one-byte quota |
| Integrated correlation | Shuffled response IDs through pending batch/Receive into four independent Futures, duplicate/unknown suppression, shared payload surviving all producer contexts |

Async operation lifetime is locally testable without sockets. This does not prove
HTTP/WebSocket/stdio transport, physical network close, executor behavior, aggregate
retained-result quotas or end-to-end production/performance acceptance.


| Ordinary HTTP/TCP call contract | Verification |
| --- | --- |
| Real runtime | IOCP/epoll loopback, fixed worker affinity, persistent queued calls, copied caller params and retained results after engine cleanup |
| Framing/correlation | Fragmented reads, fixed/chunked/EOF bodies, informational responses/trailers, matching IDs, remote errors on HTTP 500, invalid ID/framing/truncation/encoding |
| Limits/admission | Count/byte overload preserves output/error, HTTP head/body/trailer and JSON/owned-snapshot caps, invalid nested configuration and output/client alias |
| Deadline/cancellation | Immediate unsent timeout, queued cancellation, sent cancel/timeout retires connection without replay or sending cancelled queued work |
| Ownership/concurrency | Four concurrent submission threads, completion-close/release reentry, request/timer/captured-post accounting and engine join |
| Allocation failure | 4096 request-construction quota prefixes, separate factory quota thresholds, success/failure publication and zero live budget bytes after cleanup |

This is the initial ordinary call slice. Notification/batch networking, TLS/DNS,
other transports/server, aggregate retained-result admission, independent peers
and final pinned/native/long-run acceptance remain pending. Closed excludes the
publishing stack's mechanical reference tail; engine join retires that stack.
