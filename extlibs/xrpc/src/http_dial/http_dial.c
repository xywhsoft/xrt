#include <xrpc/http_dial.h>
#include "../internal/xrpc_http_connect.h"
#include "../internal/xrpc_http_tls.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <math.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_DIAL)
typedef struct xrpchttpdialdata {
    xatomic32 References;
    xrpchttpconnectplan Plan;
    xrpchttpclient* Client; /* Borrowed through the client's runtime activation. */
    xnetresolver* Resolver;
    xnetaddrlist* Addresses;
    xnetdial* Dial;
    xnetdialconfig Config;
    uint16 Port;
    char Name[];
} xrpchttpdialdata;
static bool xrpcDialArgument(cstr Message)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.rpc",XRPC_HTTP_ARGUMENT,Message);return false;}
static void xrpcDialDrop(const void* Data)
{
    xrpchttpdialdata* D=(xrpchttpdialdata*)Data;
    if(xrtAtomic32FetchSub(&D->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtNetDialDestroy(D->Dial);xrtNetAddrListDestroy(D->Addresses);xrtFree(D);
}
static void xrpcDialPlanDrop(ptr Data){xrpcDialDrop(Data);}
static bool xrpcDialCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpchttpdialdata*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcDialOpaque(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xrpcDialOps={xrpcDialCount,xrpcDialOpaque};
static void xrpcDialDone(xnetdial* Dial,xnetresult Result,xnetstream* Stream,const xerror* Cause,ptr Data)
{(void)Dial;xrpchttpdialdata* D=Data;xrpcHttpConnected(D->Client,Result,Stream,Cause);}
static void xrpcDialRetired(const void* Data)
{
    xrpchttpdialdata* D=(xrpchttpdialdata*)Data;
    /* The queued client turn cannot run until this owned callback returns on
     * the same worker. The client's runtime activation owns its plan until it
     * observes this retirement. Both plan and callback own real Data credits. */
    xrpcHttpConnectRetired(D->Client);xrpcDialDrop(D);
}
static const xnetdialownershipv1 xrpcDialPolicy={sizeof(xnetdialownershipv1),xrpcDialDone,xrpcDialRetired,&xrpcDialOps};
static bool xrpcDialStart(ptr Data,xrpchttpclient* C,xnetengine* Engine,uint64 Affinity,const xnetstreamconfig* Stream,const xnetstreamevents* Events)
{
    xrpchttpdialdata* D=Data;D->Client=C;D->Config.Stream=*Stream;D->Config.Affinity=Affinity;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED);
    D->Dial=D->Addresses?xrtNetDialAddressesOwnedV1(Engine,D->Addresses,&D->Config,Events,C,D,&xrpcDialPolicy):
        xrtNetDialOwnedV1(Engine,D->Resolver,D->Name,D->Port,&D->Config,Events,C,D,&xrpcDialPolicy);
    if(!D->Dial){xrpcDialDrop(D);return false;}
    xrtNetAddrListDestroy(D->Addresses);D->Addresses=NULL;return true;
}
static void xrpcDialCancel(ptr Data)
{xrpchttpdialdata* D=Data;(void)xrtNetDialCancel(D->Dial);}
XRPC_API bool xrpcHttpDialConfigInit(xrpchttpdialconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcDialArgument("invalid HTTP dial config output");
    *C=(xrpchttpdialconfig){sizeof(*C),XNET_FAMILY_UNSPEC,30000,250,8,1024};return true;
}
bool xrpcHttpDialCreate(xnetengine* Engine,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Name,uint16 Port,uint64 Affinity,
    xstrview Host,xstrview Target,const xrpchttpconfig* Config,const xrpchttpdialconfig* Dial,xrpchttpclient** Result,double Began,xrpchttpsecure* Secure)
{
    if(!Engine || !Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Dial && (!xrtMemRangeValid(Dial,sizeof(*Dial)) ||
        xrtMemRangesOverlap(Dial,sizeof(*Dial),Result,sizeof(*Result)))))return xrpcDialArgument("invalid HTTP dial arguments/output overlap");
    xrpchttpdialconfig K;if(Dial)K=*Dial;else xrpcHttpDialConfigInit(&K);
    if(K.Size!=sizeof(K) || (K.Family!=XNET_FAMILY_UNSPEC && K.Family!=XNET_FAMILY_IPV4 && K.Family!=XNET_FAMILY_IPV6) || K.Timeout<XRT_WAIT_FOREVER ||
        K.FallbackDelay<0 || !K.MaxAttempts || K.MaxAttempts>64 || !K.MaxHostBytes || K.MaxHostBytes>65535u)return xrpcDialArgument("invalid HTTP dial policy/bounds");
    if(Addresses){
        size_t Count=xrtNetAddrListCount(Addresses),Eligible=0;if(!Count || Count>64u)return xrpcDialArgument("prepared HTTP endpoints exceed count limit");
        for(size_t N=0;N<Count;++N){const xnetaddr* A=xrtNetAddrListGet(Addresses,N);if(!A)return false;
            if(xrtMemRangesOverlap(A,sizeof(*A),Result,sizeof(*Result)))return xrpcDialArgument("endpoint/output overlap");
            if(K.Family!=XNET_FAMILY_UNSPEC && K.Family!=A->Family)continue;
            if(!A->Port)return xrpcDialArgument("prepared HTTP endpoint port is zero");
            ++Eligible;}
        if(!Eligible)return xrpcDialArgument("no HTTP endpoint matches family");
    }else if(!Resolver || !Port || !Name.Size || Name.Size>K.MaxHostBytes || !xrtMemRangeValid(Name.Data,Name.Size) ||
        xrtMemRangesOverlap(Name.Data,Name.Size,Result,sizeof(*Result)) || memchr(Name.Data,0,Name.Size))return xrpcDialArgument("invalid HTTP DNS name/resolver/port");
    xrpchttpdialdata* D=xrtCalloc(1,sizeof(*D)+Name.Size+1u);if(!D)return false;
    xrtAtomic32Init(&D->References,1);D->Plan=(xrpchttpconnectplan){.Data=D,.Start=xrpcDialStart,.Cancel=xrpcDialCancel,.Drop=xrpcDialPlanDrop};
    D->Resolver=Resolver;D->Port=Port;xrtNetDialConfigInit(&D->Config);D->Config.Family=K.Family;D->Config.Timeout=0;
    D->Config.FallbackDelay=K.FallbackDelay;D->Config.MaxAttempts=K.MaxAttempts;
    if(Addresses){D->Addresses=xrtNetAddrListRef(Addresses);if(!D->Addresses){xrpcDialDrop(D);return false;}}
    else memcpy(D->Name,Name.Data,Name.Size);
    double Deadline=K.Timeout==XRT_WAIT_FOREVER?INFINITY:K.Timeout==0?Began:nextafter(Began+(double)(K.Timeout/1000)+(double)(K.Timeout%1000)*.001,INFINITY);
    if(Secure?xrpcHttpCreateSecurePlanned(Engine,Affinity,Host,Target,Config,&D->Plan,Deadline,Secure,Result):
        xrpcHttpCreatePlanned(Engine,Affinity,Host,Target,Config,&D->Plan,Deadline,Result))return true;
    xerror* E=xrtTakeError();xrpcDialDrop(D);xrtClearError();xrtSetErrorTake(E);return false;
}
XRPC_API bool xrpcHttpDial(xnetengine* E,xnetresolver* R,xstrview Name,uint16 Port,uint64 A,xstrview H,xstrview T,const xrpchttpconfig* C,const xrpchttpdialconfig* D,xrpchttpclient** Result)
{return xrpcHttpDialCreate(E,R,NULL,Name,Port,A,H,T,C,D,Result,xrtTimer(),NULL);}
XRPC_API bool xrpcHttpDialAddresses(xnetengine* E,xnetaddrlist* Addresses,uint64 A,xstrview H,xstrview T,const xrpchttpconfig* C,const xrpchttpdialconfig* D,xrpchttpclient** Result)
{
    double Began=xrtTimer();if(!Addresses)return xrpcDialArgument("prepared HTTP endpoints are NULL");
    return xrpcHttpDialCreate(E,NULL,Addresses,(xstrview){0},0,A,H,T,C,D,Result,Began,NULL);
}
#endif
