#ifndef XCRON_TZDB_H
#define XCRON_TZDB_H
#include <xcron/features.h>
#include <xcron/export.h>
#include <xcron/timezone.h>
#include <xrt/error.h>
#if defined(XCRON_FEATURE_XCRON_TZDB) && (!defined(XCRON_FEATURE_XCRON_TIMEZONE) || !defined(XRT_FEATURE_MUTEX))
#error "cron tzdb requires timezone and mutex"
#endif
#if defined(XCRON_FEATURE_XCRON_TZDB)
#define XCRON_TZDB_NAMES_MAX 65535u
#define XCRON_TZDB_NAME_MAX 255u
#define XCRON_TZDB_VERSION_MAX 64u
#define XCRON_TZDB_LOADS_MAX 1024u
#define XCRON_TZDB_ANY_REVISION UINT64_MAX
#define XCRON_TZDB_NO_INDEX SIZE_MAX
typedef struct xcrontzdb xcrontzdb;
typedef struct xcrontzdbsnapshot xcrontzdbsnapshot;
typedef enum xcrontzdbstatus {XCRON_TZDB_ERROR=-1,XCRON_TZDB_NONE=0,XCRON_TZDB_FOUND=1} xcrontzdbstatus;
typedef enum xcrontzdberror {
    XCRON_TZDB_OK=0,XCRON_TZDB_ARGUMENT=7001,XCRON_TZDB_NAME,XCRON_TZDB_VERSION,
    XCRON_TZDB_DUPLICATE,XCRON_TZDB_LIMIT,XCRON_TZDB_MEMORY,XCRON_TZDB_ZONE,
    XCRON_TZDB_IO,XCRON_TZDB_STALE,XCRON_TZDB_EMPTY,XCRON_TZDB_CLOSED,XCRON_TZDB_BUSY,XCRON_TZDB_SYSTEM
} xcrontzdberror;
/* Index is the original input position (not sorted order), or NO_INDEX. Zone
 * keeps typed parser failures even if its diagnostic allocation fails. Cause/
 * SystemCode describe a failed underlying IO operation when available. */
typedef struct xcrontzdbissue {xcrontzdberror Code;size_t Index;xcronzoneissue Zone;xerrkind Cause;int32 SystemCode;} xcrontzdbissue;
typedef struct xcrontzdbconfig {uint32 NamesLimit,LoadLimit;size_t BytesLimit;} xcrontzdbconfig;
/* Name is an exact case-sensitive identifier; Tzif is borrowed only during Load. */
typedef struct xcrontzdbentry {xstrview Name;xbytesview Tzif;} xcrontzdbentry;
/* Version borrows this retained snapshot. Bytes includes its arena/decoded zones. */
typedef struct xcrontzdbsnapshotinfo {uint64 Revision;size_t Names,Bytes;xstrview Version;} xcrontzdbsnapshotinfo;
typedef struct xcrontzdbstats {
    uint64 Revision;uint32 Loading,Names;size_t BytesLimit,LiveBytes,PeakBytes,LiveAllocations;uint64 Rejections;bool Closed;
} xcrontzdbstats;
XRT_EXTERN_C_BEGIN
/* Explicit named TZif database; no embedded data, ambient TZ, filesystem, worker
 * or timer. Limits are positive and bounded by the constants above. One shared
 * XRT quota charges manager, all candidate/published/retired snapshot arenas,
 * decoded zones/footer rules and temporary allocations until actually freed,
 * including zones retained independently after snapshot release. One fixed Core
 * quota-control allocation, mutex native state, allocator backing/headers and
 * final outward diagnostics are outside BytesLimit. Lower-level diagnostics
 * built inside a load DO consume quota until released. Explicit quota replaces the caller's
 * ambient quota while constructing database-owned data (XRT innermost rule).
 * Invalid spans/aliases preserve every output; other failures publish typed
 * Issue even under diagnostic OOM and preserve other outputs. Success/NONE
 * preserve ambient error. Outputs cannot overlap manager/snapshot arena, each
 * other or inputs; caller keeps them disjoint from all borrowed/retained zones
 * and any other retained snapshots. Error domain: xrt.cron.tzdb. */
