#ifndef XMQTT_JOURNAL_SLOT_H
#define XMQTT_JOURNAL_SLOT_H
#include <xmqtt/journal.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT) && !defined(XMQTT_FEATURE_XMQTT_JOURNAL)
#error "Journal slot rotation requires the MQTT journal"
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT)
typedef struct xmqttjournalslot xmqttjournalslot;
typedef struct xmqttjournalslotfiles {
    xfile Control,Data[2];
} xmqttjournalslotfiles;
typedef struct xmqttjournalslotinfo {
    uint64 Generation,PublishedSequence;
    xmqttjournaleffect Effect;
    bool Poisoned,JournalOpen,Prepared;
} xmqttjournalslotinfo;
XRT_EXTERN_C_BEGIN
/* Borrow three handles referring to distinct files through Slot Destroy.
 * Supply exclusive access to their contents and synchronize their initial
 * publication in the APP. Control needs synchronous absolute read/write,
 * stat and flush; both data files additionally require resize.
 * Create requires three empty files. Config.Generation is the exact initial
 * generation for both constructors. Handles, Config and Binding are copied.
 * The two data-file limits apply independently. Failed construction leaves
 * the output untouched and the handles with the APP. Initial writes can leave
 * incomplete bytes. File I/O belongs outside network callbacks. */
XMQTT_API xmqttjournalstatus xmqttJournalSlotCreate(const xmqttjournalslotfiles* Files,
    const xmqttjournalconfig* Config,xmqttjournalslot** Slot);
XMQTT_API xmqttjournalstatus xmqttJournalSlotOpen(const xmqttjournalslotfiles* Files,
    const xmqttjournalconfig* Config,xmqttjournalslot** Slot);
/* Open the exact selected generation. Replay all records through EMPTY to
 * verify its complete history and the published prefix. The returned Journal
 * may transfer into the existing JournalWorker. Join and destroy it before
 * opening another Journal or preparing a replacement through the public slot
 * API. An online Store Writer can rotate its own child on the storage task.
 * Serialize direct Slot access with that Writer lifetime. */
XMQTT_API xmqttjournalstatus xmqttJournalSlotOpenCurrent(xmqttjournalslot* Slot,xmqttjournal** Journal);
/* After the selected generation has been fully recovered and retired, reset
 * only the inactive file and initialize it at Generation+1. Supply a complete
 * replacement using ordinary synchronous Journal operations. The application
 * owns payload equivalence, including any MQTT snapshot or compaction rules.
 * Destroy the prepared Journal to abandon it. It cannot enter JournalWorker
 * before Publish. No publication is attempted if preparation fails. */
XMQTT_API xmqttjournalstatus xmqttJournalSlotPrepare(xmqttjournalslot* Slot,xmqttjournal** Journal);
/* Select the prepared Journal after its durable initialization/appends.
 * Publish writes two matching selector witnesses then syncs the control file.
 * On success the same Journal becomes the current one without a reopen.
 * UNCERTAIN poisons the Slot and Journal. Destroy both and reopen to resolve.
 * Info retains the last attempted publication effect. Local error restoration
 * can fail after COMMITTED. Recover only the selected generation. */
XMQTT_API xmqttjournalstatus xmqttJournalSlotPublish(xmqttjournalslot* Slot,xmqttjournal* Journal);
XMQTT_API bool xmqttJournalSlotInfo(xmqttjournalslot* Slot,xmqttjournalslotinfo* Info);
/* Refuses while a child or an operation is live. On success the APP can
 * close the three file handles. */
XMQTT_API bool xmqttJournalSlotDestroy(xmqttjournalslot* Slot);
XRT_EXTERN_C_END
#endif
#endif
