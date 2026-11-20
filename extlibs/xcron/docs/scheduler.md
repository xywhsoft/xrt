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

Dispatch reuses a successful next-time calculation for adjacent due jobs
with equal compiled rule masks and day-star flags in the same wave.
Each job advances its own heap entry and invokes its callback with its
own context.

See [the two drive paths](../examples/scheduler/main.c),
[APP-owned worker dispatch](../examples/executor/main.c), and
[configuration reload](../examples/service_checkpoint/main.c).

The scheduler benchmark uses one every-second UTC job and an equivalent
one-second deadline loop built directly with XRT EventWaiter. The direct
loop computes its next second arithmetically. Both workloads discard their
first wake and measure process CPU and callback lateness from the scheduled
deadline. The idle poll reference reads the wall clock once per iteration.

```sh
python tools/measure_performance.py --config extlibs/xcron/config/performance_profiles.json --profiles xcron_scheduler
```

The multi-job profile uses either 64 every-second jobs and 960 dormant jobs,
or 1,024 every-second jobs. Each mode checks exact callback counts and
registration order. A missing or extra wave fails the run. The direct
EventWaiter reference already knows the due cohort and calls the same
callbacks. It has no cron calendar or dormant-job index to maintain.

Both paths discard the first wave. The report records first and last
callback lateness against the scheduled UTC second at the wall clock's
millisecond resolution. The monotonic span from the first callback to the
last measures dispatch within the wave with finer resolution. Process CPU
includes waiting and callback dispatch. P99 and P99.9 are the largest
observation in each eight-wave sample.

Set XBENCH_PIN_CPU to one available logical CPU for the controlled profile.
The benchmark verifies the resulting mask. Running without the variable
inherits the parent affinity, which also allows CI smoke execution.

```sh
XBENCH_PIN_CPU=4 python tools/measure_performance.py \
  --config extlibs/xcron/config/performance_profiles.json \
  --profiles xcron_scheduler_cohort --report out/xcron-cohort.json
```
