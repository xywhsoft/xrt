#ifndef XMQTT_EXAMPLE_STORE_COMPACT_ONLINE_H
#define XMQTT_EXAMPLE_STORE_COMPACT_ONLINE_H

#include <xmqtt/store_checkpoint_online.h>

typedef struct mqttExampleCompactConfig {
    size_t MaxScratchBytes,MaxEntries;
    uint64 MinReclaimBytes;
} mqttExampleCompactConfig;

static inline void mqttExampleCompactConfigInit(mqttExampleCompactConfig* Config)
{*Config=(mqttExampleCompactConfig){1048576,1024,65536};}

static inline xmqttjournalstatus mqttExampleApplyCheckpoint(xmqttstorecheckpointcut* Cut,
    uint64 CurrentBytes,uint64 CurrentDataBytes,uint64 Minimum)
{
    if(!CurrentDataBytes||CurrentDataBytes<Minimum)return XMQTT_JOURNAL_EMPTY;
    uint64 Bytes=0;
    xmqttjournalstatus Status=xmqttStoreCheckpointMeasure(Cut,&Bytes);
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    if(Bytes>=CurrentBytes||CurrentBytes-Bytes<Minimum)return XMQTT_JOURNAL_EMPTY;
    Status=xmqttStoreCheckpointStage(Cut);
    if(Status==XMQTT_JOURNAL_UNCERTAIN)return XMQTT_JOURNAL_IO;
    if(Status!=XMQTT_JOURNAL_DONE)return Status;
    return xmqttStoreCheckpointPublish(Cut);
}

static inline xmqttjournaladmission mqttExampleCompactAsync(xmqttstorewrite* Writer,
    const mqttExampleCompactConfig* Config,int64 Timeout,xfuture** Future)
{
    const xmqttstorecheckpointrequest Request={
        sizeof(Request),Config->MaxScratchBytes,Config->MaxEntries,mqttExampleApplyCheckpoint,Config->MinReclaimBytes
    };
    return xmqttStoreWriteCheckpointAsync(Writer,&Request,Timeout,Future);
}

#endif
