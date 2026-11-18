#ifndef XMQTT_JOURNAL_H
#define XMQTT_JOURNAL_H
#include <xmqtt/wire.h>
#include <xrt/file.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL) && (!defined(XMQTT_FEATURE_XMQTT_WIRE) || !defined(XRT_FEATURE_FILE) || !defined(XRT_FEATURE_CRC32))
#error "MQTT journal requires wire, file and CRC32"
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL)
typedef struct xmqttjournal xmqttjournal;
typedef enum xmqttjournalstatus {
    XMQTT_JOURNAL_UNCERTAIN=-6,XMQTT_JOURNAL_IO=-5,XMQTT_JOURNAL_CORRUPT=-4,
    XMQTT_JOURNAL_STATE=-3,XMQTT_JOURNAL_ERROR=-2,
    XMQTT_JOURNAL_EMPTY=0,XMQTT_JOURNAL_DONE=1,XMQTT_JOURNAL_LIMIT=2,XMQTT_JOURNAL_BUSY=3
} xmqttjournalstatus;
typedef enum xmqttjournaleffect {
    XMQTT_JOURNAL_NOT_COMMITTED=0,XMQTT_JOURNAL_MAY_BE_COMMITTED=1,XMQTT_JOURNAL_COMMITTED=2
} xmqttjournaleffect;
typedef enum xmqttjournalstate {
    XMQTT_JOURNAL_RECOVERING=0,XMQTT_JOURNAL_READY=1,XMQTT_JOURNAL_POISONED=2
} xmqttjournalstate;
typedef struct xmqttjournalconfig {
    size_t Size;xmqttversion Version;xbytesview Binding;uint64 Generation;
    uint32 PayloadSchema;size_t MaxTransactionBytes,MaxMemoryBytes;uint64 MaxFileBytes;
} xmqttjournalconfig;
typedef struct xmqttjournalrecord {uint64 Sequence;xbytesview Data;} xmqttjournalrecord;
typedef struct xmqttjournalinfo {
    xmqttjournalstate State;xmqttjournaleffect Effect;
    uint64 Sequence,CommittedEnd,AttemptSequence,AttemptEnd;
    size_t MemoryBytes;uint32 Chain;
} xmqttjournalinfo;
XRT_EXTERN_C_BEGIN
/* Defaults: MQTT5, schema1,1MiB transaction,2MiB memory,1GiB file. Set exact
 * opaque broker/principal/slot Binding (1..1024 bytes), nonzero Generation.
 * Schema is an application format discriminator; payload semantics are opaque.
 * This developing storage primitive is not a durable MQTT Client/Inbox. */
XMQTT_API bool xmqttJournalConfigInit(xmqttjournalconfig* Config);
/* Borrow a READ/WRITE absolute-I/O/stat/resize/flush file, without APPEND/ASYNC.
 * Caller exclusively serializes its CONTENT through Destroy and then closes
 * it. No file lock/close/namespace publication is performed here. Native file
 * must already be durably published; virtual provider must implement real sync
 * semantics if durability is required. Filesystem/device assumptions still apply.
 * Create requires length0, initializes version1 and synchronizes before success;
 * failed creation may leave partial bytes. Open validates descriptor and two
 * commit witnesses; replay ALL committed records before Append is admitted.
 * One bounded allocation (including replay buffer), no growing storage cache.
 * Constructors leave Journal untouched on failure; never take File ownership. */
XMQTT_API xmqttjournalstatus xmqttJournalCreate(xfile File,const xmqttjournalconfig* Config,xmqttjournal** Journal);
XMQTT_API xmqttjournalstatus xmqttJournalOpen(xfile File,const xmqttjournalconfig* Config,xmqttjournal** Journal);
/* At most one transaction per call. Record.Data is borrowed until ANY next
 * operation/Destroy. Validate committed frames/checksums before publishing a
 * record. EMPTY verifies final sequence/chain, truncates any uncommitted suffix
 * and syncs before READY. Corrupt committed data is fatal, never older fallback.
 * A local capacity LIMIT leaves the cursor intact; reopen with larger limits.
 * Replay can have returned earlier records before a later failure: callers
 * must stage recovered application state until EMPTY establishes readiness. */
XMQTT_API xmqttjournalstatus xmqttJournalNext(xmqttjournal* Journal,xmqttjournalrecord* Record);
/* Storage-worker primitive: blocking two sync barriers, never call in a network
 * callback. Borrow immutable disjoint payload only during this call. Success
 * writes frame, syncs data, writes BOTH latest witnesses and syncs publication.
 * No allocation/payload copy on success; two payload CRC scans, three frame
 * writes, two4096-byte witness writes and two flushes for nonempty payload.
 * Before witness write: IO means definitely not committed. From first witness
 * write: UNCERTAIN may be recovered. Both failures poison; reopen to resolve.
 * If error-slot restoration fails after both barriers, ERROR poisons the owner
 * but Info still reports COMMITTED and the advanced committed sequence/end.
 * A fully published transaction can survive even when its caller saw no success.
 * Sequence output changes only on DONE. LIMIT is normal refusal without I/O. */
XMQTT_API xmqttjournalstatus xmqttJournalAppend(xmqttjournal* Journal,xbytesview Data,uint64* Sequence);
/* One serialized owner, no concurrent/reentrant operations. Reentry from a
 * synchronous file backend is rejected, including Info/Destroy. All caller
 * outputs must be disjoint from inputs and all Journal storage. DONE/EMPTY/LIMIT
 * preserve ambient errors without notifying their handlers again. Checked
 * error-slot failure returns ERROR and leaves caller outputs untouched; entry
 * failure performs no I/O, exit failure poisons an existing owner. The slot may
 * have no diagnostic when its native setter fails. Other failures publish an
 * operation error. Diagnostics may allocate. CRC detects accidental corruption,
 * not forgery or rollback. */
XMQTT_API bool xmqttJournalInfo(xmqttjournal* Journal,xmqttjournalinfo* Info);
XMQTT_API bool xmqttJournalDestroy(xmqttjournal* Journal);
XRT_EXTERN_C_END
#endif
#endif
