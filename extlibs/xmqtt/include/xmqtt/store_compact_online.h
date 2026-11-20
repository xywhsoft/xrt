#ifndef XMQTT_STORE_COMPACT_ONLINE_H
#define XMQTT_STORE_COMPACT_ONLINE_H
#include <xmqtt/journal_slot.h>
#include <xmqtt/store_checkpoint.h>
#include <xmqtt/store_write.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE) && (!defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT) || !defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT) || !defined(XMQTT_FEATURE_XMQTT_STORE_WRITE))
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
 * Connection state and pending records stay with the same Writer. A source
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
XRT_EXTERN_C_END
#endif
#endif
