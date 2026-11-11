#include <xredis/pool_asking.h>
#include "../internal/pool_admit.h"
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
XREDIS_API xredisadmission xredisLeaseAskingCallAsync(xredislease* L,const xbytesview* A,size_t N,int64 T,xfuture** F)
{return __xredisLeaseAsking(L,A,N,T,F,xrtTimer(),false);}
XREDIS_API xredisadmission xredisLeaseAskingCall(xredislease* L,const xbytesview* A,size_t N,int64 T,xfuture** F)
{return __xredisLeaseAsking(L,A,N,T,F,xrtTimer(),true);}
#endif
