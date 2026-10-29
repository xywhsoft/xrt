# Atomic Redis transaction operations

`XREDIS_MODULE_XREDIS_TRANSACTION` adds a bounded MULTI/queue/EXEC operation
to the existing managed client. The whole transaction family remains
`developing`: interactive WATCH/read/decision sessions, lease integration and
production/performance acceptance remain required by the overall plan.

Redis queues transaction commands and returns an ordered array from EXEC;
runtime error elements do not roll back other elements. See the
[official transaction specification](https://redis.io/docs/latest/develop/using-commands/transactions/).
The implementation is independent of Redis/client implementation sources.

## Admission and ordering

Initialize `xredistransactionconfig`, or pass NULL for a maximum of 256 commands.
The configurable hard maximum is 65,536. An empty command list is a valid empty
transaction. Each command is an array of binary byte views, just like an ordinary
call. Descriptors and payloads must remain stable and readable until admission
returns. The client copies/encodes them into one owned allocation; no caller
view is retained. Output changes only on `XREDIS_ADMISSION_ACCEPTED`.

The client must come from a managed ConnectInit/Dial/TLS factory. Raw Connect
does not provide the required initialization/control-state contract. Managed
ordinary calls reserve MULTI/EXEC/DISCARD/WATCH/UNWATCH/RESET, as well as the
existing initialization controls. A raw client retains its manual one-reply
control contract. Transaction payloads reject nested transaction controls,
connection setup/reset/quit, subscription/replication/reply-mode switches and
explicit Cluster/tracking connection controls. Unknown module commands remain
the caller's responsibility and must obey Redis transactional queue semantics.

All earlier ordinary requests drain before MULTI. A transaction occupies one
request reservation and an exclusive sent-FIFO position until its physical
protocol duties end. Later ordinary calls and transactions retain their own
deadlines while waiting. RESP3 pushes keep their separate bounded queue.

| Phase | Wire and required response |
| --- | --- |
| Start | MULTI only; require `+OK` before sending any business command |
| Queue | Encode/send the command batch contiguously; consume exactly one QUEUED/error per command |
| Commit | Send EXEC only after all commands and queue replies, with no queue error/cancellation/deadline |
| Cleanup | Send DISCARD on a queue error or pre-EXEC abandonment; require `+OK` before reusing the connection |

Checking MULTI first prevents an ACL rejection from turning a pipelined batch
into independent business commands outside a transaction. A queue error retains
only its first owned error frame, counts all errors and suppresses EXEC. Invalid
control replies, response counts or stream grammar retire the connection.
An unknown EXEC error also retires it, since that error alone does not certify
clean connection state. A recognized EXECABORT confirms abort and permits reuse.
There is no replay or automatic retry.

## Outcomes and cancellation

The Future resolves to `xredistransactionresult`, including on timeout/cancel.
Its first member is the ordinary request outcome. Successful EXEC gives its
owned array; inspect each child for command-specific errors. A rejected MULTI
or EXEC supplies that error reply. A queue rejection supplies `QueueError` and
counts, with no EXEC reply. Retain either reply explicitly to outlive the Future.
Completed results do not retain the client, engine or transaction storage.

`Execution` records separate knowledge: NOT_EXECUTED before any EXEC byte is
accepted; MAY_HAVE_EXECUTED after accepting EXEC; EXECUTED for a validated array
of the expected count; ABORTED for a null EXEC or explicit EXECABORT. A valid
array can still contain errors. A malformed/lost response after sending EXEC
cannot establish success or absence of effects. An ordinary delivery of REPLIED
records a validated terminal response and does not itself mean business success.

The monotonic timeout begins at public API entry and includes sizing, copying,
worker/setup queueing and all phases. Cancellation/timeout before EXEC suppresses
it. A partially written command batch is completed to its RESP boundary before
discarding; it never escapes MULTI. After EXEC is accepted, cancellation does
not retract execution. Logical abandonment publishes immediately and leaves a
counted physical obligation. The existing positive DrainTimeout bounds cleanup;
if the peer never responds, retire the connection and report surviving calls
as disconnected rather than pretending that their own deadlines expired.
Already published outcomes and counts never change during late draining.

The synchronous facade rejects every network worker belonging to the engine.
Future notifications follow the existing completion paths; callbacks must not
block workers. Close remains observable through the existing physical barrier,
which includes wire references, request/cancel/timer ownership and byte quotas.
Keep code resident through engine join and all retained result destructors.

## Resources, delivery and measurement

One request+wire+aligned-state allocation owns a transaction, with a separate
typed outcome and the existing Future/cancel/timer resources. There is no
per-command Future or offset array. The existing request-byte caps charge all
preencoded MULTI/EXEC/DISCARD and business bytes plus alignment/state storage.
Ordinary command charging is unchanged. Reply limits still bound each parsed
frame; only the first queue error and final EXEC response can be retained.

Selecting this feature uses one existing tail-padding byte for its private transaction tag
and adds conditional checks in the existing send/read/completion paths. The client
layout, public client configuration and result ABI stay unchanged. Copies,
validation walks, control round trips, reply allocations, references, locks and
timer work remain real costs. No zero-overhead or final performance claim is made.

```text
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_transaction_tests
python tools/build.py --manifest extlibs/xredis/config/modules.json --suite xredis_transaction
python tools/measure_performance.py --config extlibs/xredis/config/performance_profiles.json --profiles xredis_transaction --smoke
```

The self-contained example and benchmark use a bounded local C protocol fixture.
The sequential benchmark measures two-command transactions from API entry
through result release, with initialization excluded and peer work included.
Its cycle quantiles are closed-loop diagnostic evidence. Independent real Redis,
fixed-arrival/matched baselines, native Linux, resource/long-run/fault acceptance
and the remaining product functions are separate requirements.

The measured ordinary request remains 104 bytes with or without selection on
Windows/WSL x64; its private wire offset is 102 / 103 respectively. The initial
8-byte pointer growth was measured and removed. Transaction state is 72 bytes
plus at most seven padding bytes; the
typed result is 64 bytes instead of the ordinary 24-byte result. Private client
size stays 848 bytes on Windows / 920 on WSL in the same TLS/auth-selected
closures. Public client config remains 232 and ordinary result 24 on both.
These are compiler layout/logical byte counts, not allocator size-class/RSS or
complete copy/allocation/synchronization acceptance. The optional-feature
ordinary-call workload uses the same raw client pipeline as `xredis_client`;
it isolates selected metadata/frame checks and does not measure the added
managed control-word checks. Unpinned comparison noise is not a regression gate.

A reproducible independent transaction run is:

```text
python extlibs/xredis/tests/interop/run_redis_tls_auth.py --transaction --key rsa --build --report out/transactions-redis.json
```

This keeps build receipts/outputs separate from the auth-only driver and covers
both TLS versions, no-password/password/ACL configurations, RESP2/RESP3, binary
values, queue rejection/discard, runtime error elements without rollback and
control ACL denial. Native Windows clients use the documented build-only/WSL
peer workflow. WATCH sessions, additional independent fault/recovery runs and
native long-run/performance acceptance still remain required.
