#include "../tests/support/ws_connect_peer.h"
#include <xmqtt/client_ws_uri.h>
#include <inttypes.h>
static int mqttUriLatencyOrder(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y?1:0;}
int main(int Count,char** Args)
{
    unsigned N=Count>1?(unsigned)strtoul(Args[1],NULL,10):16;bool TLS=Count>2&&strtoul(Args[2],NULL,10)!=0;if(!N||N>256||Count>3)return 2;double* Times=malloc(N*sizeof(*Times));testRequire(Times!=NULL,"URI setup samples");uint64 Sum=0;double Start=xrtTimer(),Retire=0;size_t Peak=0;
    for(unsigned I=0;I<N;++I){mqttWsConnectPeer P;mqttWsConnectInit(&P,TLS,XMQTT_VERSION_5,XNET_PORT_SELECT);int A=snprintf(P.UriAuthority,sizeof(P.UriAuthority),"127.0.0.1:%u",(unsigned)P.Base.Address.Port);P.ExpectedHost=(xstrview){P.UriAuthority,(size_t)A};char URI[200];int NUri=snprintf(URI,sizeof(URI),"%s://%s/mqtt?x=%%00",TLS?"wss":"ws",P.UriAuthority);testRequire(A>0&&NUri>0&&(size_t)NUri<sizeof(URI),"bounded measured URI");xmqttclient* C=NULL;bool OK;double Entered=xrtTimer();
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        if(TLS){xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=P.Base.Context;T.Session.Verifier=P.Base.Verifier;T.Session.ResumeLimit=0;OK=xmqttClientWssConnectUri(P.Base.Engine,NULL,(xstrview){URI,(size_t)NUri},1,NULL,NULL,&T,&P.Base.Connect,&C);}else
#endif
        OK=xmqttClientWsConnectUri(P.Base.Engine,NULL,(xstrview){URI,(size_t)NUri},1,NULL,NULL,&P.Base.Connect,&C);
        testRequire(OK&&C,"measured URI factory");xfuture* Ready=xmqttClientReady(C);testRequire(((const xmqttreadyresult*)mqttWsFuture(Ready))->Status==XMQTT_STATUS_OK,"measured URI native HTTP and MQTT readiness");Times[I]=(xrtTimer()-Entered)*1000000.;xmqttclientwsconnectinfo Info={.Size=sizeof(Info)};testRequire(xmqttClientWsConnectInfo(C,&Info)&&Info.Upgraded&&Info.Status==101,"actual URI HTTP state");if(Info.PeakColdBytes>Peak)Peak=Info.PeakColdBytes;
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("ws/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* Sent=NULL;testRequire(xmqttClientPublishAsync(C,&Q,5000,&Sent)==XMQTT_ADMISSION_ACCEPTED,"measured URI traffic");const xmqttwriteresult* R=mqttWsFuture(Sent);testRequire(R->Status==XMQTT_STATUS_OK&&R->Identifier==1&&R->Puback,"measured URI PUBACK");Sum+=R->Identifier;Entered=xrtTimer();mqttWsCloseClient(C);mqttWsConnectStop(&P);Retire+=xrtTimer()-Entered;xrtFutureDestroy(Ready);xrtFutureDestroy(Sent);}
    double Elapsed=xrtTimer()-Start;testRequire(Sum==N,"URI setup checksum");qsort(Times,N,sizeof(*Times),mqttUriLatencyOrder);printf("cycles_per_sec: %.3f\nready_latency_p50_us: %.3f\nready_latency_p99_us: %.3f\nready_latency_max_us: %.3f\nretire_latency_mean_us: %.3f\ncold_peak_bytes: %zu\nchecksum: %" PRIu64 "\n",N/Elapsed,Times[N/2],Times[(N-1)*99/100],Times[N-1],Retire*1000000./N,Peak,Sum);free(Times);return 0;
}
