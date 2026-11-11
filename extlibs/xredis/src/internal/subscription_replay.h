#ifndef XREDIS_SUBSCRIPTION_REPLAY_INTERNAL_H
#define XREDIS_SUBSCRIPTION_REPLAY_INTERNAL_H
#include <xredis/subscriptions.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
xredisadmission __xredisSubscriberReplayStart(xredisclient* Client,const xredissubscriberconfig* Config,ptr Owner);
void __xredisSubscriberReplayRelease(xredisclient* Client,ptr Owner);
xredisadmission __xredisSubscriberCommandBudget(xredisclient* Client,xredispubsubcommand Command,
    const xbytesview* Names,size_t Count,int64 Timeout,double Entered,ptr Owner,xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
bool __xredisSubscriptionsFits(xredissubscriptions* Store,const xredissubscriberconfig* Config);
bool __xredisSubscriptionRegistryBounds(const xredissubscriberconfig* Config);
bool __xredisSubscriptionClientProfile(xredisclient* Client,xnetengine* Engine);
xredisadmission __xredisSubscriptionsClaim(xredissubscriptions* Store,ptr Owner,ptr Output);
void __xredisSubscriptionsUnclaim(xredissubscriptions* Store,ptr Owner);
xredisadmission __xredisSubscriptionsApply(xredissubscriptions* Store,xredispubsubcommand Command,const xbytesview* Names,size_t Count,uint64* Revision,ptr Owner);
xredisadmission __xredisSubscriptionTransitionCreate(xredissubscriptions* Store,xredisclient* Client,xredissubscriptionsnapshot* Base,const xredissubscriptionreplayconfig* Config,double Entered,ptr Scope,xredissubscriptionreplay** Output);
xredissubscriptionsnapshot* __xredisSubscriptionTransitionSnapshot(xredissubscriptionreplay* Replay);
#endif
#endif
#endif
