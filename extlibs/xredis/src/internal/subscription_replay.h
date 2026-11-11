#ifndef XREDIS_SUBSCRIPTION_REPLAY_INTERNAL_H
#define XREDIS_SUBSCRIPTION_REPLAY_INTERNAL_H
#include <xredis/subscriptions.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
xredisadmission __xredisSubscriberReplayStart(xredisclient* Client,const xredissubscriberconfig* Config,ptr Owner);
void __xredisSubscriberReplayRelease(xredisclient* Client,ptr Owner);
xredisadmission __xredisSubscriberCommandBudget(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,double Entered,ptr Owner,xfuture** Future);
#endif
#endif