XCRON_API bool xcronTzdbCreate(const xcrontzdbconfig* Config,xcrontzdb** Database,xcrontzdbissue* Issue);
/* Build/validate/copy all input names, version and decoded TZif off the publish
 * lock. Names are1..255 ASCII letters/digits/_/./-/+ with slash separators;
 * empty/dot/dotdot components, leading/trailing slash, components starting '-'
 * or ending '.' are rejected. Legacy names such as Etc/GMT+5 remain valid.
 * Version is1..64 visible ASCII bytes. Input descriptors and bytes stay stable
 * throughout this synchronous call. Empty input publishes an empty snapshot.
 * Duplicate exact names fail atomically; names are not locale/case normalized.
 * ExpectedRevision is0 for initial publication, the prior revision for CAS, or
 * ANY_REVISION for deliberate last-publication-wins. Revision never wraps or
 * uses the ANY sentinel. Failures/stale/close leave the current snapshot and
 * Revision output intact. LoadLimit bounds admitted concurrent builders; excess
 * calls fail LIMIT rather than wait for a builder slot. Close stops subsequent
 * build entries/publication; it cannot interrupt a current syscall/decode.
 * This is cold work: run it outside a latency-sensitive scheduler owner. */
XCRON_API bool xcronTzdbLoad(xcrontzdb* Database,xstrview Version,const xcrontzdbentry* Entries,size_t Count,
    uint64 ExpectedRevision,uint64* Revision,xcrontzdbissue* Issue);
/* Acquire takes the publication mutex once and retains a coherent immutable
 * snapshot; EMPTY before first publication, CLOSED after Close. Retain/Release
 * permit explicit cross-thread ownership. Snapshot readers need no database or
 * publication lock and may outlive its physical destruction. */
XCRON_API bool xcronTzdbAcquire(xcrontzdb* Database,xcrontzdbsnapshot** Snapshot,xcrontzdbissue* Issue);
XCRON_API bool xcronTzdbSnapshotRetain(const xcrontzdbsnapshot* Snapshot);
XCRON_API void xcronTzdbSnapshotRelease(const xcrontzdbsnapshot* Snapshot);
XCRON_API bool xcronTzdbSnapshotInfo(const xcrontzdbsnapshot* Snapshot,xcrontzdbsnapshotinfo* Info,xcrontzdbissue* Issue);
/* Exact bounded binary search. FOUND returns one retained zone, independently
 * releasable with ZoneRelease; NONE preserves Zone and publishes OK Issue.
 * No IO/allocation/lock on normal lookup. New snapshots affect new lookups only;
 * existing scheduler jobs/leases keep their immutable zone, never silently
 * rebase consumed occurrences. A caller explicitly replaces jobs when desired. */
XCRON_API xcrontzdbstatus xcronTzdbFind(const xcrontzdbsnapshot* Snapshot,xstrview Name,xcronzone** Zone,xcrontzdbissue* Issue);
/* Revision/name/closed/loading are one publication-state sample; quota gauges
 * are independently synchronized and can include concurrent cold build/free.
 * LiveBytes includes the manager itself and every retained generation. */
XCRON_API bool xcronTzdbInspect(xcrontzdb* Database,xcrontzdbstats* Stats,xcrontzdbissue* Issue);
/* Idempotent, rejects new loads/acquires and releases the current reference
 * outside the lock. Old snapshots/zones remain usable. Release may be O(names).
 * Destroy requires Close and zero admitted builders, else BUSY; caller must also
 * physically exclude/join ALL concurrent database calls before Destroy. A failed
 * native mutex cleanup keeps the closed object live for retry. */
XCRON_API bool xcronTzdbClose(xcrontzdb* Database,xcrontzdbissue* Issue);
XCRON_API bool xcronTzdbDestroy(xcrontzdb* Database,xcrontzdbissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
