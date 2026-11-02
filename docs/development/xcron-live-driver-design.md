# xcron live driver: implementation contract, not delivered functionality

The current expression/calendar/zone/actual-occurrence, scheduling kernel and
borrowed-executor adapter are real implemented stages. This document preserves
the next required live-driver work; it does not register a placeholder module or
claim a functioning timer service. The four-library objective and final native,
performance, recovery and long-run gates remain unchanged.

## Owner and waiting

A dedicated public-XRT thread owns a kernel and an execution adapter borrowing a
host Executor. It calculates wall recurrences, dispatches actual leases, consumes
physical completions and sleeps using a public XRT event until the earliest
projected monotonic deadline, pending retry, cross-thread command or completion.
Use an auto-reset wake with bounded wall resampling; do not reset a wake after
checking work, which could lose a producer's signal. Wake delivery can fail, so
the resampling bound remains mandatory. No work runs in a network callback.

The driver samples xrtNow/xrtTimer or an explicit test clock and applies the
kernel's wall-jump/misfire rules. Invalid or regressing monotonic samples become
an observable failure followed by controlled shutdown, not guessed schedules.
Clock callbacks must finish their data-using tails on return. They are no longer
invoked once shutdown begins. Deep history/calendar queries may still delay this
owner; incremental/precomputed query work and adversarial P99 remain necessary.

Before any blocking facade, reject the driver owner and a current callback of
the borrowed executor. Checking only driver-owned business bodies misses an
unrelated parent job occupying the same single-worker executor. The archived
one/two-worker condition-variable prototype proves the dependency, and existing
ExecutorWaitFor reports indistinguishable STATE errors for an open external
caller and its own worker. The minimal public xrtExecutorIsCurrent query exposes
the existing worker/cancellation registry for this guard; it adds no runtime.

## Bounded commands and responses

Use a preallocated, mutex-protected command FIFO and generation-tagged ticket
slots. Outstanding replies continue occupying their ticket slots until collected.
Add copies the expression and success-only retains the input zone for its queued
command. Acceptance means the command was admitted, not that the job exists.
Its Data remains borrowed until a failed Add reply is collected, or a successful
job ultimately returns a retirement receipt. Processing transfers a separate zone
reference to the kernel on success, then releases the command's reference.

Initial commands cover Add, Cancel (with optional running-token request), Info
and Reap. Each reply has a typed status, original request kind, relevant job info
or retirement value, and an explicitly owned optional error. Owner errors never
borrow the submitting thread's ambient error. A timed collection does not cancel
an accepted command or return its Data early. Conditional waits share one absolute
monotonic budget across contention/spurious wakeups; completion wins over timeout.

Close rejects new Adds and completes queued unprocessed Adds with CLOSED replies.
An Add already executing on the owner can succeed and subsequently be cancelled;
its response must still identify the admitted job and its retirement duty.
Close's observation is asynchronous at owner turns, not a no-new-body barrier.
The owner finishes all commands admitted before its final stopping check.

## Reserve completion capacity before issuing leases

Reserve a completion slot before calling SchedulerPoll. Outstanding reservations
include unsent leases, accepted execution and completed results not yet consumed.
The free completion slots and adapter slots bound how many leases can be pulled.
An active mapping from kernel run index/ID to reservation is safe because an
active kernel run slot cannot be reused before Complete. A prior delivered result
can remain in a separate completed slot while that run index gets a new generation.

Completion records contain IDs, scheduled/covered/observed times, result, typed
code and owned error, **not a borrowed job Data pointer**. Thus a caller can process
a retirement receipt before consuming older completion metadata without receiving
a dangling Data pointer later. Completion order across workers is unspecified.

A full output queue stops dispatch instead of dropping results. Every issued
lease already has space, so completion and shutdown never require a consumer to
free additional output capacity. Native submission failure consumes no work;
the driver either reports it explicitly or keeps a bounded unsent lease and
retries only no-effect overload. Freeze retry interval, attempt/elapsed limits
and one monotonic admission deadline. Count attempts/rejections/expiry separately.
Close completes unsent leases as cancelled without running their bodies.

## Stop, ownership handoff and destruction

The owner closes the kernel and adapter, requests running cancellation only when
configured, and returns every physical lease. Adapter publication/Notify tails
must reach zero before its destruction and before releasing wake context. No
forced termination substitutes for completion. Async wake posts must own their
own host lifetime beyond the Notify call; bounded event delivery is simpler here.

Do not automatically discard/reap terminal jobs. Their slots and original Data
stay in the kernel until explicit retirement. Reap is an owner command while
running. After the owner publishes that it will no longer touch the kernel, a
serialized caller-side reap can take over. Native thread join still includes
XRT-context cleanup before destruction or code unload. Close/Join can finish
before clients drain replies/completions/reap jobs: all tail outputs were reserved.
Destroy requires the joined owner, zero jobs and no outstanding tickets/results,
plus caller-exclusive access. It consumes neither a borrowed Executor nor caller
Data. A busy result preserves the object and every remaining duty.

Preallocate command/response slots, completion reservations, mappings, buffers and
synchronization storage. Reject arithmetic overflow. A total requested-byte cap
can reserve the sum of the driver arena plus explicit kernel/adapter caps, then
report actual component bytes. Borrowed zones/business data, host Executor and
callback/native runtime allocations require separately stated accounting; never
advertise them as covered by a smaller local arena cap.

## Evidence required for the live feature

Use real wall time and deterministic paired clocks, including forward/backward
jumps, folds/gaps, overload/retry expiry, slow consumers and cancellation. Verify
callback reentry, self-wait rejection, command/close races, slot/ID reuse, Add
failure ownership, output backpressure, join-before-consumption and post-stop
retirement. Inject every observed cold allocation failure and prove warmed steady
dispatch allocation behavior. Supply runnable documented examples and consumers
for modular/single/static/shared forms; run sanitizer and native lifetime checks.
Measure fixed-arrival end-to-end lateness/queue wait as well as component costs.
Durable recovery, named zone database/reload and nominal DST delivery remain
separate required stages, not implied by a successful live timer test.
