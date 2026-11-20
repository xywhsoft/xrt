#ifndef XMQTT_STORE_CHECKPOINT_ONLINE_H
#define XMQTT_STORE_CHECKPOINT_ONLINE_H
#include <xmqtt/journal_slot.h>
#include <xmqtt/store_checkpoint.h>
#include <xmqtt/store_write.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT_ONLINE) && \
    (!defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT) || !defined(XMQTT_FEATURE_XMQTT_JOURNAL_SLOT) || !defined(XMQTT_FEATURE_XMQTT_STORE_WRITE))
#error "Online MQTT checkpoints require the typed slot and Store Writer"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT_ONLINE)
typedef struct xmqttstorecheckpointcut xmqttstorecheckpointcut;
typedef xmqttjournalstatus (*xmqttstorecheckpointapply)(xmqttstorecheckpointcut* Cut,
    uint64 CurrentBytes,uint64 CurrentDataBytes,uint64 User);
typedef struct xmqttstorecheckpointrequest {
    size_t Size,MaxScratchBytes,MaxEntries;
    xmqttstorecheckpointapply Apply;
    uint64 User;
} xmqttstorecheckpointrequest;
typedef struct xmqttstorecheckpointresult {
    xmqttcommitstatus Status;xmqttjournalstatus StorageStatus;xmqttjournaleffect Effect;
    uint64 PreviousGeneration,Generation,Sequence,PreviousBytes,Bytes;
    const xerror* Cause;
} xmqttstorecheckpointresult;
XRT_EXTERN_C_BEGIN
/* Queue Apply at an exclusive Writer FIFO boundary on its storage task.
 * The Writer owns the selected Slot child. Keep Slot and its files alive
 * through Writer Destroy. Apply runs outside Writer and Worker locks.
 * Cut is valid only synchronously in Apply on that task, including its
 * calls to Measure, Stage and Publish. Apply must return before waiting
 * for any later operation or for destruction of this Writer.
 * CurrentBytes includes framing and the fixed descriptor/witness region.
 * CurrentDataBytes excludes that fixed region. Return EMPTY to skip, or
 * propagate the status from the last checkpoint operation. Request and
 * User are copied on admission. Only ACCEPTED changes the Future output.
 * The resolved immutable result reports the observed selector effect.
 * COMMITTED rebases the same Writer before the next request runs, even
 * if a late stop accompanies the effect. MAY fences the Writer until a
 * fresh recovery. Other results retain the original generation and cut. */
XMQTT_API xmqttjournaladmission xmqttStoreWriteCheckpointAsync(xmqttstorewrite* Writer,
    const xmqttstorecheckpointrequest* Request,int64 Timeout,xfuture** Future);
/* Measure the encoded Journal size, including framing. Scratch capacity
 * and entries are capped by the request. A committed reset is required
 * after recovery projected unsynchronized expiry. No storage I/O. */
XMQTT_API xmqttjournalstatus xmqttStoreCheckpointMeasure(xmqttstorecheckpointcut* Cut,uint64* Bytes);
/* After measurement, encode a replacement in the inactive file. One
 * attempt per cut, with bounded scratch and no historical replay. An
 * uncertain inactive append leaves the selected Journal in use. A failed
 * selector witness repair fences the Writer. */
XMQTT_API xmqttjournalstatus xmqttStoreCheckpointStage(xmqttstorecheckpointcut* Cut);
/* Publish a successful Stage once. A stop observed before publication
 * abandons the candidate. Once selector I/O enters, it retires physically.
 * Returning from Apply abandons any candidate that was not published. */
XMQTT_API xmqttjournalstatus xmqttStoreCheckpointPublish(xmqttstorecheckpointcut* Cut);
XRT_EXTERN_C_END
#endif
#endif
