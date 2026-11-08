# Bounded result files and recovery

`xworkToolResultWriterSaveArtifact` publishes a UTF-8 text file through the
agent's host-selected artifact store. Its metadata is borrowed from the live
writer. The publication uses anchored paths, a flushed temporary file and a
atomic exclusive hard-link publication. Persistence-disabled agents receive a policy error; callers
must return a bounded preview instead. The writer attaches the file to the
existing tool completion event without putting the full file in model history.

`read` accepts a read-only `@results/run-<20 digits>/<artifact filename>` path
within the current agent's store. It cannot mount another session's store or
write results. Text pages default to 4096 bytes including display metadata;
`max_bytes` accepts 256 through 8192. `start_line`/`max_lines` select ranges;
`offset` is an exclusive alternative to `start_line`, in original file bytes.
The next offset always ends at a complete UTF-8 scalar, including long lines.
Image passthrough retains its separate image size limit.

`xworkRuntimeConfigureToolRecovery` explicitly declares a read operation
recoverable while preserving all permission effects. Mutation effects reject
this declaration. Catalog snapshots retain their original declarations.
For these tools, a small immutable completion receipt is committed before the
completion event and model-ledger append. Exact session store, durable turn,
source, tool name, call ID and argument bytes identify the receipt. Reopening
the runtime verifies the referenced file size and SHA-256 and returns the
saved result without executing the callback. Damaged receipts fail with IO;
they never silently repeat a request. Child-task receipts are disabled until
their durable task identity is available.

Receipts cover completed calls. An external service may accept a request and
disconnect before completion is saved; exactly-once billing in that window
requires idempotency at the service. This API does not claim otherwise.
User cancellation remains cancellation. An event callback that fails because
of persistence must set an xrt IO error before returning false; this is reported
as IO failure instead of a user stop. A false callback with no error retains
the existing cancellation contract.

Validation: `test_xwork --utf8-tool-output`, `--result-files`, and the ordinary
bounded native suite. The fixtures check byte boundaries, exact reconstruction,
runtime/store reopening, failed completion events, non-repeated callbacks,
mutation rejection, path confinement and damaged-file behavior.
