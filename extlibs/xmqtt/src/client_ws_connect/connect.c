#include <xmqtt/client_ws_connect.h>
#include "../internal/xmqtt_client_ws_connect_internal.h"
#include <xrt/memory.h>
#include <xrt/time.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT)
static bool xmqttWsConnectError(cstr Message)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt.ws",1,Message);return false;}
XMQTT_API bool xmqttClientWsConnectConfigInit(xmqttclientwsconnectconfig* K)
{
    if(!K||!xrtMemRangeValid(K,sizeof(*K)))return xmqttWsConnectError("invalid WS connect config output");
    *K=(xmqttclientwsconnectconfig){.Size=sizeof(*K)};xmqttClientWsConfigInit(&K->WebSocket);xmqttClientDialConfigInit(&K->Dial);xrtHttp1LimitsInit(&K->Head);K->Head.MaxHead=XMQTT_WS_CONNECT_HEAD_MAX;K->Head.MaxFields=XMQTT_WS_CONNECT_FIELDS_MAX;K->MaxWriteBytes=XMQTT_WS_CONNECT_WRITE_MAX;K->MaxInformationalResponses=4;K->Target=XRT_STR_LITERAL("/mqtt");return true;
}
static bool xmqttWsHeaderName(xstrview V,cstr Name)
{
    size_t N=strlen(Name);if(V.Size!=N)return false;
    for(size_t I=0;I<N;++I){unsigned char A=(unsigned char)V.Data[I],B=(unsigned char)Name[I];if(A>='A'&&A<='Z')A+=32;if(B>='A'&&B<='Z')B+=32;if(A!=B)return false;}return true;
}
static bool xmqttWsManaged(xstrview V)
{
    static const cstr Names[]={"Host","Connection","Upgrade","Sec-WebSocket-Key","Sec-WebSocket-Version","Sec-WebSocket-Protocol","Sec-WebSocket-Extensions","Sec-WebSocket-Accept","Content-Length","Transfer-Encoding","Origin"};
    for(size_t I=0;I<sizeof(Names)/sizeof(*Names);++I)if(xmqttWsHeaderName(V,Names[I]))return true;
    return false;
}
XMQTT_INTERNAL void xmqttInternalWsHttpDrop(xmqttwshttp* H)
{
    if(!H)return;
    xrtSecureZero(H->Cache,H->Cache?H->Limits.MaxHead:0);xrtFree(H->Cache);xrtSecureZero(H->Cold,H->ColdSize);xrtFree(H->Cold);xrtSecureZero(H,sizeof(*H));xrtFree(H);
}
XMQTT_INTERNAL xmqttwshttp* xmqttInternalWsHttpCreate(const xmqttclientwsconnectconfig* K,xmqttclient** Out)
{
    if(!K||!Out||!xrtMemRangeValid(Out,sizeof(*Out))||!xrtMemRangeValid(K,sizeof(*K))||xrtMemRangesOverlap(K,sizeof(*K),Out,sizeof(*Out))||K->Size!=sizeof(*K)||!K->Head.MaxHead||K->Head.MaxHead>XMQTT_WS_CONNECT_HEAD_MAX||!K->Head.MaxStartLine||K->Head.MaxStartLine>K->Head.MaxHead||!K->Head.MaxFieldLine||K->Head.MaxFieldLine>K->Head.MaxHead||!K->Head.MaxFields||K->Head.MaxFields>XMQTT_WS_CONNECT_FIELDS_MAX||!K->MaxWriteBytes||K->MaxWriteBytes>XMQTT_WS_CONNECT_WRITE_MAX||K->MaxInformationalResponses>16||!K->Host.Size||K->Host.Size>1024||!K->Target.Size||K->Target.Size>4096||K->Origin.Size>2048||K->FieldCount>XMQTT_WS_CONNECT_EXTRA_MAX)goto Invalid;
    xstrview Views[]={K->Host,K->Target,K->Origin};
    for(size_t I=0;I<sizeof(Views)/sizeof(*Views);++I)if(!xrtMemRangeValid(Views[I].Data,Views[I].Size)||xrtMemRangesOverlap(Views[I].Data,Views[I].Size,Out,sizeof(*Out)))goto Invalid;
    if(!xrtHttpHostValid(K->Host)||K->Target.Data[0]!='/'||memchr(K->Target.Data,'#',K->Target.Size)||!xrtHttpFieldValueValid(K->Origin)||!xrtMemRangeValid(K->Fields,K->FieldCount*sizeof(xhttpfield))||xrtMemRangesOverlap(K->Fields,K->FieldCount*sizeof(xhttpfield),Out,sizeof(*Out)))goto Invalid;
    size_t Extras=0;unsigned Authorization=0;
    for(size_t I=0;I<K->FieldCount;++I){xhttpfield F=K->Fields[I];if(!F.Name.Size||F.Name.Size>256||F.Value.Size>4096||!xrtMemRangeValid(F.Name.Data,F.Name.Size)||!xrtMemRangeValid(F.Value.Data,F.Value.Size)||xrtMemRangesOverlap(F.Name.Data,F.Name.Size,Out,sizeof(*Out))||xrtMemRangesOverlap(F.Value.Data,F.Value.Size,Out,sizeof(*Out))||xmqttWsManaged(F.Name)||(xmqttWsHeaderName(F.Name,"Authorization")&&++Authorization>1))goto Invalid;Extras+=F.Name.Size+F.Value.Size;if(Extras>16384)goto Invalid;}
    xmqttwshttp* H=xrtCalloc(1,sizeof(*H));if(!H)return NULL;H->Info.Size=sizeof(H->Info);H->Limits=K->Head;H->MaxInformational=K->MaxInformationalResponses;
    if(!xrtWsKeyGenerate(H->Key,sizeof(H->Key)))goto Failed;
    xhttpfield F[XWS_UPGRADE_REQUEST_FIELDS_MAX+XMQTT_WS_CONNECT_EXTRA_MAX+1];size_t Count=0;
    if(!xrtWsUpgradeRequestFields(K->Host,(xstrview){H->Key,XWS_KEY_SIZE},XRT_STR_LITERAL("mqtt"),(xstrview){0},F,sizeof(F)/sizeof(*F),&Count))goto Failed;
    if(K->Origin.Size)F[Count++]=(xhttpfield){XRT_STR_LITERAL("Origin"),K->Origin};
    for(size_t I=0;I<K->FieldCount;++I)F[Count++]=K->Fields[I];
    if(!xrtHttp1RequestWrite(XRT_STR_LITERAL("GET"),K->Target,XHTTP_VERSION_1_1,F,Count,NULL,0,&H->WireBytes))goto Failed;
    if(H->WireBytes>K->MaxWriteBytes){xmqttWsConnectError("HTTP Upgrade request exceeds configured wire cap");goto Failed;}
    H->ColdSize=K->Head.MaxFields*sizeof(xhttpfield)+H->WireBytes;H->Cold=xrtMalloc(H->ColdSize);if(!H->Cold)goto Failed;H->Fields=(xhttpfield*)H->Cold;H->Wire=H->Cold+K->Head.MaxFields*sizeof(xhttpfield);
    size_t Written=0;if(!xrtHttp1RequestWrite(XRT_STR_LITERAL("GET"),K->Target,XHTTP_VERSION_1_1,F,Count,H->Wire,H->WireBytes,&Written))goto Failed;
    H->Info.ColdBytes=H->Info.PeakColdBytes=sizeof(*H)+H->ColdSize;return H;
Failed:
    xmqttInternalWsHttpDrop(H);return NULL;
Invalid:
    xmqttWsConnectError("invalid WS connect HTTP policy, ranges or output overlap");return NULL;
}
XMQTT_API bool xmqttClientWsConnect(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttpacket* Q,xmqttclient** O)
{return xmqttInternalClientWsConnect(E,NULL,NULL,R,(xstrview){0},0,A,C,W,NULL,Q,O,xrtTimer(),NULL,NULL);}
XMQTT_API bool xmqttClientWsDial(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttpacket* Q,xmqttclient** O)
{return xmqttInternalClientWsConnect(E,R,NULL,NULL,H,P,A,C,W,NULL,Q,O,xrtTimer(),NULL,NULL);}
XMQTT_API bool xmqttClientWsDialAddresses(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttpacket* Q,xmqttclient** O)
{return xmqttInternalClientWsConnect(E,NULL,L,NULL,(xstrview){0},0,A,C,W,NULL,Q,O,xrtTimer(),NULL,NULL);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
XMQTT_API bool xmqttClientWssConnect(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{double At=xrtTimer();if(!T)return xmqttWsConnectError("WSS requires explicit TLS policy");return xmqttInternalClientWsConnect(E,NULL,NULL,R,(xstrview){0},0,A,C,W,T,Q,O,At,NULL,NULL);}
XMQTT_API bool xmqttClientWssDial(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{double At=xrtTimer();if(!T)return xmqttWsConnectError("WSS requires explicit TLS policy");return xmqttInternalClientWsConnect(E,R,NULL,NULL,H,P,A,C,W,T,Q,O,At,NULL,NULL);}
XMQTT_API bool xmqttClientWssDialAddresses(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclientwsconnectconfig* W,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{double At=xrtTimer();if(!T)return xmqttWsConnectError("WSS requires explicit TLS policy");return xmqttInternalClientWsConnect(E,NULL,L,NULL,(xstrview){0},0,A,C,W,T,Q,O,At,NULL,NULL);}
#endif
#endif
