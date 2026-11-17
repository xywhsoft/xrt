#ifndef XMQTT_STORE_WRITE_H
#define XMQTT_STORE_WRITE_H
#include <xmqtt/store_state.h>
#include <xmqtt/journal_worker.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE) && (!defined(XMQTT_FEATURE_XMQTT_STORE_STATE) || !defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER))
#error "MQTT live store writer requires recovered State and host-pool Journal worker"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
typedef struct xmqttstorewrite xmqttstorewrite;
typedef struct xmqttstorewriteconfig {size_t Size,MaxScratchBytes,MaxModelWork;xmqttjournalworkerconfig Worker;} xmqttstorewriteconfig;
typedef struct xmqttstorewriteinfo {size_t Size,ScratchBytes;xmqttstorestateinfo State;xmqttjournalworkerinfo Worker;} xmqttstorewriteinfo;
XRT_EXTERN_C_BEGIN
/* Developing semantic storage owner, not a connected MQTT Client. Defaults:
 * 1MiB writer+operation/property scratch+one staged metadata plan,
 * 65536 model work units per batch,
 * native Worker defaults. Work counts records plus base/overlay entries visited
 * during reset; packet bytes and entry counts have their independent caps. State's
 * original count/memory/lifetime-work/codec limits continue to apply. Retained and
 * staged new payloads use that State memory cap. Metadata uses reserved writer
 * scratch instead,so a full model can still acknowledge/retire records; Journal/worker/Core/file
 * and this fixed scratch are additional separately bounded ownership. */
XMQTT_API bool xmqttStoreWriteConfigInit(xmqttstorewriteconfig* Config);
/* Success exclusively consumes the complete recovered State AND its exact
 * matching READY Journal; failure consumes neither. Exact descriptor and final
 * sequence/chain/end are checked. Stop all State readers/borrowed views first;
 * direct State aliases refuse access/destruction after transfer. Pool and
 * already-published exclusive file/provider remain host-owned as for Worker.
 * Create checks fixed scratch plus the largest configured entry/batch reset
 * plan fits MaxScratchBytes before taking Journal. Constructors perform no IO. Output changes only on success. */
XMQTT_API bool xmqttStoreWriteCreate(xtaskpool* Pool,xmqttjournal* Journal,xmqttstorestate* State,
    const xmqttstorewriteconfig* Config,xmqttstorewrite** Writer);
/* Copy/admit one encoded typed-delta batch; FIFO and deadline/cancellation/
 * retained-result rules match JournalWorker. On its native host task, decode
 * and stage legal transitions/resources BEFORE any append. Invalid batches
 * resolve INVALID, caps LIMIT, with AppendCalled=false and NOT_COMMITTED, leaving
 * committed state usable and unchanged. Local initialization/storage failures
 * stop the owner. Ordinary batches stage only touched metadata and new wires;
 * protocol reset may visit all entries within MaxModelWork. After recovery,
 * a fresh recorded OPEN is required before accepting online protocol flows. No whole-State payload snapshot.
 * COMMITTED atomically installs the already prepared model even when a late
 * timeout/cancel/Close or error restoration changes local status. MAY requires
 * destruction/recovery. No post-sync allocation can prevent model installation.
 * Retired payloads and metadata are freed outside the model gate after complete
 * publication; backing Free callbacks may inspect this live owner. Payload
 * credits remain charged until actual destruction returns, before resolution.
 * Future is the ordinary immutable xmqttcommitresult. This primitive emits no
 * network ACK, business completion or delivery lease; a live Client driver
 * must still gate output and authenticate actual broker decisions. */
XMQTT_API xmqttjournaladmission xmqttStoreWriteAppendAsync(xmqttstorewrite* Writer,
    xbytesview EncodedDelta,int64 Timeout,xfuture** Future);
/* Separately sampled model/worker snapshots, not a combined linearizable pair.
 * ScratchBytes reports fixed requested bytes; the remaining MaxScratchBytes is
 * reserved for at most one native metadata plan. ClientId is immutable borrowed
 * owner storage,readable until Destroy. */
XMQTT_API bool xmqttStoreWriteInfo(xmqttstorewrite* Writer,xmqttstorewriteinfo* Info);
/* Copy a committed key's canonical wire and metadata under the model gate.
 * No allocation. Required changes on DONE/LIMIT only; View only on DONE and
 * Wire then refers to caller Buffer. EMPTY leaves both outputs unchanged.
 * All caller writable spans must be mutually disjoint and outside owned state.
 * A size query may LIMIT; another transaction can remove the key before retry.
 * This read is neither a delivery claim/lease nor an application ACK capability. */
XMQTT_API xmqttstorestatus xmqttStoreWriteRead(xmqttstorewrite* Writer,uint64 Key,
    ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required);
XMQTT_API bool xmqttStoreWriteClose(xmqttstorewrite* Writer);
/* Close first; same-pool or own callback/admission joins are rejected. */
XMQTT_API xwaitresult xmqttStoreWriteWaitFor(xmqttstorewrite* Writer,int64 Timeout);
/* Serialize with API entries. Close+physical worker wait, destroy State and
 * transferred Journal. Retained results keep only worker ledger/input bytes.
 * Host still closes file/provider and joins/destroys its own pool. */
XMQTT_API bool xmqttStoreWriteDestroy(xmqttstorewrite* Writer);
XRT_EXTERN_C_END
#endif
#endif
