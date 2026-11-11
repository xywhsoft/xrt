# Pub/Sub wire semantics

The developing `xredis_pubsub` module supplies typed caller-buffer control
encoding and immutable event views. It distinguishes regular, pattern and shard
messages, all six subscription ACKs, and PONG. It is the wire foundation for the
subscribed connection work; the ordinary Client still rejects subscribe/mode
commands. This module does not open a subscribed connection, correlate ACKs,
maintain a subscription registry, implement backpressure or reconnect/replay.
Those remain required work in the full development plan.
The optional [native subscriber module](subscriber.md) now supplies connection
adoption, pipelined controls, a bounded confirmed/reserved registry and delivery
limits while reusing Client transport ownership.

## Commands

`xredisPubSubCommandSize` / `xredisPubSubCommandWrite` handle SUBSCRIBE,
PSUBSCRIBE, SSUBSCRIBE, UNSUBSCRIBE, PUNSUBSCRIBE, SUNSUBSCRIBE and PING. Subscribe
requires 1..1024 names; unsubscribe accepts 0..1024, where zero means all names
in that family. Names are binary and may be empty or duplicated. They remain in
caller order and are never normalized or deduplicated. PING accepts zero or one
binary payload, including an explicit empty payload.

The caller supplies actual readable inputs and writable output. Only the
measured wire span is written; no terminator is appended. All validation precedes
the first write, and both wire and optional Written remain unchanged on failure.
Outputs cannot overlap name descriptors, name payloads or each other. No heap
or argument-descriptor array copy is used. Each payload is copied once.

## Events and ownership

Feed an owned `xredisreply` from the existing incremental reader to
`xredisPubSubEventRead`. It classifies RESP2 arrays and RESP3 pushes, with complete
cardinality/type/null validation before publishing output. MESSAGE/SMESSAGE
expose Channel and Payload; PMESSAGE additionally exposes Pattern. Pattern ACKs
use Pattern, other ACKs use Channel. The Envelope records the actual wire type,
not negotiated protocol. Root attributes stay on the original Reply.

Empty binary names are valid subscriptions. A null unsubscribe name has
NAME_ABSENT and differs from an empty name; it confirms an unsubscribe-all
operation on an already-empty family. Its reported count can remain positive
when another regular family is still active. Regular channel/pattern counts and
shard counts are separate server domains. Never sum/decrement these fields as
if every ACK added or removed one unique subscription: duplicate arguments and
unknown-name unsubscribe ACKs are valid.

The legacy RESP2 `__redis__:invalidate` message may have an array/null payload
instead of bytes. STRUCTURED flags this exception after validating all keys.
Payload is empty and PayloadNode identifies the preserved subtree; it can also
be read by the optional Tracking view API. A normal byte message to that channel
stays a byte message. Direct RESP3 invalidations remain OTHER for separate routing.

For normal events the decoder examines fixed metadata and tag bytes; it does
not scan/copy channel, pattern or payload. Structured invalidations additionally
validate each key node. Output contains borrowed views and retains nothing.
Retain Reply before dropping its original owner; the reader can already be gone.
Output cannot alias any reply allocation, including unused reserved storage.
OTHER and MALFORMED preserve all output bytes and ambient error. Invalid API use
returns ERROR with an explicit argument diagnostic.

## PING and routing

RESP2 subscribed PING uses an aggregate pong, while RESP3 uses ordinary scalar
replies. RESP2 also returns scalars after leaving subscribed mode. The event
decoder does not guess that an arbitrary bulk string is a PING result.
Use `xredisPubSubPongRead` only while expecting a PING response, passing NULL for
no argument or a view for an explicit payload. It checks exact binary content.
Other events, errors and unknown aggregates remain OTHER for separate routing;
wrong recognized pong/scalar values are MALFORMED. Matching is not a replacement
for command/response correlation in the future connection runtime. Exact PONG
payload matching scans the expected bytes once; ordinary event views do not.

[Runnable wire example](../examples/pubsub/README.md). Separate unit, real-server
corpus, allocation/ownership and component benchmark assets are registered. The
corpus capture verifies an owned disposable Redis PID before mutation and cleans
all processes/ephemeral TLS material. Its Python transports produce reference
wire bytes; it is not proof of a native subscribed Client runtime.

Sources: [Redis Pub/Sub](https://redis.io/docs/latest/develop/pubsub/),
[SUBSCRIBE](https://redis.io/docs/latest/commands/subscribe/),
[SUNSUBSCRIBE](https://redis.io/docs/latest/commands/sunsubscribe/),
[PING](https://redis.io/docs/latest/commands/ping/).
