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
business commands is never implicit. Pool-wide generation replacement, replica
selection, optional discovered-Sentinel refresh/events and automatic persistent
HA sessions remain future work; this leaf does not claim full HA acceptance.

Service bytes are copied once, host bytes once per positive query, one allocation
holds helper plus bounded service/host capacity. Existing reply reader bounds
all wire/parsing/owned storage. Config.Role adds explicit ROLE entry/payload
caps (defaults4096 entries/1MiB). Endpoint hosts reject control/space/DEL; ports
are 1..65535 decimal strings with at most five digits. Result views borrow the
helper through Destroy; outputs must be disjoint writable caller storage.
[Runnable example](../examples/sentinel/README.md).
