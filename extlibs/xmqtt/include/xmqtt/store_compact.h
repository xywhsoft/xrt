#ifndef XMQTT_STORE_COMPACT_H
#define XMQTT_STORE_COMPACT_H
#include <xmqtt/journal_slot.h>
#include <xmqtt/store_state.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT) && (!defined(XMQTT_FEATURE_XMQTT_STORE_STATE) || !defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT))
#error "MQTT store compaction requires typed recovery and a native slot"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT)
XRT_EXTERN_C_BEGIN
/* Blocking offline replacement of a typed slot's selected generation.
 * Exclusively own the Slot and retire all Journal/Worker/Client owners first.
 * Config identifies the current generation. Recovery and checkpoint creation
 * obey its original replay and memory limits. MaxScratchBytes bounds one
 * additional output buffer, also capped by both transaction byte limits.
 * Replays the complete durable prefix, preserving epoch, the highest issued key,
 * raw expiry and disconnect wall, protocol stages and pending Inbox deliveries.
 * Empty source history returns EMPTY; LIMIT can leave unused inactive bytes.
 * On DONE, query SlotInfo.Generation and reopen the current Journal to recover
 * the checkpoint before transferring it to a new writer. Earlier readers and
 * objects retain their original contents and lifetimes.
 * Source/torn checkpoint corruption returns CORRUPT. A failure before selector
 * publication leaves the old generation selected. With UNCERTAIN or a poisoned
 * slot, retire and reopen to resolve. SlotInfo reports the actual selector
 * effect even if final cleanup or error-slot restoration fails.
 * Writes the checkpoint payload format accepted by this version of StoreState
 * recovery. Older StoreState readers reject that payload. Serialize calls and
 * keep the namespace and inputs stable. Never call from a network callback. */
XMQTT_API xmqttjournalstatus xmqttStoreCompact(xmqttjournalslot* Slot,
    const xmqttstorestateconfig* Config,size_t MaxScratchBytes);
XRT_EXTERN_C_END
#endif
#endif
