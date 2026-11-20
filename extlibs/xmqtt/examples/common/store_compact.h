#ifndef XMQTT_EXAMPLE_STORE_COMPACT_H
#define XMQTT_EXAMPLE_STORE_COMPACT_H

#include "journal_files.h"
#include <xmqtt/store_checkpoint.h>

static inline xmqttjournalstatus mqttExampleCheckpointRecoverStatus(xmqttstorestatus Status)
{
    switch(Status){
    case XMQTT_STORE_DONE:return XMQTT_JOURNAL_DONE;
    case XMQTT_STORE_EMPTY:return XMQTT_JOURNAL_EMPTY;
    case XMQTT_STORE_LIMIT:return XMQTT_JOURNAL_LIMIT;
    case XMQTT_STORE_CORRUPT:return XMQTT_JOURNAL_CORRUPT;
    case XMQTT_STORE_STATE_ERROR:return XMQTT_JOURNAL_STATE;
    case XMQTT_STORE_IO:return XMQTT_JOURNAL_IO;
    default:return XMQTT_JOURNAL_ERROR;
    }
}

static inline xmqttjournalstatus mqttExampleCompactStore(xmqttjournalslot* Slot,
    const xmqttstorestateconfig* Config,size_t MaxScratchBytes)
{
    xerror* Saved=NULL;
    if(!xrtTrySwapErrorTake(NULL,&Saved))return XMQTT_JOURNAL_ERROR;
    xmqttjournal* Current=NULL;
    xmqttjournal* Next=NULL;
    xmqttstorecheckpoint* Checkpoint=NULL;
    xmqttjournalstatus Status=xmqttJournalSlotOpenCurrent(Slot,&Current);
    if(Status==XMQTT_JOURNAL_DONE)
        Status=mqttExampleCheckpointRecoverStatus(xmqttStoreCheckpointRecover(Current,Config,&Checkpoint));
    if(!xmqttJournalDestroy(Current))Status=XMQTT_JOURNAL_STATE;
    if(Status==XMQTT_JOURNAL_DONE)Status=xmqttJournalSlotPrepare(Slot,&Next);
    if(Status==XMQTT_JOURNAL_DONE){
        Status=xmqttStoreCheckpointWrite(Checkpoint,Next,MaxScratchBytes);
        /* An uncertain append to the inactive file leaves the old selector valid. */
        if(Status==XMQTT_JOURNAL_UNCERTAIN)Status=XMQTT_JOURNAL_IO;
        if(Status==XMQTT_JOURNAL_DONE)Status=xmqttJournalSlotPublish(Slot,Next);
    }
    if(!xmqttJournalDestroy(Next)&&Status!=XMQTT_JOURNAL_UNCERTAIN)Status=XMQTT_JOURNAL_STATE;
    xmqttStoreCheckpointDestroy(Checkpoint);
    return mqttExampleJournalIOResult(Status,Saved);
}

#endif
