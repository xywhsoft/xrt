#include <xmqtt/client_ws_uri.h>
#include <xrt/memory.h>
#include <xrt/error.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_WS_URI_PARSE)
static bool xmqttWsUriError(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.mqtt.ws.uri",1,Message);return false;}
XMQTT_API bool xmqttWsUriParse(xstrview Input,xmqttwsuri* Out)
{
    if(!Out||!xrtMemRangeValid(Out,sizeof(*Out))||Out->Size!=sizeof(*Out)||!xrtMemRangeValid(Input.Data,Input.Size)||xrtMemRangesOverlap(Input.Data,Input.Size,Out,sizeof(*Out)))return xmqttWsUriError(XERR_ARGUMENT,"invalid URI view/output/Size/overlap");
    if(Input.Size>XMQTT_WS_URI_MAX_BYTES)return xmqttWsUriError(XERR_RANGE,"WS URI exceeds byte limit");
    xhttptarget T={0};if(!xrtHttpTargetParse(XRT_STR_LITERAL("GET"),Input,&T))return false;
    bool Secure=xrtHttpTokenEqual(T.Scheme,XRT_STR_LITERAL("wss"));
    if(T.Form!=XHTTP_TARGET_ABSOLUTE||!(T.Flags&XHTTP_TARGET_HAS_AUTHORITY)||(!Secure&&!xrtHttpTokenEqual(T.Scheme,XRT_STR_LITERAL("ws"))))return xmqttWsUriError(XERR_ARGUMENT,"absolute ws/wss URI with authority required");
    uint16 Port=0;if(!xrtHttpAuthorityPort(&T.Host,Secure?443:80,&Port)||!Port)return xmqttWsUriError(XERR_ARGUMENT,"WS URI requires a positive network port");
    size_t Target=(T.Path.Size?T.Path.Size:1)+(T.Query.Size?T.Query.Size+1:0);
    if(T.Authority.Size>XMQTT_WS_URI_MAX_AUTHORITY||Target>XMQTT_WS_URI_MAX_TARGET)return xmqttWsUriError(XERR_RANGE,"WS URI authority/resource exceeds bound");
    xmqttwsurihostkind Kind=XMQTT_WS_URI_NAME;
    if(T.Host.Flags&XHTTP_AUTHORITY_IP_LITERAL)Kind=xrtHttpIpv6Valid(T.Host.Host)?XMQTT_WS_URI_IPV6:XMQTT_WS_URI_IPVFUTURE;
    else if(xrtHttpIpv4Valid(T.Host.Host))Kind=XMQTT_WS_URI_IPV4;
    size_t Route=T.Host.Host.Size;for(size_t I=0;I<T.Host.Host.Size;++I)if(T.Host.Host.Data[I]=='%'){Route-=2;I+=2;}
    *Out=(xmqttwsuri){.Size=sizeof(*Out),.Secure=Secure,.HasQuery=(T.Flags&XHTTP_TARGET_HAS_QUERY)!=0,.Port=Port,.HostKind=Kind,.Authority=T.Authority,.Host=T.Host.Host,.Path=T.Path,.Query=T.Query,.RouteBytes=Route,.TargetBytes=Target};return true;
}
#endif
