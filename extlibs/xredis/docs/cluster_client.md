# Asynchronous Cluster client

The developing xredis_cluster_client leaf combines shared maintained Nodes,
private metadata refresh and bounded Cluster routes on one existing XRT worker.
Create retains Nodes, copies explicit seeds, pins the progress engine, and
starts automatic SHARDS/default or explicit SLOTS bootstrap. Call returns a
typed Future; callers never drive Step. Ready observes the first valid map or a typed stop. Timer capacity/allocation
exhaustion produces RESOURCE; explicit Close preserves the first stop reason.
All commands use an explicit slot; the caller verifies all command keys agree.

The [Redis Cluster specification](https://redis.io/docs/latest/operate/oss_and_stack/reference/cluster-spec/)
allows full-map refresh on MOVED. A matching-slot MOVED from the current map
generation invalidates once per generation and triggers coalesced refresh.
Late MOVED from superseded generations cannot dirty newer maps. ASK affects
only that route and uses adjacent native ASKING/query. Missing slots request
one fresh map before a terminal no-route outcome. Metadata failures preserve
the previous map and retry at bounded exponential intervals. Seed evolution,
replica-read policy, key-command metadata and a synchronous facade remain work.

Each operation inherits one Call-entry deadline through allocation, queue,
bootstrap, provider waits and native redirects. Cancellation stops only that
operation. COMPLETE means a terminal route and can include a server or uncertain
transport failure: inspect Route.Query.Status/Delivery. Accepted business never
replays except validated MOVED/ASK. Parent stop and observed native result can
coexist. Published fields never change when a stopped native operation later
finishes. Result Native and Route views borrow the result Future.

Admission bounds reserved/in-flight requests, encoded commands and controller
reservations, including cold input copy and overlapping route replacement.
Futures/cancel observers/native parser/FIFO and metadata snapshot storage have
independent bounded duties. Caller-retained completed outcomes leave client
quotas and remain caller-owned. Warm calls copy arguments directly into the
route; bootstrap adds one cold queue copy. Parent Future/box/cancel observer,
route storage, native Future/wire, gates, references, heap updates and timer
work are real costs. One preallocated min-heap and one timer serve all operations;
ready queues and embedded posts avoid full active-request scans on completion.

Close seals admission, stops client routes/metadata and retires client watches,
posts/timer and progress-engine pin before Closed. Shared Nodes remains open;
its owner separately closes Nodes and joins engines for native physical FIFO
retirement and factory release. Retained results/maps survive client teardown;
pending native work can retain independent resources. Code remains loaded until
all results are destroyed. Production/native-Linux/TLS/auth/trust/upstream/
fixed-arrival/mixed-load/CPU/RSS/72h performance acceptance remains open.
