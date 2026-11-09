#include "../tests/test.h"
#include "../tests/support/tls_resume_peer.h"
#include <stdlib.h>
static int resumeBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y?1:0;}
int main(int Argc,char** Argv)
{
    unsigned Count=Argc>1?(unsigned)strtoul(Argv[1],NULL,10):100;if(Argc>2 || !Count || Count>10000)return 1;
    double *Full=malloc(Count*sizeof(*Full)),*Resumed=malloc(Count*sizeof(*Resumed));testRequire(Full && Resumed,"setup latency storage");mqttTlsResumePeer R;mqttTlsResumeStart(&R,XMQTT_VERSION_5);
    uint64 Sum=0;xtlsresume* Ticket=NULL;
    for(unsigned Pair=0;Pair<Count;++Pair)for(unsigned Round=0;Round<2;++Round){xmqttclienttlsconfig T;char Name[12],Alpn[4];xstrview Protocol;xtlsverifier* V=mqttTlsConfigure(&R.Peer,&T,Name,&Protocol,Alpn);T.Session.Resume=Ticket;T.Session.ResumeOnly=Round!=0;
        xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("resume-bench");xmqttclient* C=NULL;double Start=xrtTimer();
        testRequire(xmqttClientTlsConnect(R.Peer.Engine,&R.Peer.Address,1,NULL,&T,&Q,&C),"setup factory");xfuture* F=xmqttClientReady(C);testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"fresh CONNACK");(Round?Resumed:Full)[Pair]=(xrtTimer()-Start)*1000000.0;
        xrtFutureDestroy(F);xrtTlsVerifierRelease(V);xrtTlsResumeRelease(Ticket);Ticket=NULL;xmqttclienttlsresumeinfo I;testRequire(xmqttClientTlsResumeInfo(C,&I) && I.Established && I.Resumed==(Round!=0),"measured actual TLS handshake");
        if(!Round){mqttTlsResumeIssue(&R,1);double End=xrtTimer()+10;while(!xmqttClientTlsTakeResume(C,&Ticket)){testRequire(xrtTimer()<End,"setup credential publication");xrtSleep(1);}}
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("p");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");F=NULL;testRequire(xmqttClientPublish(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"each setup carries real QoS1");Sum+=((const xmqttwriteresult*)xrtFutureValue(F))->Identifier;xrtFutureDestroy(F);F=NULL;
        testRequire(xmqttClientDisconnect(C,NULL,5000,&F)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)xrtFutureValue(F))->Status==XMQTT_STATUS_OK,"setup authenticated close");xrtFutureDestroy(F);testRequire(xmqttClientWaitFor(C,10000)==XWAIT_OK && xmqttClientDestroy(C),"setup physical owner retirement");mqttTlsResumeReset(&R);
    }
    mqttTlsResumeStop(&R);testRequire(Sum==(uint64)Count*2,"one fresh MQTT identifier per real setup");qsort(Full,Count,sizeof(*Full),resumeBenchCompare);qsort(Resumed,Count,sizeof(*Resumed),resumeBenchCompare);
    printf("full_setup_p50_us: %.3f\nfull_setup_p99_us: %.3f\nresumed_setup_p50_us: %.3f\nresumed_setup_p99_us: %.3f\nchecksum: %llu\n",Full[Count/2],Full[(Count-1)*99/100],Resumed[Count/2],Resumed[(Count-1)*99/100],(unsigned long long)Sum);free(Full);free(Resumed);return 0;
}
