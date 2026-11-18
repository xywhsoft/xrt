#ifndef XMQTT_STORE_COMPACT_ONLINE_H
#define XMQTT_STORE_COMPACT_ONLINE_H
#include <xmqtt/store_compact.h>
#include <xmqtt/store_write.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE) && (!defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT) || !defined(XMQTT_FEATURE_XMQTT_STORE_WRITE))
#error "Online MQTT compaction requires the typed slot and Store Writer"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
typedef struct xmqttstorecompactconfig {
    size_t Size,MaxScratchBytes,MaxEntries;
    uint64 MinReclaimBytes;
} xmqttstorecompactconfig;
typedef struct xmqttstorecompactresult {
    xmqttcommitstatus Status;xmqttjournalstatus StorageStatus;xmqttjournaleffect Effect;
    uint64 PreviousGeneration,Generation,Sequence,PreviousBytes,Bytes;
    const xerror* Cause;
} xmqttstorecompactresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 1 MiB output scratch, 1024 live entries and 64 KiB minimum
 * selected-file reduction. Scratch and entry count are hard caps. */
XMQTT_API bool xmqttStoreCompactConfigInit(xmqttstorecompactconfig* Config);
/* Queue one bounded checkpoint at a FIFO Store Writer commit boundary.
 * The Writer must own a native JournalSlot child. Keep the Slot/root alive
 * through Writer Destroy and serialize direct Slot calls with that lifetime.
 * Connection state and Inbox leases stay with the same Writer. A source
 * whose recovery projected unsynchronized expiry requires a committed reset
 * establishing the durable cut before compaction is eligible.
 * The host storage task scans bounded live entries and writes inactive
 * blocks without replaying history or holding the model gate during I/O.
 * Other commits wait in the existing bounded FIFO. No additional task pool.
 * Scratch is allocated only during execution, capped by both transaction
 * limits. One extra Journal allocation is bounded by the Slot's configuration.
 * An insufficient reduction resolves EMPTY without replacement I/O.
 * The byte figures describe selected journal lengths. The old file remains
 * as the inactive generation until its next reuse. Sequence is generation
 * local, including the ordinary commit results before and after a switch.
 * Entry deadline, Close and explicit cancellation are sampled before the
 * next block or publication. Entered I/O completes physically. A late
 * stop can accompany COMMITTED. A definite inactive-file failure leaves
 * the old Writer usable. An ambiguous selector change fences the Writer.
 * Accepted Future resolves to an immutable xmqttstorecompactresult. On
 * COMMITTED, Generation/Sequence/Bytes describe the new cut. Otherwise
 * they describe the original cut if inspected; with MAY reopen to resolve.
 * The config is copied, and Future changes only on ACCEPTED. */
XMQTT_API xmqttjournaladmission xmqttStoreWriteCompactAsync(xmqttstorewrite* Writer,
    const xmqttstorecompactconfig* Config,int64 Timeout,xfuture** Future);
typedef struct xmqttstoreautocompactconfig {
    size_t Size;
    xmqttstorecompactconfig Compact;
    uint64 TriggerBytes,RetryGrowthBytes;
    uint32 RetryDelayMs;
} xmqttstoreautocompactconfig;
typedef struct xmqttstoreautocompactinfo {
    size_t Size;
    bool Enabled,Running;
    uint64 Attempts,Rotations,NextEligibleBytes;
    double RetryAfter;
    xmqttcommitstatus Status;
    xmqttjournalstatus StorageStatus;
    xmqttjournaleffect Effect;
    uint64 Generation,PreviousBytes,Bytes;
} xmqttstoreautocompactinfo;
/* Defaults: 64 MiB selected-file trigger, 4 MiB committed growth after a
 * nonpublishing attempt, 1000 ms to reconsider an inactive I/O failure,
 * and the manual checkpoint's default bounds. */
XMQTT_API bool xmqttStoreAutoCompactConfigInit(xmqttstoreautocompactconfig* Config);
/* Install or replace the copied policy at an idle Writer boundary. Pass NULL
 * to disable. The Writer must own the selected native JournalSlot child and
 * remain live through this call. Busy, closed or unsupported Writers refuse.
 * Foreground work wins before the pump starts an idle checkpoint. A valid
 * append refused by the physical file cap may checkpoint and retry once
 * within the original deadline. The typed model is restaged on the new cut.
 * Other appends queue normally. No extra task, timer or Future is created.
 * After a nonpublishing attempt, wait for RetryGrowthBytes of new committed
 * bytes. A later foreground request can also retry a definite inactive I/O,
 * BUSY or local allocation failure after RetryDelayMs. Zero disables that
 * clock-based path. Policy replacement resets its history.
 * Close fences new work and waits for any entered native I/O as usual.
 * This policy applies to the physical selected file. Typed replay and entry
 * quotas retain their independent limits. */
XMQTT_API bool xmqttStoreWriteSetAutoCompact(xmqttstorewrite* Writer,const xmqttstoreautocompactconfig* Config);
/* A gate-protected snapshot of the last finished automatic attempt plus
 * the running flag. Disabled returns Enabled=false and zero counters.
 * Generation is the last definite selection, or the original cut on MAY.
 * Zero NextEligibleBytes means there is no committed-growth restriction.
 * RetryAfter is an xrtTimer() deadline. Zero disables the clock-based path.
 * Policy memory is included in the Store Writer's fixed scratch budget. */
XMQTT_API bool xmqttStoreWriteAutoCompactInfo(xmqttstorewrite* Writer,xmqttstoreautocompactinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
