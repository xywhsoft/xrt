# APP-driven message endpoint

Select `XRPC_MODULE_XRPC_DUPLEX` for a bounded JSON-RPC endpoint. Both peers can
originate calls and receive requests at the same time. The APP owns the
transport, input framing, execution context and progress loop.

## Owner and APP request tokens

Use one serialized owner for Receive, Reply, Step, output selection and
destruction. Call, Notify, Batch and Close accept concurrent callers. A
configured Wake posts or signals the owner and can run on a submitter or
cancellation thread. Completion watches can run on the owner. Reentrant
owner mutation is refused.

Set OnRequest for a peer that handles incoming work. The handler receives
already parsed message descriptors and the original input and context views.
They are valid during the callback. Copy the inputs needed by a deferred job.
The endpoint reserves its own bounded record of the expected IDs and shape.
A false handler return releases that credit. The handler transfers an accepted
token to the APP, which may schedule work on its own executor.

After the handler returns, send a reply from the owner with that token.
Preserve the exact raw IDs in request order and the enclosing object/array
shape. Omit notifications. A notification-only job completes with an empty
span. A malformed document supplies no decoded descriptors and expects a
Parse error with a null ID. xrpcDispatch provides standard sync handling
including errors and notification suppression. The runnable APP copies a
bounded job and calls xrpcDispatch outside the endpoint callback.

Wrong reply shape/ID and output quota exhaustion leave the token available
for correction or retry. Retry publication of the encoded bytes after credit
returns. The APP retires its own work on close. The endpoint releases
unanswered tokens during Step.

## Output and deadlines

Next transfers an immutable wire reference. Call Started immediately before
the first write offer, then Sent after the entire selected document has been
accepted by the transport. Release every output reference when the physical
borrow finishes. Output references and retained replies can outlive the
endpoint. A notification result reports its transport acceptance.

Step retires timed-out and cancelled calls. An expired or cancelled partial
send closes the endpoint and the APP must stop using that transport.
A whole sent request can still execute remotely after local cancellation.
A result distinguishes local refusal, not sent, possibly executed, and
received reply. The APP drives at the next monotonic deadline and on Wake.

A valid remote response batch can arrive in any order. The pending table
matches the complete document before committing reply associations. The
endpoint preserves typed capacity and lifetime limits for pending IDs,
in-flight APP work, retained outputs and retained results.

## Concurrent admission benchmark

Share one client endpoint between 1, 4 or 8 submitting threads. Each thread
waits for its current reply before submitting the next call. One owner drives
the endpoint, transfers complete documents in memory, and runs the APP echo
handler. Samples start before admission and end after checking the reply,
including owner wake and queue time. The workload verifies every result and
reports throughput and P50, P95, P99 and P99.9 latency.

```sh
python tools/measure_performance.py --config extlibs/xrpc/config/performance_profiles.json --profiles xrpc_duplex_concurrent
```
