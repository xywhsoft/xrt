# Explicit named timezone snapshots

`xcron_tzdb` builds immutable named TZif catalogs and publishes complete revisions.
`xcron_tzdb_file` additionally imports an explicit name manifest under a borrowed
XRT anchored directory root. Both leaves remain developing. The database creates
no worker/timer and changes no process TZ, locale, environment or system zone.

Names are exact case-sensitive keys, independent of the host filesystem's case
behavior. The accepted portable path alphabet is ASCII letters/digits, `_`, `.`,
`-`, `+`, with `/` between nonempty components. Dot/dotdot components, leading or
trailing slash, a component starting `-` or ending `.` are rejected. Each full
name has at most255 bytes. Legacy identifiers such as `Etc/GMT+5` and `EST5EDT`
are supported alongside location names. This is a defined input grammar, not a
claim that every syntactically accepted name is present in an IANA release.
See [IANA identifier guidelines](https://data.iana.org/time-zones/tzdb/theory.html#naming)
and [the TZif interchange standard](https://www.rfc-editor.org/rfc/rfc9636.html).
The existing independent TZif decoder owns the actual rules/history; a name never
infers an offset or silently substitutes UTC. Leap-aware data still returns SCALE.

## Build and publication

Create takes positive NamesLimit (<=65535), LoadLimit (<=1024) and BytesLimit.
Initially there is no published snapshot; Acquire returns EMPTY. Load copies a
visible-ASCII version label (1..64 bytes), validates every descriptor/name,
allocates the catalog, copies and sorts names with an in-place bounded heapsort,
rejects duplicate exact names and decodes all entries. Input descriptors/bytes
remain stable only until the synchronous load returns; nothing is borrowed after
success. An empty manifest is an explicit, usable empty revision.

Candidate construction and destruction occur outside the publication mutex.
ExpectedRevision0 selects the initial state, an actual prior revision requests
compare-and-publish, and ANY_REVISION explicitly selects last-publication-wins.
The expected value is checked before building and again at publication. A late
old candidate cannot overwrite a newer publication when CAS is requested.
Revisions stop at UINT64_MAX-1; the ANY sentinel and zero are never published.
Any failed/closed/stale load leaves both current snapshot and Revision output
unchanged. Index identifies an original input position, even after sorting.
Zone preserves typed parser errors independently of diagnostic allocation.

LoadLimit counts admitted builders. Excess work fails LIMIT rather than joining
an internal queue. Load is cold, potentially expensive work and should run on an
application control/executor thread outside a latency-sensitive scheduler owner.
No implicit background reload or filesystem watch is created.

## Readers and scheduled jobs

Acquire holds the publication mutex only while retaining a coherent snapshot.
Info's Version borrows that retained snapshot. Explicit SnapshotRetain/Release
support cross-thread ownership; Find itself is immutable and takes no database
lock. FOUND returns one independently retained `xcronzone`; release it with
ZoneRelease. NONE preserves the zone output and does no allocation or fallback.
Every query requires a live snapshot owner throughout the call.

Snapshots and independently retained zones can outlive Close and even physical
database destruction. Existing jobs already retain their immutable zone; reload
therefore changes new lookups without rebasing consumed occurrences, pending
searches or physical leases. Applications explicitly replace jobs if desired.
For the live service, release Find's zone reference only after AddAsync has
accepted its copied configuration/retained zone, following its success contract.

Output spans/aliases are checked against inputs and the manager/snapshot arena.
Find also rejects outputs overlapping its selected zone. Callers keep outputs
disjoint from all other retained zones/snapshots and borrowed application data;
lookup does not scan every zone merely to validate outputs. Structural argument
failures preserve every output; other failures publish typed Issue and preserve
the remaining outputs. Success/NONE preserve the caller's ambient error.

## Source files and reload boundaries

LoadRoot borrows a live anchored `xroot` and an explicit list of names. It uses
RootFileOpen to resolve contained relative aliases, rejects nonregular/oversize
files and keeps at most one <=16MiB input file buffer at a time. It checks a full
read, EOF, then size/available identity and modification/change times again.
Missing, invalid, escaping or observably changed members reject the whole reload;
every partial file/buffer/zone/catalog is released before returning.

The caller supplies a trusted immutable version directory/manifest and chooses
the release label. Publication atomicity is not a filesystem-wide read transaction;
metadata checks cannot authenticate a release or detect every same-size concurrent
rewrite. A pinned manifest/hash and immutable release directory belong to source
selection/deployment. The module does not guess a default directory, enumerate a
host database, parse distribution metadata, download data, or treat POSIX text as
a database name. Windows users can distribute the same compiled TZif data as Linux.

## Resource and performance costs

One Core allocation quota charges manager, candidate/published/retired catalog
arenas, decoded history/footer rules and loader temporary/IO-helper allocations
until they are physically freed. Independent zone owners keep their allocations
charged after the catalog goes away. Old plus candidate generations must fit
simultaneously; repeated reload cannot evade the aggregate limit. Each alias name
is decoded into its own zone in this stage; there is no hidden deduplication claim.
Lower-level diagnostics created during a load also consume quota until released.
The final outward diagnostic, one fixed Core quota-control allocation, native
mutex state, allocator headers/backing and caller-owned root/handles are outside
the requested-byte cap. XRT's explicit innermost quota applies during construction,
replacing a caller's ambient quota; it is not a nested intersection of limits.

Cold costs include descriptor/name validation, name/version copies, heapsort row
moves, TZif validation/decode/footer-cache initialization and old-catalog release.
File loading additionally resolves/opens/reads/stats/closes each member. Steady
Find validates its handle/spans/name, binary-searches bounded bytes and retains
one zone on FOUND. It requests no heap, IO, timer, callback or publication lock.
Acquire adds one mutex and one snapshot reference; batching on a retained snapshot
avoids that synchronization for each lookup. No zero-overhead or elapsed/P99 claim.

The16/1024-name benchmarks time128 actual lookups/zone releases per batch,
either using one retained snapshot or acquiring one for every lookup. They record
throughput and whole-batch P50/P95/P99/P99.9; no percentile is divided into a
fictitious per-lookup latency. Cold import/IO, concurrent reload and worker/body
costs are outside these workloads and remain separate qualification work.

Inspect's revision/name/loading/closed values share one publication-state sample;
quota gauges are independently synchronized and include concurrent build/free.
Close marks the database closed, rejects new work and releases the current owner
outside the mutex (possibly O(names)). A builder notices close between entries and
before publication; a current syscall/decode is not forcibly interrupted. Destroy
requires explicit Close and zero admitted builders, otherwise BUSY. The caller
must physically exclude/join all database calls before successful destruction;
Loading0 alone is not proof that another caller has returned. Failed native mutex
cleanup keeps the closed manager available for retry. External owners release
their remaining snapshots/zones and quota ownership afterward.

Run the [memory snapshot example](../examples/tzdb/README.md) and
[anchored-file example](../examples/tzdb_file/README.md). Separate module/single,
package, adversarial/quota/lifetime/concurrent and IO-fault tests accompany the
APIs. Native Linux, production mixed-load/latency, nominal DST delivery, durable
recovery and the full long-run gates remain open.
