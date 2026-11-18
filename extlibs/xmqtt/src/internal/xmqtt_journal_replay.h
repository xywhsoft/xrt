#ifndef XMQTT_JOURNAL_REPLAY_H
#define XMQTT_JOURNAL_REPLAY_H
#include <xmqtt/journal.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
/* Extension-private descriptor/cursor check. No public borrowed Journal layout. */
XMQTT_INTERNAL bool xmqttJournalReplayStart(xmqttjournal* Journal,xmqttversion Version,
    xbytesview Binding,uint64 Generation,const void* Output,size_t Bytes,uint64* Transactions);
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT)
XMQTT_INTERNAL size_t xmqttJournalReplayTransactionLimit(const xmqttjournal* Journal);
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
XMQTT_INTERNAL bool xmqttJournalWriteStart(xmqttjournal* Journal,xmqttversion Version,xbytesview Binding,uint64 Generation,uint64 Sequence,uint32 Chain,uint64 End,void* Output,size_t Bytes);
#endif
#endif
#endif
