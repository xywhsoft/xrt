#include <xrpc/tls_dial.h>
#include "../internal/xrpc_tls_connect.h"
#include <xrt/atomic.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_TLS_DIAL)
typedef struct xrpctlsdialdata {
    xatomic32 References;
    xrpctlsconnect Plan;
    xrpctls* Peer;
    xnetresolver* Resolver;
    xnetaddrlist* Addresses;
    xnetdial* Dial;
    xnetdialconfig Config;
    uint16 Port;
    char Name[];
} xrpctlsdialdata;
static bool xrpcTlsDialArgument(cstr Message)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.rpc",19301,Message);return false;}
static void xrpcTlsDialDrop(const void* Data)
{
    xrpctlsdialdata* D=(xrpctlsdialdata*)Data;
    if(xrtAtomic32FetchSub(&D->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtNetDialDestroy(D->Dial);xrtNetAddrListDestroy(D->Addresses);xrtFree(D);
}
static void xrpcTlsDialPlanDrop(ptr Data){xrpcTlsDialDrop(Data);}
static bool xrpcTlsDialCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpctlsdialdata*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcTlsDialTrace(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xrpcTlsDialOps={xrpcTlsDialCount,xrpcTlsDialTrace};
static void xrpcTlsDialDone(xnetdial* Dial,xnetresult Result,xnetstream* Stream,const xerror* Cause,ptr Data)
{(void)Dial;xrpcTlsConnected(((xrpctlsdialdata*)Data)->Peer,Result,Stream,Cause);}
static void xrpcTlsDialRetired(const void* Data)
{
    xrpctlsdialdata* D=(xrpctlsdialdata*)Data;
    /* Same-Worker posted peer work follows the physical owned callback tail.
     * Creator and callback hold real credits; Count is not a lifetime test. */
    xrpcTlsConnectRetired(D->Peer);xrpcTlsDialDrop(D);
}
static const xnetdialownershipv1 xrpcTlsDialPolicy={sizeof(xnetdialownershipv1),xrpcTlsDialDone,xrpcTlsDialRetired,&xrpcTlsDialOps};
static bool xrpcTlsDialStart(ptr Data,xrpctls* Peer,xnetengine* E,uint64 Affinity,const xnetstreamconfig* Stream)
{
    xrpctlsdialdata* D=Data;D->Peer=Peer;D->Config.Stream=*Stream;D->Config.Affinity=Affinity;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED);
    /* Losers have no facade events and cannot start TLS or emit RPC. */
    D->Dial=D->Addresses?xrtNetDialAddressesOwnedV1(E,D->Addresses,&D->Config,NULL,NULL,D,&xrpcTlsDialPolicy):
        xrtNetDialOwnedV1(E,D->Resolver,D->Name,D->Port,&D->Config,NULL,NULL,D,&xrpcTlsDialPolicy);
    if(!D->Dial){xrpcTlsDialDrop(D);return false;}
    xrtNetAddrListDestroy(D->Addresses);D->Addresses=NULL;return true;
}
static void xrpcTlsDialCancel(ptr Data)
{(void)xrtNetDialCancel(((xrpctlsdialdata*)Data)->Dial);}
XRPC_API bool xrpcTlsDialConfigInit(xrpctlsdialconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcTlsDialArgument("invalid TLS dial config output");
    *C=(xrpctlsdialconfig){sizeof(*C),XNET_FAMILY_UNSPEC,250,8,1024};return true;
}
static bool xrpcTlsDialCreate(xnetengine* E,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Name,uint16 Port,uint64 Affinity,
    const xtlsclientconfig* Client,const xrpctlsconfig* Config,const xrpctlsdialconfig* Dial,xrpctls** Out,double Entered)
{
    if(!E || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Dial && (!xrtMemRangeValid(Dial,sizeof(*Dial)) ||
        xrtMemRangesOverlap(Dial,sizeof(*Dial),Out,sizeof(*Out)))))return xrpcTlsDialArgument("invalid TLS dial arguments/output overlap");
    xrpctlsdialconfig K={0};if(Dial)K=*Dial;else if(!xrpcTlsDialConfigInit(&K))return false;
    if(K.Size!=sizeof(K) || (K.Family!=XNET_FAMILY_UNSPEC && K.Family!=XNET_FAMILY_IPV4 && K.Family!=XNET_FAMILY_IPV6) ||
        K.FallbackDelay<0 || !K.MaxAttempts || K.MaxAttempts>64 || !K.MaxHostBytes || K.MaxHostBytes>65535)return xrpcTlsDialArgument("invalid TLS dial policy/bounds");
    if(Addresses){
        size_t Count=xrtNetAddrListCount(Addresses),Eligible=0;if(!Count || Count>64)return xrpcTlsDialArgument("prepared TLS endpoints exceed count limit");
        for(size_t I=0;I<Count;++I){const xnetaddr* A=xrtNetAddrListGet(Addresses,I);if(!A)return false;
            if(xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)))return xrpcTlsDialArgument("endpoint/output overlap");
            if(K.Family!=XNET_FAMILY_UNSPEC && K.Family!=A->Family)continue;
            if(!A->Port)return xrpcTlsDialArgument("prepared TLS endpoint port is zero");
            ++Eligible;}
        if(!Eligible)return xrpcTlsDialArgument("no TLS endpoint matches family");
    }else if(!Resolver || !Port || !Name.Size || Name.Size>K.MaxHostBytes || !xrtMemRangeValid(Name.Data,Name.Size) ||
        xrtMemRangesOverlap(Name.Data,Name.Size,Out,sizeof(*Out)) || memchr(Name.Data,0,Name.Size))return xrpcTlsDialArgument("invalid TLS DNS name/resolver/port");
    xrpctlsdialdata* D=xrtCalloc(1,sizeof(*D)+Name.Size+1);if(!D)return false;
    xrtAtomic32Init(&D->References,1);D->Plan=(xrpctlsconnect){.Data=D,.Start=xrpcTlsDialStart,.Cancel=xrpcTlsDialCancel,.Drop=xrpcTlsDialPlanDrop};
    D->Resolver=Resolver;D->Port=Port;xrtNetDialConfigInit(&D->Config);D->Config.Family=K.Family;D->Config.Timeout=0;
    D->Config.FallbackDelay=K.FallbackDelay;D->Config.MaxAttempts=K.MaxAttempts;
    if(Addresses){D->Addresses=xrtNetAddrListRef(Addresses);if(!D->Addresses){xrpcTlsDialDrop(D);return false;}}
    else memcpy(D->Name,Name.Data,Name.Size);
    if(xrpcTlsCreatePlanned(E,Affinity,Client,Config,&D->Plan,Out,Entered))return true;
    xerror* Error=xrtTakeError();xrpcTlsDialDrop(D);xrtClearError();xrtSetErrorTake(Error);return false;
}
XRPC_API bool xrpcTlsDial(xnetengine* E,xnetresolver* R,xstrview Name,uint16 Port,uint64 A,const xtlsclientconfig* Client,const xrpctlsconfig* C,const xrpctlsdialconfig* D,xrpctls** Out)
{double Entered=xrtTimer();return xrpcTlsDialCreate(E,R,NULL,Name,Port,A,Client,C,D,Out,Entered);}
XRPC_API bool xrpcTlsDialAddresses(xnetengine* E,xnetaddrlist* Addresses,uint64 A,const xtlsclientconfig* Client,const xrpctlsconfig* C,const xrpctlsdialconfig* D,xrpctls** Out)
{
    double Entered=xrtTimer();if(!Addresses)return xrpcTlsDialArgument("prepared TLS endpoints are NULL");
    return xrpcTlsDialCreate(E,NULL,Addresses,(xstrview){0},0,A,Client,C,D,Out,Entered);
}
#endif
