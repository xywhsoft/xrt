#include "support/ws_connect_peer.h"
#include <xmqtt/client_ws_uri.h>
static xmqttclient* mqttUriStateStart(mqttWsConnectPeer* P,unsigned Route,const xmqttclientconfig* Cfg,const xmqttclientwsconnectconfig* Policy)
{
    int A=snprintf(P->UriAuthority,sizeof(P->UriAuthority),"%s:%u",Route==1?"example.com":Route==2?"%31%32%37.0.0.1":"127.0.0.1",(unsigned)P->Base.Address.Port);testRequire(A>0&&(size_t)A<sizeof(P->UriAuthority),"state URI authority");P->ExpectedHost=(xstrview){P->UriAuthority,(size_t)A};char URI[256];int N=snprintf(URI,sizeof(URI),"%s://%s%.*s",P->Base.TLS?"wss":"ws",P->UriAuthority,(int)Policy->Target.Size,Policy->Target.Data);testRequire(N>0&&(size_t)N<sizeof(URI),"state URI resource");xmqttclient* C=NULL;bool OK;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    if(P->Base.TLS){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P->Base.Context;T.Session.Verifier=P->Base.Verifier;T.Session.ResumeLimit=0;OK=xmqttClientWssConnectUri(P->Base.Engine,P->Resolver,(xstrview){URI,(size_t)N},1,Cfg,Policy,&T,&P->Base.Connect,&C);}else
#endif
    OK=xmqttClientWsConnectUri(P->Base.Engine,P->Resolver,(xstrview){URI,(size_t)N},1,Cfg,Policy,&P->Base.Connect,&C);
    testRequire(OK&&C,"URI ordinary Client with selected Auth/Session/Inbox");return C;
}
#define wsConnectStart mqttUriStateStart
#include "test_client_ws_connect_state.c"
#undef wsConnectStart
