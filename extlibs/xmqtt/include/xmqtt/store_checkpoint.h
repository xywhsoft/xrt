#ifndef XMQTT_STORE_CHECKPOINT_H
#define XMQTT_STORE_CHECKPOINT_H
#include <xmqtt/store_state.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT) && !defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
#error "MQTT checkpoints require typed recovery"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT)
typedef struct xmqttstorecheckpoint xmqttstorecheckpoint;
XRT_EXTERN_C_BEGIN
/* Take an immutable snapshot of the fully validated typed Journal prefix.
 * The exact descriptor, original cursor, wall clock and replay/memory limits
 * follow StoreStateRecover. The snapshot preserves the stored expiry fields
 * and protocol state at the durable cut. It is independent of the source
 * Journal's lifetime. On EMPTY or failure the output stays untouched. */
XMQTT_API xmqttstorestatus xmqttStoreCheckpointRecover(xmqttjournal* Source,
    const xmqttstorestateconfig* Config,xmqttstorecheckpoint** Checkpoint);
/* Encode the snapshot into a different, empty READY Journal with the same
 * MQTT version, binding and typed schema. The application chooses its
 * generation and storage. One output buffer is capped by MaxScratchBytes
 * and the destination and delta transaction limits. A partial or uncertain
 * write can leave bytes in Destination. Publish it for recovery only on DONE.
 * Keep Checkpoint alive throughout the call and serialize access to Destination. */
XMQTT_API xmqttjournalstatus xmqttStoreCheckpointWrite(const xmqttstorecheckpoint* Checkpoint,
    xmqttjournal* Destination,size_t MaxScratchBytes);
XMQTT_API void xmqttStoreCheckpointDestroy(xmqttstorecheckpoint* Checkpoint);
XRT_EXTERN_C_END
#endif
#endif
