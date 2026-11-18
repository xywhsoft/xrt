# Sentinel master discovery

The developing xredis_sentinel leaf drives bounded multi-seed master discovery,
SENTINEL GET-MASTER-ADDR-BY-NAME, target ROLE validation and stale-role retry.
It implements the [Sentinel client procedure](https://redis.io/docs/latest/develop/reference/sentinel-clients/)
using independent C code and existing managed Client factories.

Separate seed/master factories select native numeric, DNS, prepared-address or
TLS setup and separate credentials/trust. They return typed failures, never
ambient-error inference. A successful factory transfers one private native
Client. One candidate exists at a time; failed/query connections physically
close before the next factory. Seed connect+query shares SeedTimeout; the
original total deadline also covers scheduling, retries and physical retirement.
An all-null seed round reports UNKNOWN_SERVICE; transport loss reports
UNREACHABLE; stale candidate roles requery after RetryDelay up to MaxRounds.
Authentication, ACL, malformed replies, bounds and memory failures fail closed.

All APIs require one serialized caller; Step is nonblocking and may not reenter
from a factory. WAIT is driven by Pending notification or NextWake, whichever
comes first. MORE needs another scheduled turn. No worker, timer or wrapper
Future is created. A native request owns copied arguments; Destroy does not
leave helper pointers in callbacks. Cancel followed by Step to DONE observes
physical retirement; Destroy alone requests asynchronous native close.

On DONE/OK, TakeClient transfers the native verified master and close duty.
Ordinary business calls use the original native API without a proxy. Preserve
Result.PreferredSeed for the next discovery and rediscover after every lost
connection. ROLE is point-in-time identity, not freshness/consensus. Retrying
business commands is never implicit. [Automatic native pools](sentinel_pool.md) provide pool-wide generation
replacement. The same discovery owner also supports standalone replica
selection. Optional Sentinel list refresh, event subscriptions and continuous
replica pool maintenance remain separate work.

Service bytes are copied once, host bytes once per positive query, one allocation
holds helper plus bounded service/host capacity. Existing reply reader bounds
all wire/parsing/owned storage. Config.Role adds explicit ROLE entry/payload
caps (defaults4096 entries/1MiB). Endpoint hosts reject control/space/DEL; ports
are 1..65535 decimal strings with at most five digits. Result views borrow the
helper through Destroy; outputs must be disjoint writable caller storage.
[Runnable example](../examples/sentinel/README.md).

## Replica discovery

Initialize the base configuration with `xredisSentinelConfigInit` and call
`xredisSentinelReplicaDiscoveryCreate` with a replica connection factory and
an explicit list limit. Drive the resulting object with the normal Step,
Pending and NextWake calls. TakeClient transfers the verified native
connection and its close duty. The procedure follows the public Sentinel
client specification for replica discovery and ROLE verification.

The seed query uses SENTINEL REPLICAS. The complete RESP2 array or RESP3 map
list is validated before a data connection begins. Each row must carry one
IP/host, port and flag field. The library skips rows without the replica
flag and rows marked subjectively down, objectively down or disconnected.
The first candidate with a native ROLE reply identifying a connected replica
wins. A promoted or syncing node proceeds through physical close before
another candidate. The factory can authorize or map Sentinel-announced names
and addresses for the caller's network boundary.

The configured seed timeout bounds each seed query and each candidate setup
plus ROLE. The original total deadline covers the complete operation,
including retry delay and pending physical duties. Exhausting a list advances
to the next seed. A round can report NO_REPLICA when known services have no
eligible candidates, UNREACHABLE for unsuccessful transport attempts or
STALE_ROLE when a candidate no longer meets the requested role. The exact
unknown-service reply and null replies participate in UNKNOWN_SERVICE.
ACL errors and invalid or over-budget lists stop discovery.

Discovery reserves a bounded endpoint descriptor array in its existing
allocation. It retains one immutable seed reply through candidate attempts
and copies a host only when selecting a candidate. The native reply budget,
the additional payload cap, list length and pair count bound the retained
reply and parsing work. Successful data requests use the native Client.
Replication lag, read-after-write visibility and replica pool policy depend
on the caller's workload.
