# Selected lease execution (developing)

Select `XCRON_MODULE_XCRON_EXECUTOR_SELECTED` and include
`<xcron/executor_selected.h>`. The optional implementation shares the existing
adapter's submission, cancellation, hash table, completion publication and
Notify-tail lifetime code. It does not create another executor or runtime.

CreateSelected uses the existing config and callback signature. SubmitSelected
copies the ordinary lease and its nominal Local/global Candidates before any
worker can execute. During Proc, ExecutionSelected returns an immutable borrowed
label view. It is NULL for a plain adapter or NULL token. Neither the view nor
the token may escape Proc. The caller can reuse or overwrite submission storage
as soon as Submit returns. Job Data retains its scheduler retirement lifetime.

DrainSelected returns a complete copied selected lease, result/code and owned
worker Error. Queued native cancellation still returns the original metadata
even when Proc never ran. Call SchedulerComplete exactly once for each physical
receipt and free Error with xrtErrorFree. Slot reuse cannot invalidate an already
copied completion. Adapter destruction also waits for outstanding Notify tails;
borrowed module/callback code remains resident through the host executor join.

One allocation preallocates all slots, duplicate-ID hash and selected labels
within BytesLimit. Plain adapters allocate no label array; actual-only builds
omit the optional implementation. Worker callback dispatch remains unchanged;
only callbacks that request the view perform its pointer lookup. Selected
receipts additionally copy the label. No zero-overhead claim is made.

Verified x64 costs: existing public config/completion/stats/run stay56/80/64/64
bytes. The actual-only private owner stays176 bytes; enabling the optional
capability adds one8-byte label-array pointer, including for plain instances in
that build. The hot slot stays104 bytes and token offset88. Selected instances
add56 bytes per slot; two-slot extended plain/selected arenas request408/520
bytes. Submit and Drain each copy56 additional nominal bytes; extended builds
also check the adapter mode. The callback getter performs its pointer lookup.
The existing worker dispatch and physical publication/Notify-tail bodies are
unchanged. Both direct and generated declarations agree on label56 and selected
completion136 bytes.

Plain Submit/Drain reject a selected adapter and selected Submit/Drain reject a
plain adapter atomically. Common Cancel/CancelJob/Close/Inspect/Destroy retain
their owner and reentry contracts. Rejection transfers no physical completion
duty and invokes no callback. Closing this adapter does not close its borrowed
executor. See the [offline example](../examples/executor_selected/README.md).

The [stage evidence](../tests/results/selected-executor-20261109.json) records
Windows/WSL modular/single, static/shared, OOM/lifetime, ASan/UBSan and TSan checks.
First component performance profiles use synthetic64-lease batches, actual
workers, two shared callback validation atomics, complete owner validation and
receipt collection. Both environments fail noise gates and four workers have
lower throughput than one. The cause is not isolated; these observations require
investigation rather than a scaling or production-performance claim.

Live-service command/response integration, compensation, durable recovery and
qualified native production performance/long-run acceptance remain open.
