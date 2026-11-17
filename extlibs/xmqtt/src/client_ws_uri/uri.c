#include <xmqtt/client_ws_uri.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
#include <xmqtt/client_store_ws.h>
#endif
#include <xrt/memory.h>
#include <xrt/error.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI)
static bool xmqttWsUriSetupError(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.mqtt.ws.uri",2,Message);return false;}
#include "../internal/xmqtt_client_ws_connect_internal.h"
#include <xrt/time.h>
#include <xrt/charset.h>
static int xmqttWsUriHex(unsigned char C)
{return C>='0'&&C<='9'?C-'0':C>='a'&&C<='f'?C-'a'+10:C>='A'&&C<='F'?C-'A'+10:-1;}
static bool xmqttWsUriCreate(xnetengine* E,xnetresolver* R,xstrview URI,uint64 Affinity,const xmqttclientconfig* Config,
    const xmqttclientwsconnectconfig* WebSocket,const void* TLS,bool Secure,const xmqttpacket* Connect,xmqttclient** Out,double Entered,struct xmqttsession* Session,const struct xmqttclientstoreconfig* Store)
{
    if(!Out||!xrtMemRangeValid(Out,sizeof(*Out))||!xrtMemRangeValid(URI.Data,URI.Size)||xrtMemRangesOverlap(URI.Data,URI.Size,Out,sizeof(*Out)))return xmqttWsUriSetupError(XERR_ARGUMENT,"invalid URI/output range or overlap");
    xmqttwsuri U={.Size=sizeof(U)};if(!xmqttWsUriParse(URI,&U))return false;
    if(U.Secure!=Secure)return xmqttWsUriSetupError(XERR_ARGUMENT,"URI scheme does not match explicit plain/secure factory");
    if(U.HostKind==XMQTT_WS_URI_IPVFUTURE)return xmqttWsUriSetupError(XERR_UNSUPPORTED,"IPvFuture routing is unsupported");
    char Decoded[XMQTT_WS_URI_MAX_AUTHORITY];xstrview Route=U.Host;
    if(memchr(Route.Data,'%',Route.Size)){
        size_t N=0;for(size_t I=0;I<Route.Size;++I){unsigned char C=(unsigned char)Route.Data[I];if(C=='%'){C=(unsigned char)(xmqttWsUriHex((unsigned char)Route.Data[I+1])*16+xmqttWsUriHex((unsigned char)Route.Data[I+2]));I+=2;}
            if(C<=32||C==127||C=='/'||C=='\\'||C==':'||C=='?'||C=='#'||C=='@'||C=='['||C==']')return xmqttWsUriSetupError(XERR_ARGUMENT,"decoded URI host contains forbidden routing byte");
            Decoded[N++]=(char)C;}
        Route=(xstrview){Decoded,N};if(!xrtUtf8Valid(Route,NULL))return xmqttWsUriSetupError(XERR_ARGUMENT,"decoded URI host is not UTF-8");}
    bool Numeric=U.HostKind==XMQTT_WS_URI_IPV4||U.HostKind==XMQTT_WS_URI_IPV6||xrtHttpIpv4Valid(Route);xnetaddr Address;
    if(Numeric){char IP[64];if(Route.Size>=sizeof(IP))return xmqttWsUriSetupError(XERR_ARGUMENT,"URI IP literal exceeds bound");memcpy(IP,Route.Data,Route.Size);IP[Route.Size]=0;if(!xrtNetAddrParse(&Address,IP,U.Port))return false;}
    xmqttclientwsconnectconfig W;if(WebSocket){if(!xrtMemRangeValid(WebSocket,sizeof(*WebSocket))||WebSocket->Size!=sizeof(*WebSocket)||xrtMemRangesOverlap(WebSocket,sizeof(*WebSocket),Out,sizeof(*Out)))return xmqttWsUriSetupError(XERR_ARGUMENT,"invalid URI WebSocket policy/output");W=*WebSocket;}else xmqttClientWsConnectConfigInit(&W);
    char Resource[XMQTT_WS_URI_MAX_TARGET];W.Host=U.Authority;
    if(!U.Path.Size&&U.Query.Size){Resource[0]='/';Resource[1]='?';memcpy(Resource+2,U.Query.Data,U.Query.Size);W.Target=(xstrview){Resource,U.TargetBytes};}else W.Target=U.Path.Size?(xstrview){U.Path.Data,U.TargetBytes}:XRT_STR_LITERAL("/");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    xmqttclienttlsconfig T;
    if(Secure){if(!TLS||!xrtMemRangeValid(TLS,sizeof(T))||xrtMemRangesOverlap(TLS,sizeof(T),Out,sizeof(*Out)))return xmqttWsUriSetupError(XERR_ARGUMENT,"WSS URI requires explicit TLS trust policy");T=*(const xmqttclienttlsconfig*)TLS;
        if(T.ServerNameFromHost){if(!T.Session.VerifyName.Size)T.Session.VerifyName=Route;if(!T.Session.ServerName.Size&&!Numeric)T.Session.ServerName=Route;}
        TLS=&T;}
#endif
    return xmqttInternalClientWsConnect(E,Numeric?NULL:R,NULL,Numeric?&Address:NULL,Numeric?(xstrview){0}:Route,Numeric?0:U.Port,Affinity,Config,&W,TLS,Connect,Out,Entered,Session,Store);
}
XMQTT_API bool xmqttClientWsConnectUri(xnetengine* E,xnetresolver* R,xstrview URI,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttpacket* Q,xmqttclient** O)
{return xmqttWsUriCreate(E,R,URI,A,C,W,NULL,false,Q,O,xrtTimer(),NULL,NULL);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
XMQTT_API bool xmqttClientWssConnectUri(xnetengine* E,xnetresolver* R,xstrview URI,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{return xmqttWsUriCreate(E,R,URI,A,C,W,T,true,Q,O,xrtTimer(),NULL,NULL);}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
XMQTT_API bool xmqttClientWsConnectUriStore(xnetengine* E,xnetresolver* R,xstrview URI,uint64 A,const xmqttclientconfig* C,
    const xmqttclientwsconnectconfig* W,xmqttsession* Session,const xmqttclientstoreconfig* Store,
    const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();
    if(!Session||!Store)return xmqttWsUriSetupError(XERR_ARGUMENT,"durable WS requires Session and Store");
    return xmqttWsUriCreate(E,R,URI,A,C,W,NULL,false,Q,O,At,Session,Store);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
XMQTT_API bool xmqttClientWssConnectUriStore(xnetengine* E,xnetresolver* R,xstrview URI,uint64 A,const xmqttclientconfig* C,
    const xmqttclientwsconnectconfig* W,const xmqttclienttlsconfig* T,xmqttsession* Session,const xmqttclientstoreconfig* Store,
    const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();
    if(!Session||!Store)return xmqttWsUriSetupError(XERR_ARGUMENT,"durable WSS requires Session and Store");
    return xmqttWsUriCreate(E,R,URI,A,C,W,T,true,Q,O,At,Session,Store);
}
#endif
#endif
#endif
