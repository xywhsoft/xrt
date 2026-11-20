#ifndef XMQTT_INTERNAL_STORE_CHECKPOINT_ONLINE_H
#define XMQTT_INTERNAL_STORE_CHECKPOINT_ONLINE_H
#include <xmqtt/store_checkpoint_online.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_CHECKPOINT_ONLINE)
typedef bool (*xmqttcheckpointstop)(ptr Context);
typedef struct xmqttcheckpointstats {
    uint64 Transactions,Operations,ReplayBytes;
} xmqttcheckpointstats;

XMQTT_INTERNAL bool xmqttInternalJournalCheckpointCurrent(const xmqttjournal* Journal,const void* Owner,
    xmqttstorecheckpointresult* Result,uint64* DataBytes);
XMQTT_INTERNAL uint64 xmqttInternalJournalCheckpointBytes(uint64 Transactions,uint64 PayloadBytes);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalCheckpointStage(xmqttjournal* Current,
    xmqttstorecheckpointprepare Prepare,ptr Context,xmqttjournal** Candidate);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalCheckpointPublish(xmqttjournal* Candidate,
    xmqttstorecheckpointselect Select,ptr Context,xmqttstorecheckpointresult* Result);
XMQTT_INTERNAL void xmqttInternalJournalCheckpointFinish(xmqttjournal** Current,const void* Owner,xmqttjournal* Candidate,
    xmqttstorecheckpointresult* Result);

XMQTT_INTERNAL xmqttjournaladmission xmqttInternalJournalCheckpointSubmit(xmqttjournalworker* Worker,
    const xmqttstorecheckpointrequest* Request,double Deadline,xfuture** Out);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCheckpointWork(xmqttstorewrite* Writer,
    xmqttjournal** Journal,const void* Owner,const xmqttstorecheckpointrequest* Request,
    xmqttcheckpointstop Stop,ptr StopContext,xmqttstorecheckpointresult* Result);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCheckpointModel(const xmqttstorestate* State,const void* StateOwner,
    xmqttjournal** Journal,const void* Owner,const xmqttstorecheckpointrequest* Request,
    xmqttcheckpointstop Stop,ptr StopContext,xmqttstorecheckpointresult* Result,xmqttcheckpointstats* Stats);
XMQTT_INTERNAL void xmqttInternalStoreCheckpointRebase(xmqttstorestate* State,const void* StateOwner,
    const xmqttjournalinfo* Journal,const xmqttstorecheckpointresult* Result,const xmqttcheckpointstats* Stats);
#endif
#endif
