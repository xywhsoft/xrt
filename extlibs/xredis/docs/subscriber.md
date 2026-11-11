# Native Pub/Sub connection mode

The developing `xredis_subscriber` module dedicates an existing idle, ready
Client to Pub/Sub. Existing transport factories, readiness and physical close
remain authoritative. Ordinary calls and exclusive sessions cannot share it.
Use `xredisSubscriberCommandAsync` for pipelined controls and
`xredisClientTakePush` to transfer owned message/sideband replies.

The registry bounds confirmed and reserved distinct binary names across all
three subscription families. Request/Future storage and command metadata use
the existing per-call/aggregate request quotas. Persistent names additionally
use MaxSubscriptions and MaxNameBytes. Allocation/limit failure before sending
rolls back all reservations and returns a typed NOT_SENT result.

Every named ACK and reported count is validated. Unsubscribe-all accepts its
unordered confirmed family set or one absent-name ACK when that family is
empty. Regular channel/pattern and shard count domains remain separate.
Duplicate names and unknown-name unsubscriptions do not imply count changes.
Messages never consume command FIFO slots. The existing push count/owned-byte
limits close an overflowing connection; no silent delivery loss is accepted.

Cancellation/deadline completes the caller's immutable result without removing
a sent command's remaining ACK obligations. Late ACKs still update confirmed
state and drain in order under the existing bounded grace. No request or
subscription is implicitly replayed. Reconnect/desired-state restoration still
requires later work. Production/performance qualification remains incomplete.

Names are hashed and copied once on first persistent reservation; duplicates
reuse the same entry. Messages hash/compare their channel or pattern to a
confirmed entry without scanning/copying payloads. This validation and the
existing owned reply trees are counted in runtime benchmarks. The table has a
fixed power-of-two bucket array and bounded entry chains, not an unbounded map.
Close frees at most WorkRequests entries per worker turn after request duties
retire. The cold mode/bucket allocation remains until the last Client reference,
so a retained closed Client can still report Info. Queued messages retain their
own reply ownership independently of the registry and Engine.

[Runnable example](../examples/subscriber/README.md). Tests include a disposable
real Redis TCP/TLS1.2/TLS1.3 matrix with RESP2/3, numeric/DNS/prepared factories,
password/ACL variants and instance identity checks before subscription/ACL
mutation. Source identities and raw results determine which forms have passed;
registration alone does not establish production completeness.

Protocol references: [Redis Pub/Sub](https://redis.io/docs/latest/develop/pubsub/),
[UNSUBSCRIBE](https://redis.io/docs/latest/commands/unsubscribe/),
[SSUBSCRIBE](https://redis.io/docs/latest/commands/ssubscribe/).

The optional [receive notification leaf](subscriber_notify.md) adds shared
readability Futures and batch ownership transfer over the same bounded queue.
The base subscriber remains independently selectable without those fields.
