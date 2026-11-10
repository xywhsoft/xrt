# Subscription intent and automatic recovery

`xmqtt_reconnect_subscriptions` adds an optional volatile subscription registry to
the reconnect controller. It uses the existing Engine worker, Client control
operations and cleanup pool. It creates no thread or shared runtime. The ordinary
`xmqttReconnectCreate` interface and behavior remain available unchanged.

Initialize `xmqttsubscriptionsconfig`, then call `xmqttReconnectCreateSubscribed`
with zero or more initial SUBSCRIBE descriptors. Initial descriptors are copied
before connection work starts. ONLINE is withheld until current subscription
intent has settled. MQTT 5 options, Subscription Identifier and all User Properties
are preserved, including when a group must be split into smaller wire requests.

`xmqttReconnectSubscribeIntent` and `xmqttReconnectUnsubscribeIntent` atomically
publish desired state for an entire input descriptor. They work during an outage.
An accepted revision is a local admission, not a broker acknowledgement. A later
revision can supersede an earlier one before transmission. There is no Future per
intent, and no guarantee that every historical revision is sent. Duplicate exact
filters within one input packet are rejected without changing the registry.

Use `xmqttReconnectSubscriptionsInfo` to inspect pending/rejected counts and the
latest settled revision. A settled revision covers the current replacement of any
superseded intent. `Synchronized` must be interpreted with `Status` and `Rejected`.
Use `xmqttReconnectSubscriptionInfo` for the exact broker grant/rejection reason
and original options of a filter. An old ACK cannot confirm a newer revision.
Successful removals reclaim their entries. A normal missing-filter lookup preserves
its output and ambient diagnostic. `KnownPresent` is the last confirmed state;
`Confirmed` additionally requires the current connected session and revision.

The optional control-result callback receives the original group revision, exact
ordered request subset and complete typed result outside locks. The request Wire
view is empty, so there is no extra encoded-wire copy for observation. The ACK message
may be separately retained, including its Reason String and User Properties, beyond
the controller and Engine. An older result remains observable when newer intent
has already superseded it.

The default rejects a failed restoration by closing and physically retiring the
controller with the typed failure. `FailOnRejected=false` instead exposes per-filter
negative/restricted results while allowing ONLINE. Successful filters in a partial
SUBACK stay individually recorded. A local inability to preserve an ACK's metadata
is a failure; scalar acknowledgement alone is not fabricated into per-filter grants.

## Session reconciliation

On Session Present 1, known confirmed subscriptions are carried forward. Pending
changes and uncertain controls are reconciled; confirmed filters are not needlessly
resubscribed. On Session Present 0, all desired subscriptions are rebuilt and pending
removal tombstones become unnecessary. A lost ACK can require repeating a control
on a resumed session. That repetition can cause retained-message delivery according
to the original Retain Handling option. Incoming messages can precede SUBACK.

Subscriptions and their identifiers belong to the broker's session state. The
registry is application intent kept separately from the QoS Session owner. The
original CONNECT/outage deadline includes restoration. Online synchronization has
its own positive `OperationTimeout`, including waiting for Client admission. Close
stops new admissions and joins Client, posts/timers, leases, cleanup work and actual
Future-watch release tails before Destroy can reclaim the registry.

Raw Subscribe/Unsubscribe calls on a leased Client bypass this registry. Do not mix
them for managed filters: the registry cannot infer those changes. The controller
does not queue offline publications, persist subscriptions to disk or provide broker
durability/subscriber-delivery acknowledgements.

A newly created volatile registry cannot discover filters installed by an earlier
process. Use Clean Start when that previous broker state must be discarded, or
explicitly supply the desired changes for known filters.

## Bounds and costs

Defaults are 1024 filter slots, 4MiB registry storage, at most 32 filters per control
request and 30 seconds per online synchronization wave. `MaxBytes` covers fixed
entries/hash buckets/batch scratch, immutable encoded snapshots, decoded metadata,
sorting scratch and at most two actual watch nodes. Concurrent snapshot builders
reserve their entire allocation before allocating it; overload does not publish a
revision. These are requested registry payload bytes; allocator headers/pools and native or
kernel resources are outside these counters. The controller's CONNECT/Session/
Inbox/Client budgets remain separate.

Intent admission encodes and decodes an owned snapshot, hashes filters and uses
a fixed-stack in-place heapsort for duplicate-detection metadata. It does not rely
on a libc sort that may allocate outside the quota. Old groups remain alive while referenced by entries
or a control batch. Hash lookup and an intrusive dirty queue avoid scanning every
filter for each request. Session transitions scan the bounded entry table; aggregate info queries read
maintained counters. Negotiated packet checks and the actual Client outgoing-byte cap split a group
before Client admission. The cap is captured at lifecycle attachment, including
per-attempt factory narrowing; a single filter that cannot fit is restricted.
Candidate checks rescan the growing prefix within MaxBatch, so the default keeps
that work bound at 32. Metadata/request quotas still use finite admission retry. Each accepted control request uses an ordinary Client Future and one
bounded heap watch node; its completion wakes the controller without polling the
Future. Each registration is separately allocated and released; no live watch
is reinitialized. Its actual controller reference includes the release tail.

Client admission overload retries after `RetryDelay` (default 5ms), under the same
wave deadline; it does not poll a pending ACK. Public overloads and internal retry
admissions are counted separately. A partly superseded original snapshot remains
charged until its last entry/batch reference is removed.

The selected controller gains one optional registry pointer. There is no additional
publication callback wrapper, payload scan/copy, per-publication Future or lease
operation. An idle synchronized controller has no registry heartbeat. These are
implementation costs, not measured global throughput or tail-latency acceptance.

## References

- [MQTT 5.0 sections 3.2.2.1.1, 3.8, 3.9 and 4.1](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html)
- [MQTT 3.1.1 specification and errata](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/errata01/os/mqtt-v3.1.1-errata01-os-complete.html)
