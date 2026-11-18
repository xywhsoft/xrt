#ifndef XMQTT_JOURNAL_SLOT_H
#define XMQTT_JOURNAL_SLOT_H
#include <xmqtt/journal_file.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT) && !defined(XMQTT_FEATURE_XMQTT_JOURNAL_FILE)
#error "Journal slot rotation requires the locked MQTT journal"
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT)
typedef struct xmqttjournalslot xmqttjournalslot;
typedef struct xmqttjournalslotinfo {
    uint64 Generation,PublishedSequence;
    xmqttjournaleffect Effect;
    bool Poisoned,JournalOpen,Prepared;
} xmqttjournalslotinfo;
XRT_EXTERN_C_BEGIN
/* Borrow the anchored directory until Destroy. It contains three distinct
 * regular files: journal.slot, journal.0 and journal.1. Provision these names
 * durably and keep them stable. Create requires all three files empty.
 * Config.Generation is the exact initial generation for both constructors.
 * Config and Binding are copied. The two data-file limits apply independently.
 * One control-file lock excludes cooperating owners until physical Destroy.
 * Serialize control operations with the Journal's lifecycle. Functions that
 * touch files can block and belong outside network callbacks. BUSY preserves
 * the output. A failed Create can leave an incomplete initial installation. */
XMQTT_API xmqttjournalstatus xmqttJournalSlotCreate(xroot Root,const xmqttjournalconfig* Config,xmqttjournalslot** Slot);
XMQTT_API xmqttjournalstatus xmqttJournalSlotOpen(xroot Root,const xmqttjournalconfig* Config,xmqttjournalslot** Slot);
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
/* With a live Journal, refuse and retain ownership. On a native close error,
 * retry Destroy to free the poisoned wrapper. */
XMQTT_API bool xmqttJournalSlotDestroy(xmqttjournalslot* Slot);
XRT_EXTERN_C_END
#endif
#endif
