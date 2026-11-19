#ifndef XMQTT_EXAMPLE_RECONNECT_WS_ENDPOINT_H
#define XMQTT_EXAMPLE_RECONNECT_WS_ENDPOINT_H
#include <xmqtt.h>
#include <stdio.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
#include <xrt/x509.h>
#endif
/* Application-owned immutable endpoint; keep its strings, Resolver and native
 * trust references alive through controller Destroy, not merely Create. */
typedef struct mqttWsEndpoint {
    xstrview URI;bool Secure;xnetresolver* Resolver;xmqttclientwsconnectconfig Policy;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    xmqttclienttlsconfig TLS;xx509store* Store;xtlsverifier* Verifier;
#endif
} mqttWsEndpoint;
static inline bool mqttWsEndpointInit(mqttWsEndpoint* E,xstrview URI,cstr CA)
{
    *E=(mqttWsEndpoint){0};xmqttwsuri Parsed={.Size=sizeof(Parsed)};if(!xmqttWsUriParse(URI,&Parsed))return false;E->URI=URI;E->Secure=Parsed.Secure;if(E->Secure!=(CA!=NULL))return false;xmqttClientWsConnectConfigInit(&E->Policy);E->Resolver=xrtNetResolverCreate(NULL);if(!E->Resolver)return false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    xmqttClientTlsConfigInit(&E->TLS);E->TLS.Session.ResumeLimit=0;
    if(E->Secure){if(!CA)return false;FILE* F=fopen(CA,"rb");if(!F)return false;unsigned char* Data=xrtMalloc(65537);if(!Data){fclose(F);return false;}size_t N=fread(Data,1,65537,F);bool OK=!ferror(F)&&N&&N<=65536;fclose(F);E->Store=xrtX509StoreCreate();OK=OK&&E->Store&&xrtX509StoreAddData(E->Store,(xbytesview){Data,N},NULL);xrtSecureZero(Data,N);xrtFree(Data);if(!OK)return false;xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Store=E->Store;xtlsverifylimits L;xrtTlsVerifyLimitsInit(&L);E->Verifier=xrtTlsVerifierCreateLimited(&V,&L);if(!E->Verifier)return false;E->TLS.Session.Verifier=E->Verifier;}
#else
    (void)CA;if(E->Secure)return false;
#endif
    return true;
}
static inline bool mqttWsEndpointClear(mqttWsEndpoint* E)
{
    if(E->Resolver&&!xrtNetResolverDestroy(E->Resolver)){return false;}E->Resolver=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    xrtTlsVerifierRelease(E->Verifier);xrtX509StoreFree(E->Store);E->Verifier=NULL;E->Store=NULL;
#endif
    return true;
}
static inline xmqttclientstatus mqttWsEndpointFactory(xnetengine* Engine,uint64 Affinity,const xmqttclientconfig* Config,
    xmqttsession* Session,const xmqttpacket* Connect,ptr Data,xmqttclient** Client)
{
    mqttWsEndpoint* E=Data;xmqttclientwsconnectconfig W=E->Policy;W.WebSocket.Session=Session;bool OK;
    xerror* Saved=xrtTakeError();
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    if(E->Secure)OK=xmqttClientWssConnectUri(Engine,E->Resolver,E->URI,Affinity,Config,&W,&E->TLS,Connect,Client);else
#endif
    OK=xmqttClientWsConnectUri(Engine,E->Resolver,E->URI,Affinity,Config,&W,Connect,Client);
    xerror* Fresh=xrtTakeError();xmqttclientstatus Status=OK?XMQTT_STATUS_OK:xrtErrorKind(Fresh)==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_INITIALIZATION;xrtErrorFree(Fresh);xrtSetErrorTake(Saved);return Status;
}
#endif
