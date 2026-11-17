#ifndef XMQTT_JOURNAL_REPLAY_H
#define XMQTT_JOURNAL_REPLAY_H
#include <xmqtt/journal.h>
#include "xmqtt_internal.h"
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
/* Extension-private descriptor/cursor check. No public borrowed Journal layout. */
XMQTT_INTERNAL bool xmqttJournalReplayStart(xmqttjournal* Journal,xmqttversion Version,
    xbytesview Binding,uint64 Generation,const void* Output,size_t Bytes,uint64* Transactions);
#endif
#endif
