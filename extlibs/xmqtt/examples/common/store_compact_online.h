#ifndef XMQTT_EXAMPLE_STORE_COMPACT_ONLINE_H
#define XMQTT_EXAMPLE_STORE_COMPACT_ONLINE_H

#include <xmqtt/store_checkpoint_online.h>
#include "journal_slot.h"

typedef struct mqttExampleCompactConfig {
    size_t MaxScratchBytes,MaxEntries;
    uint64 MinReclaimBytes;
} mqttExampleCompactConfig;

static inline void mqttExampleCompactConfigInit(mqttExampleCompactConfig* Config)
{*Config=(mqttExampleCompactConfig){1048576,1024,65536};}

/* Called only from the storage FIFO after the selected Journal was
 * replayed and transferred. The APP owns the selector and both files. */
static inline xmqttjournalstatus mqttExamplePrepareCheckpoint(ptr Context,uint64 Generation,
    const xmqttjournalinfo* Current,xmqttjournal** Candidate,bool* Fence)
{
    mqttExampleJournalSlot* S=Context;
    if(!S||S->Child||S->Busy||S->Poisoned||!S->Validated||S->Current.Generation!=Generation||
       Current->State!=XMQTT_JOURNAL_READY)
        return mqttExampleSlotError(XMQTT_JOURNAL_STATE,"online replacement requires a verified selected slot");
    S->Busy=true;
    xmqttjournalstatus Status=mqttExampleJSRepair(S);
    if(Status==XMQTT_JOURNAL_DONE)Status=mqttExampleJSData(S,Generation+1,true,true,Candidate);
    *Fence=S->Poisoned;
    S->Busy=false;
    return Status;
}

static inline xmqttjournalstatus mqttExampleSelectCheckpoint(ptr Context,uint64 Generation,
    const xmqttjournalinfo* Candidate,xmqttjournaleffect* Effect)
{
    mqttExampleJournalSlot* S=Context;
    if(!S||S->Child||S->Busy||S->Poisoned||!S->Validated||Generation!=S->Current.Generation+1)
        return mqttExampleSlotError(XMQTT_JOURNAL_STATE,"checkpoint selector requires the slot's next generation");
    S->Busy=true;
    xmqttjournalstatus Status=mqttExampleJSSelect(S,Generation,Candidate);
    *Effect=S->Effect;
    S->Busy=false;
    return Status;
}

static inline xmqttjournalstatus mqttExampleApplyCheckpoint(xmqttstorecheckpointcut* Cut,
    uint64 CurrentBytes,uint64 CurrentDataBytes,ptr Context,uint64 Minimum)
{
    if(!CurrentDataBytes||CurrentDataBytes<Minimum)return XMQTT_JOURNAL_EMPTY;
    uint64 Bytes=0;
    xmqttjournalstatus Status=xmqttStoreCheckpointMeasure(Cut,&Bytes);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    if(Bytes>=CurrentBytes||CurrentBytes-Bytes<Minimum)return XMQTT_JOURNAL_EMPTY;
    Status=xmqttStoreCheckpointStage(Cut,mqttExamplePrepareCheckpoint,Context);
    if(Status==XMQTT_JOURNAL_UNCERTAIN)return XMQTT_JOURNAL_IO;
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    return xmqttStoreCheckpointPublish(Cut,mqttExampleSelectCheckpoint,Context);
}

static inline xmqttjournaladmission mqttExampleCompactAsync(xmqttstorewrite* Writer,mqttExampleJournalSlot* Slot,
    const mqttExampleCompactConfig* Config,int64 Timeout,xfuture** Future)
{
    const xmqttstorecheckpointrequest Request={
        sizeof(Request),Config->MaxScratchBytes,Config->MaxEntries,mqttExampleApplyCheckpoint,Slot,Config->MinReclaimBytes
    };
    return xmqttStoreWriteCheckpointAsync(Writer,&Request,Timeout,Future);
}

#endif
