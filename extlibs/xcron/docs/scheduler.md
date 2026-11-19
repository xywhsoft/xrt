# APP-driven UTC scheduler

Select `XCRON_MODULE_XCRON_SCHEDULER` for modular or single-header use. Create
one scheduler with a fixed capacity and an explicit five- or six-field dialect.
Registration compiles the string, computes the first occurrence strictly after
the current wall time and returns an opaque job ID. Remove a job using its
original ID. A removed ID cannot affect a replacement that reuses its slot.
The APP keeps the context alive until removal or destruction.

One APP thread owns create, add, remove, poll, wait and destroy. It receives
callbacks inline with their original context. A callback can set an APP flag
or submit work to a pool. Calls on the same scheduler that mutate or drive it
must wait until the callback returns. Another thread can call Wake while the
scheduler is live. Join such callers before destroying the scheduler.

Poll checks the earliest job immediately. Wait parks through XRT's reusable
event waiter until a job may be due, the timeout expires, or an explicit wake
arrives. On timer wake the scheduler checks wall time again. An explicit wake
returns WOKEN so the caller can apply APP changes or drive again. With a zero
timeout, Wait dispatches due jobs and otherwise returns TIMEOUT. An empty
scheduler can wait for Wake indefinitely.

Each drive samples UTC wall time once per dispatch wave. Each due job invokes
its callback once. Its next deadline follows the original expression strictly
after the sampled time, even if several occurrences elapsed during an APP
pause. A backward clock change keeps already advanced deadlines. If the wall
clock steps forward during a monotonic wait, signal Wake from the APP's system
time-change or resume handler to bring the check forward.

The heap preserves registration order for equal deadlines. An idle Poll reads
one wall timestamp and inspects the heap minimum. Registration, removal and
advancement update the heap in logarithmic time. Storage is allocated during
creation and the waiter is initialized on first use of blocking Wait. The
dispatched APP callback controls execution cost.

See [the two drive paths](../examples/scheduler/main.c),
[APP-owned worker dispatch](../examples/executor/main.c), and
[configuration reload](../examples/service_checkpoint/main.c).
