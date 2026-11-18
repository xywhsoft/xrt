#ifndef XMQTT_INTERNAL_STORE_COMPACT_ONLINE_H
#define XMQTT_INTERNAL_STORE_COMPACT_ONLINE_H
#include <xmqtt/store_compact_online.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
typedef bool (*xmqttcompactstop)(ptr Context);
typedef xmqttjournalstatus (*xmqttcompactencode)(xmqttjournal* Candidate,ptr Context);
typedef struct xmqttcheckpointstats {
    uint64 Transactions,Operations,ReplayBytes;
} xmqttcheckpointstats;
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalCompactCheck(const xmqttjournal* Journal,const void* Owner,
    uint64 Transactions,uint64 ReplayBytes,uint64 MinReclaim,xmqttstorecompactresult* Result);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalCompactReplace(xmqttjournal** Journal,const void* Owner,
    xmqttcompactencode Encode,ptr Context,xmqttcompactstop Stop,ptr StopContext,xmqttstorecompactresult* Result);
XMQTT_INTERNAL xmqttjournaladmission xmqttInternalJournalCompactSubmit(xmqttjournalworker* Worker,
    const xmqttstorecompactconfig* Config,double Deadline,xfuture** Out);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCompactWork(xmqttstorewrite* Writer,
    xmqttjournal** Journal,const void* Owner,const xmqttstorecompactconfig* Config,
    xmqttcompactstop Stop,ptr StopContext,xmqttstorecompactresult* Result);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCompactModel(const xmqttstorestate* State,const void* StateOwner,
    xmqttjournal** Journal,const void* Owner,const xmqttstorecompactconfig* Config,
    xmqttcompactstop Stop,ptr StopContext,xmqttstorecompactresult* Result,xmqttcheckpointstats* Stats);
XMQTT_INTERNAL void xmqttInternalStoreCompactRebase(xmqttstorestate* State,const void* StateOwner,
    const xmqttjournalinfo* Journal,const xmqttstorecompactresult* Result,const xmqttcheckpointstats* Stats);
#endif
#endif
