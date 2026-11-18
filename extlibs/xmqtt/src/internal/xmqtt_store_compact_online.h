#ifndef XMQTT_INTERNAL_STORE_COMPACT_ONLINE_H
#define XMQTT_INTERNAL_STORE_COMPACT_ONLINE_H
#include <xmqtt/store_compact_online.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE)
XMQTT_INTERNAL bool xmqttInternalJournalCompactAvailable(const xmqttjournal* Journal,const void* Owner);
XMQTT_INTERNAL bool xmqttInternalJournalCompactFilePressure(const xmqttjournal* Journal,size_t Bytes);
XMQTT_INTERNAL bool xmqttInternalJournalCompactFits(const xmqttjournal* Journal,uint64 Transactions,
    uint64 ReplayBytes,uint64 NextBytes);
typedef struct xmqttcompactionrequest {
    uint64 Operations,Bytes;
    bool ReplayBlocked,NoRoom;
} xmqttcompactionrequest;
typedef struct xmqttautocompactstate {
    xmqttstoreautocompactconfig Config;
    xmqttstoreautocompactinfo Info;
    uint64 DeniedEnd,DeniedOperations,DeniedBytes;
} xmqttautocompactstate;
XMQTT_INTERNAL bool xmqttInternalJournalAutoCompactSet(xmqttjournalworker* Worker,
    xmqttautocompactstate* State,const xmqttstoreautocompactconfig* Config);
XMQTT_INTERNAL bool xmqttInternalJournalAutoCompactInfo(xmqttjournalworker* Worker,xmqttstoreautocompactinfo* Info);

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
    xmqttcompactstop Stop,ptr StopContext,xmqttstorecompactresult* Result,xmqttcompactionrequest* Request);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalStoreCompactModel(const xmqttstorestate* State,const void* StateOwner,
    xmqttjournal** Journal,const void* Owner,const xmqttstorecompactconfig* Config,
    xmqttcompactstop Stop,ptr StopContext,xmqttstorecompactresult* Result,xmqttcheckpointstats* Stats,
    xmqttcompactionrequest* Request);
XMQTT_INTERNAL void xmqttInternalStoreCompactRebase(xmqttstorestate* State,const void* StateOwner,
    const xmqttjournalinfo* Journal,const xmqttstorecompactresult* Result,const xmqttcheckpointstats* Stats);
#endif
#endif
