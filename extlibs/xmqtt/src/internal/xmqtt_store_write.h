#ifndef XMQTT_INTERNAL_STORE_WRITE_H
#define XMQTT_INTERNAL_STORE_WRITE_H
#include <xmqtt/store_state.h>
#include <xmqtt/journal_worker.h>
#include "xmqtt_internal.h"
#include "xmqtt_journal_worker.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
typedef struct xmqttstoreplan xmqttstoreplan;
XMQTT_INTERNAL bool xmqttInternalStoreWriteClaim(xmqttstorestate* State,xmqttjournal* Journal,const void* Owner,void* Output,size_t Bytes,xmqttstoredeltaconfig* Delta,size_t* MinimumPlanBytes);
XMQTT_INTERNAL void xmqttInternalStoreWriteRelease(xmqttstorestate* State,const void* Owner,bool Destroy);
XMQTT_INTERNAL bool xmqttInternalStoreWriteOutput(xmqttstorestate* State,const void* Owner,void* Output,size_t Bytes);
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreWritePrepare(xmqttstorestate* State,const void* Owner,const xmqttstoredeltadocument* Delta,size_t MaxWork,size_t MaxPlanBytes,xmqttstoreplan** Plan);
XMQTT_INTERNAL void xmqttInternalStoreWriteFinish(xmqttstorestate* State,const void* Owner,xmqttstoreplan* Plan,bool Committed,uint64 Sequence);
XMQTT_INTERNAL size_t xmqttInternalStoreWriteDispose(xmqttstorestate* State,const void* Owner,xmqttstoreplan* Plan);
XMQTT_INTERNAL void xmqttInternalStoreWriteRefund(xmqttstorestate* State,const void* Owner,size_t Bytes);
XMQTT_INTERNAL void xmqttInternalStoreWriteInfo(xmqttstorestate* State,const void* Owner,xmqttstorestateinfo* Info);
XMQTT_INTERNAL void xmqttInternalStoreWriteIndexPending(xmqttstorestate* State,const void* Owner);
XMQTT_INTERNAL xmqttstorestatus xmqttInternalStoreWriteRead(xmqttstorestate* State,const void* Owner,uint64 Key,ptr Buffer,size_t Capacity,xmqttstoreview* View,size_t* Required);
XMQTT_INTERNAL bool xmqttJournalWriteStart(xmqttjournal* Journal,xmqttversion Version,xbytesview Binding,uint64 Generation,uint64 Sequence,uint32 Chain,uint64 End,void* Output,size_t Bytes);
typedef xmqttcommitstatus (*xmqttstoreprepare)(ptr Context,xbytesview Data);
typedef void (*xmqttstorefinish)(ptr Context,const xmqttcommitresult* Result);
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalJournalWorkerAppendAt(xmqttjournalworker* Worker,xbytesview Data,double Deadline,xfuture** Future);
XMQTT_INTERNAL bool xmqttInternalJournalWorkerBind(xmqttjournalworker* Worker,xmqttstoreprepare Prepare,xmqttstorefinish Finish,ptr Context);
#endif
#endif
