# Automatic reconnect controller

`xmqtt_reconnect` is developing. It drives bounded automatic attempts on a borrowed
Engine worker and uses a borrowed task pool only for physical old-Client cleanup.
It creates no thread. An online connection has no reconnect heartbeat timer and
keeps the application's original Client.Notify dispatch. Optional lifecycle hooks
run only at readiness and logical closure. With this feature selected, Client
gains one pointer; unselected Client builds do not acquire it.

Create a controller with a Session-factory adapter. Its Factory runs on the
supplied worker, with private controller-owned Session/Inbox, copied Client config
and a borrowed decoded CONNECT. Use exactly these Engine/affinity/passive owners.
FactoryData and code remain alive through Destroy. The adapter may select numeric
TCP, Dial, TLS or Auth; their independent policies and borrowed resources retain
their existing contracts. Factory returns an explicit Client status; older
ambient errors never select its retry policy. The controller owns one generation at a time. Its
encoded CONNECT, including credentials, is bounded by MaxControlBytes and wiped
at Destroy. Session/Inbox use their separately configured bounded resources.

Acquire an initialized zero Client lease while ONLINE, then call ordinary Client
APIs through Lease.Client. A batch may share a lease. Release it exactly once;
storage must not move/copy while active. Normal offline or lease-cap refusal
preserves output/error. Leases allocate no object and create no wrapper Future or
payload copy. They prevent destruction, not transport interruption. Results stay
scoped to their original connection. A last ONLINE lease release posts no work;
closure/stop releases wake the controller. Every old Client must physically retire
and all leases must return before its unique owner is destroyed and the next
factory runs. Incoming Inbox messages survive generations and stop.

Defaults retry transport, timeout and server-disconnect failures only. Exponential
backoff saturates at MaxDelay and bounded relative jitter uses a local noncrypto
xorshift state. MaxDelay is at most INT32_MAX ms; RetirementPoll is 1..60000ms.
MaxAttempts bounds consecutive attempts until a connection lasts StableReset.
MaxAttempts=0 permits unlimited attempts with bounded live state. One original
RecoveryTimeout covers each outage, including queueing, backoff and setup; -1
disables it. A successful Ready ends that outage budget. Invalid/auth/protocol/
local-resource failures are terminal by default. Explicitly enabling their retry
status bits is an application policy decision. Stop cancels retry timers and
closes the current Client; an already leased Client remains accessible to its
holder until Release.

By default CONNECT uses its requested clean flag for attempt1 and false thereafter
(also for 311 Clean Session). Disable CleanStartFirstOnly to preserve that flag
on every attempt, including always-clean/stateless retries. A
successfully bound server-assigned MQTT5 identity is read before the next attempt.
The existing Session policy decides reset/resume, expiry, QoS1/2 identifiers/DUP/
PUBREL progress and fail-closed mismatches. Old application Futures are never
rewritten after protocol recovery. This follows [OASIS MQTT5 sections 3.1.2.4,
3.2.2.1 and 4.4](https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html).

Wait/Destroy reject Engine and cleanup-Pool workers. They include actual leases,
Clients, timers, posts and task Data finalizers. If task/timer admission fails,
the controller retains its old Client for off-worker Wait/Destroy assistance;
no orphan or forced free is introduced. A wait timeout preserves the owner and
uses one original deadline. Keep the Pool/Engine and callback contexts alive
through Destroy, even after closing the Pool. Stop concurrent API entries and
borrowers before destroying the unique controller owner.

The [offline example](../examples/reconnect/main.c) forces a peer outage while
holding the old Client lease, then observes an automatic second generation and
retained old outcomes after physical joins. It requires no broker. Offline publication queues,
automatic subscription recovery, durable/crash recovery and complete transport/
interoperability/performance/production qualification remain work.

## Validation and performance

See the [stage receipt](../tests/results/reconnect-controller-20261110.json) for
source identities, root/sanitizer/package checks,160 independent numeric-TCP broker
cases,68 packaged typed-factory adversarial cases and local performance observations.
The [typed-policy probe](../tests/interop/reconnect_policy_probe.c) intentionally
returns each typed status with the same ambient MEMORY diagnostic. No old-error
heuristic may select a retry. The [broker runner](../tests/interop/run_reconnect_broker.py)
uses the independently parsed pinned broker/proxy harness and reaps every process.

`bench/bench_reconnect.c` accepts COUNT VERSION(4|5) WINDOW INSTRUMENT MODE. MODE0
is a manual private Session baseline,1 holds one batch lease,2 acquires/releases
per publication. The stage uses400000/32/0, seven samples plus one warmup per case,
actual static packages and serialized OS runs. All throughput comparisons qualify;
maximum observed loss is1.8%. Six of eight P99 comparisons remain noise-unqualified.
These are closed-loop observations, not production or fixed-arrival acceptance.

Only call Client APIs while their lease remains active; finish every entry using
that lease before Release. Bind the factory and Session to one logical broker and
security principal across attempts. FactoryData remains application-owned; its
borrowed transport/auth resources must obey the selected factory's lifetime rules.

## Automatic TLS/Dial/Auth composition evidence

The [transport stage](../tests/results/reconnect-transports-20261110.json) passes
3456 independent broker/proxy cases over eight native Windows/WSL build forms,
including full ASan/UBSan/leak and TSan. It covers TLS1.2/1.3, mutual TLS, accepted
TLS1.3 tickets/full fallback, MQTT5/311, numeric/custom async DNS/prepared factories,
five ACK cuts and576 strictly locally partial PUBLISH cases. One Engine hosts both
automatic generations; the same bounded Resolver is borrowed through both and
joins after the controller. Runtime trust/identity material is removed each run.
The broker backend is TCP behind a bounded Python/OpenSSL TLS proxy.

Both OSes pass61 root programs and final2-program transport reruns; WSL adds full
memory instrumentation and a final TSan run. Sixteen literal-peer scenarios cover
proof quarantine, two authenticated generations, reauth, held tickets, default
proof-failure retry policy, TLS rejection and DNS/TLS deadlines. These fixed-token
authentication fixtures establish protocol/lifetime behavior, not a real algorithm
or enhanced-auth broker-plugin interoperability. Production source/ABI/closures
are unchanged; broader performance, real system DNS, broker-native TLS/other brokers,
native Linux and soak acceptance remain open.
