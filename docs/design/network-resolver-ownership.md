# Resolver physical ownership contract

The resolver and resolve-operation adapters describe actual strong owners, not
worker counts, queue indexes, or inferred service liveness. One accepted request
owns one operation reference. An operation independently owns its resolver shell,
result/error, and (for the certified API) one callback-data reference. Cache
entries own their own result/error references; hash and LRU links do not duplicate
these edges. Address lists are immutable reference-counted leaves.

`xrtNetResolverCreateOwnedV1` and `xrtNetResolverResolveOwnedV1` transfer exactly
one context reference on success, none on failure. A callback policy is explicit,
immutable and resident. Lookup context lasts until all native threads and TLS
tails have joined. Request context lasts until completion dispatch or suppression
has finished. Their Drop callbacks run outside runtime-owned mutation scopes.
Legacy callbacks remain opaque rather than being inferred safe from a function
address or an empty-looking context.

All participating queue/RC/slot transitions enter the ownership mutation domain
before the resolver lock. A condition wake releases that lock before re-entering
mutation. Blocking lookup, callback, context Drop and native thread join do not
hold mutation. Actual running/joining/retiring states refuse inspection. Parked
workers are borrowed participants; no artificial worker RC edges are invented.
An exited thread is not considered quiescent until native join proves its TLS
tail finished.

Adapters reject unknown policy identity before child Count/Trace. Claim closes
new request/cache admission but does not suppress accepted callbacks. Preparation
drains accepted work, without synthesizing cancellation, and joins nonblocking.
It retains every owning field. Clear marks the prepared object; Finish retires
real fields/resources outside freeze and is idempotent. The creator owner is
separate from plan holds and is consumed only by successful public destruction.
External operation owners and resurrection are still observable as physical RC.

This is the resolver component contract, not proof of the complete network
service graph. The native Future producer/bridge/cancel chain and engine queues,
ports and handles must be admitted independently before a module can relax its
current opaque-service refusal. Focused and preserved full native results are
recorded by the integrating compiler workspace; unexecuted tests are not passes.
