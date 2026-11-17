#ifndef XMQTT_INTERNAL_JOURNAL_WORKER_H
#define XMQTT_INTERNAL_JOURNAL_WORKER_H
#include <xmqtt/journal.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
XMQTT_INTERNAL bool xmqttInternalJournalWorkerClaim(xmqttjournal* Journal,const void* Owner,void* Output,size_t Bytes,xmqttjournalinfo* Info);
XMQTT_INTERNAL xmqttjournalstatus xmqttInternalJournalWorkerAppend(xmqttjournal* Journal,const void* Owner,xbytesview Data,uint64* Sequence);
XMQTT_INTERNAL bool xmqttInternalJournalWorkerInfo(xmqttjournal* Journal,const void* Owner,xmqttjournalinfo* Info);
XMQTT_INTERNAL bool xmqttInternalJournalWorkerDestroy(xmqttjournal* Journal,const void* Owner);
#endif
#endif
