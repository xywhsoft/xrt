#include <xrpc/tcp_dial.h>
#include "../internal/xrpc_tcp_connect.h"
#include <xrt/atomic.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_TCP_DIAL)
typedef struct xrpctcpdialdata {
    xatomic32 References;
    xrpctcpconnect Plan;
    xrpctcp* Peer;
    xnetresolver* Resolver;
    xnetaddrlist* Addresses;
    xnetdial* Dial;
    xnetdialconfig Config;
    uint16 Port;
    char Name[];
} xrpctcpdialdata;
static bool xrpcTcpDialArgument(cstr Message)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.rpc",19201,Message);return false;}
static void xrpcTcpDialDrop(const void* Data)
{
    xrpctcpdialdata* D=(xrpctcpdialdata*)Data;
    if(xrtAtomic32FetchSub(&D->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtNetDialDestroy(D->Dial);xrtNetAddrListDestroy(D->Addresses);xrtFree(D);
}
static void xrpcTcpDialPlanDrop(ptr Data){xrpcTcpDialDrop(Data);}
static bool xrpcTcpDialCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpctcpdialdata*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcTcpDialTrace(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xrpcTcpDialOps={xrpcTcpDialCount,xrpcTcpDialTrace};
static void xrpcTcpDialDone(xnetdial* Dial,xnetresult Result,xnetstream* Stream,const xerror* Cause,ptr Data)
{(void)Dial;xrpcTcpConnected(((xrpctcpdialdata*)Data)->Peer,Result,Stream,Cause);}
static void xrpcTcpDialRetired(const void* Data)
{
    xrpctcpdialdata* D=(xrpctcpdialdata*)Data;
    /* Posted peer work cannot run before this owned Drop returns on its Worker.
     * Both creator and callback hold real Data references, not Count guesses. */
    xrpcTcpConnectRetired(D->Peer);xrpcTcpDialDrop(D);
}
static const xnetdialownershipv1 xrpcTcpDialPolicy={sizeof(xnetdialownershipv1),xrpcTcpDialDone,xrpcTcpDialRetired,&xrpcTcpDialOps};
static bool xrpcTcpDialStart(ptr Data,xrpctcp* Peer,xnetengine* E,uint64 Affinity,const xnetstreamconfig* Stream)
{
    xrpctcpdialdata* D=Data;D->Peer=Peer;D->Config.Stream=*Stream;D->Config.Affinity=Affinity;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED);
    /* No candidate may publish facade Open/Read/Close. Only Done's transferred
     * winner receives the permanent owned binding from the peer. */
    D->Dial=D->Addresses?xrtNetDialAddressesOwnedV1(E,D->Addresses,&D->Config,NULL,NULL,D,&xrpcTcpDialPolicy):
        xrtNetDialOwnedV1(E,D->Resolver,D->Name,D->Port,&D->Config,NULL,NULL,D,&xrpcTcpDialPolicy);
    if(!D->Dial){xrpcTcpDialDrop(D);return false;}
    xrtNetAddrListDestroy(D->Addresses);D->Addresses=NULL;return true;
}
static void xrpcTcpDialCancel(ptr Data)
{(void)xrtNetDialCancel(((xrpctcpdialdata*)Data)->Dial);}
XRPC_API bool xrpcTcpDialConfigInit(xrpctcpdialconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcTcpDialArgument("invalid TCP dial config output");
    *C=(xrpctcpdialconfig){sizeof(*C),XNET_FAMILY_UNSPEC,250,8,1024};return true;
}
static bool xrpcTcpDialCreate(xnetengine* E,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Name,uint16 Port,uint64 Affinity,
    const xrpctcpconfig* Config,const xrpctcpdialconfig* Dial,xrpctcp** Out,double Entered)
{
    if(!E || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Dial && (!xrtMemRangeValid(Dial,sizeof(*Dial)) ||
        xrtMemRangesOverlap(Dial,sizeof(*Dial),Out,sizeof(*Out)))))return xrpcTcpDialArgument("invalid TCP dial arguments/output overlap");
    xrpctcpdialconfig K={0};if(Dial)K=*Dial;else if(!xrpcTcpDialConfigInit(&K))return false;
    if(K.Size!=sizeof(K) || (K.Family!=XNET_FAMILY_UNSPEC && K.Family!=XNET_FAMILY_IPV4 && K.Family!=XNET_FAMILY_IPV6) ||
        K.FallbackDelay<0 || !K.MaxAttempts || K.MaxAttempts>64 || !K.MaxHostBytes || K.MaxHostBytes>65535)return xrpcTcpDialArgument("invalid TCP dial policy/bounds");
    if(Addresses){
        size_t Count=xrtNetAddrListCount(Addresses),Eligible=0;if(!Count || Count>64)return xrpcTcpDialArgument("prepared TCP endpoints exceed count limit");
        for(size_t I=0;I<Count;++I){const xnetaddr* A=xrtNetAddrListGet(Addresses,I);if(!A)return false;
            if(xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)))return xrpcTcpDialArgument("endpoint/output overlap");
            if(K.Family!=XNET_FAMILY_UNSPEC && K.Family!=A->Family)continue;
            if(!A->Port)return xrpcTcpDialArgument("prepared TCP endpoint port is zero");
            ++Eligible;}
        if(!Eligible)return xrpcTcpDialArgument("no TCP endpoint matches family");
    }else if(!Resolver || !Port || !Name.Size || Name.Size>K.MaxHostBytes || !xrtMemRangeValid(Name.Data,Name.Size) ||
        xrtMemRangesOverlap(Name.Data,Name.Size,Out,sizeof(*Out)) || memchr(Name.Data,0,Name.Size))return xrpcTcpDialArgument("invalid TCP DNS name/resolver/port");
    xrpctcpdialdata* D=xrtCalloc(1,sizeof(*D)+Name.Size+1);if(!D)return false;
    xrtAtomic32Init(&D->References,1);D->Plan=(xrpctcpconnect){.Data=D,.Start=xrpcTcpDialStart,.Cancel=xrpcTcpDialCancel,.Drop=xrpcTcpDialPlanDrop};
    D->Resolver=Resolver;D->Port=Port;xrtNetDialConfigInit(&D->Config);D->Config.Family=K.Family;
    D->Config.Timeout=0; /* One peer-owned original deadline, never restarted. */
    D->Config.FallbackDelay=K.FallbackDelay;D->Config.MaxAttempts=K.MaxAttempts;
    if(Addresses){D->Addresses=xrtNetAddrListRef(Addresses);if(!D->Addresses){xrpcTcpDialDrop(D);return false;}}
    else memcpy(D->Name,Name.Data,Name.Size);
    if(xrpcTcpCreatePlanned(E,Affinity,Config,&D->Plan,Out,Entered))return true;
    xerror* Error=xrtTakeError();xrpcTcpDialDrop(D);xrtClearError();xrtSetErrorTake(Error);return false;
}
XRPC_API bool xrpcTcpDial(xnetengine* E,xnetresolver* R,xstrview Name,uint16 Port,uint64 A,const xrpctcpconfig* C,const xrpctcpdialconfig* D,xrpctcp** Out)
{return xrpcTcpDialCreate(E,R,NULL,Name,Port,A,C,D,Out,xrtTimer());}
XRPC_API bool xrpcTcpDialAddresses(xnetengine* E,xnetaddrlist* Addresses,uint64 A,const xrpctcpconfig* C,const xrpctcpdialconfig* D,xrpctcp** Out)
{
    double Entered=xrtTimer();if(!Addresses)return xrpcTcpDialArgument("prepared TCP endpoints are NULL");
    return xrpcTcpDialCreate(E,NULL,Addresses,(xstrview){0},0,A,C,D,Out,Entered);
}
#endif
