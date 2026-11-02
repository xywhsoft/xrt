#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct txgate {xatomic32 Entered,Release;bool Hold;} txgate;
static void tcheck(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] send-alias: %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void tnotify(xmqttclient* C,ptr Data){txgate* G=Data;xmqttclientinfo I;xmqttClientInfo(C,&I);if(!G->Hold || !I.Ready || I.State!=XMQTT_CLIENT_OPEN || xrtAtomic32Load(&G->Entered,XMEMORY_ACQUIRE))return;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);double End=xrtTimer()+10;while(!xrtAtomic32Load(&G->Release,XMEMORY_ACQUIRE)){tcheck(xrtTimer()<End,"test barrier");xrtThreadYield();}}
static const xmqttwriteresult* tresult(xfuture* F){tcheck(F && xrtFutureWaitFor(F,8000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed publication Future");return xrtFutureValue(F);}
static xmqttclientadmission tsubmit(xmqttclient* C,const char* Topic,size_t Length,uint16 Alias,unsigned Qos,unsigned Index,size_t Payload,int64 Timeout,xfuture** Out,bool Duplicate)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)(Qos<<1);P.Identifier=Qos?(uint16)(Index+1):0;P.Body.Publish.Topic=(xstrview){Topic,Length};
    unsigned char* Data=malloc(Payload);tcheck(Data!=NULL,"probe payload");memset(Data,'p',Payload);Data[Payload-1]=(unsigned char)('0'+Index);P.Body.Publish.Payload=(xbytesview){Data,Payload};
    xmqttproperty A[2]={{XMQTT_PROPERTY_TOPIC_ALIAS,XMQTT_PROPERTY_UINT16,{0}},{XMQTT_PROPERTY_TOPIC_ALIAS,XMQTT_PROPERTY_UINT16,{0}}};A[0].Value.Number=A[1].Value.Number=Alias;P.Properties=(xmqttpropertylist){A,Duplicate?2u:1u};xmqttclientadmission S=xmqttClientPublishAsync(C,&P,Timeout,Out);free(Data);return S;
}
static void tmarker(xmqttclient* C,const char* Text)
{xmqttmessage* M=NULL;double End=xrtTimer()+8;while(!xmqttClientTakeMessage(C,&M)){tcheck(xrtTimer()<End,"peer observation marker");xrtSleep(1);}const xmqttpacket* P=xmqttMessagePacket(M);tcheck(P->Body.Publish.Topic.Size==8 && !memcmp(P->Body.Publish.Topic.Data,"incoming",8) && P->Body.Publish.Payload.Size==strlen(Text) && !memcmp(P->Body.Publish.Payload.Data,Text,strlen(Text)),"marker topic/payload independent of outgoing alias");xmqttMessageRelease(M);}
static xfuture* tsignal(xmqttclient* C){xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Body.Publish.Topic=XRT_STR_LITERAL("signal");P.Body.Publish.Payload=XRT_BYTES_LITERAL("go");xfuture* F=NULL;tcheck(xmqttClientPublishAsync(C,&P,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"flow signal");return F;}
int main(int Argc,char** Argv)
{
    tcheck(Argc==3,"usage: send_alias_probe PORT CASE");const char* Case=Argv[2];bool Flow=!strncmp(Case,"flow",4),Cancel=!strcmp(Case,"cancel_bind"),Timeout=!strcmp(Case,"timeout_bind"),Snapshot=!strcmp(Case,"snapshot");
    bool Partial=!strncmp(Case,"partial_",8),PeerClose=!strcmp(Case,"peer_close"),Restricted=!strcmp(Case,"disabled") || !strcmp(Case,"over_max"),Bad=!strcmp(Case,"zero") || !strcmp(Case,"duplicate") || !strcmp(Case,"unknown") || !strcmp(Case,"private");
    bool Qos1=!strcmp(Case,"qos1") || !strcmp(Case,"reject_bind"),Qos2=!strcmp(Case,"qos2"),Graceful=!strcmp(Case,"graceful"),Cap=!strcmp(Case,"cap_peer") || !strcmp(Case,"cap_local");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);tcheck(Engine && xrtNetEngineStart(Engine),"Engine");txgate G={0};G.Hold=Cancel || Timeout || Snapshot;xrtAtomic32Init(&G.Entered,0);xrtAtomic32Init(&G.Release,0);
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Notify=tnotify;K.User=&G;K.SetupTimeout=2000;
    if(!strcmp(Case,"tiny") || Partial || PeerClose){K.Stream.WriteLimit=K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;if(Partial || PeerClose)K.Codec.MaxPacketBytes=2u*1024u*1024u;}
    if(!strcmp(Case,"cap_local")){K.Codec.MaxPacketBytes=40;K.InitialInputBytes=5;}
    if(!strcmp(Case,"private"))K.MaxPrivateBytes=16000;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("send-alias");xmqttproperty Receive={XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Receive.Value.Number=1;
    if(!strcmp(Case,"duplex")){Connect.Properties=(xmqttpropertylist){&Receive,1};}xnetaddr A;xmqttclient* C=NULL;tcheck(xrtNetAddrParse(&A,"127.0.0.1",(uint16)atoi(Argv[1])) && xmqttClientConnect(Engine,&A,1,&K,&Connect,&C),"factory");xfuture* Ready=xmqttClientReady(C);tcheck(xrtFutureWaitFor(Ready,6000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"readiness");
    if(G.Hold){double End=xrtTimer()+5;while(!xrtAtomic32Load(&G.Entered,XMEMORY_ACQUIRE)){tcheck(xrtTimer()<End,"entered barrier");xrtSleep(1);}}
    xfuture *F[10]={0},*Signal=NULL,*Closed=NULL;size_t Count=0;const char* Alpha="alias/alpha";const char* Bravo="alias/bravo";const char* Other="alias/other";char* Long=NULL;size_t TopicLength=11;
    if(Cap){TopicLength=30;Long=malloc(TopicLength+1);memset(Long,'t',TopicLength);Long[TopicLength]=0;Alpha=Long;}
    if(!strcmp(Case,"max_topic")){TopicLength=65535;Long=malloc(TopicLength+1);memset(Long,'t',TopicLength);Long[TopicLength]=0;Alpha=Long;}
    uint16 Alias=!strcmp(Case,"max_id")?65535:!strcmp(Case,"over_max")?3:1;
    size_t Payload=!strcmp(Case,"vbi127")?111:!strcmp(Case,"vbi16383")?16367:Partial || PeerClose?1048576:5;
    if(Bad){xfuture* Out=(xfuture*)(uintptr_t)1;xmqttclientadmission S=tsubmit(C,!strcmp(Case,"unknown")?NULL:Alpha,!strcmp(Case,"unknown")?0:TopicLength,!strcmp(Case,"zero")?0:Alias,0,0,5,5000,&Out,!strcmp(Case,"duplicate"));tcheck(S==(!strcmp(Case,"private")?XMQTT_ADMISSION_OVERLOADED:XMQTT_ADMISSION_ERROR) && Out==(xfuture*)(uintptr_t)1,"invalid/overloaded alias output atomicity");}
    else if(Flow){
        tcheck(tsubmit(C,Alpha,11,1,1,0,5,5000,&F[0],false)==0 && tsubmit(C,Bravo,11,1,1,1,5,!strcmp(Case,"flow_timeout")?100:5000,&F[1],false)==0 && tsubmit(C,NULL,0,1,0,2,5,5000,&F[2],false)==0 && tsubmit(C,Other,11,1,0,3,5,5000,&F[3],false)==0,"flow admissions");Count=4;
        if(strcmp(Case,"flow")){tmarker(C,"cancel");if(!strcmp(Case,"flow_cancel"))tcheck(xrtFutureCancel(F[1]),"cancel blocked rebind");const xmqttwriteresult* R=tresult(F[1]);tcheck(R->Status==(!strcmp(Case,"flow_cancel")?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_TIMEOUT) && !R->AcceptedBytes,"unsent blocked binding terminal result");Signal=tsignal(C);}
        for(size_t I=0;I<4;++I){(void)tresult(F[I]);}tcheck(tsubmit(C,NULL,0,1,0,4,5,5000,&F[4],false)==0,"post-flow snapshot uses latest admitted topic");Count=5;tmarker(C,"done");
    }else {
        unsigned Qos=Qos1?1:Qos2?2:Cancel || Timeout || PeerClose || !strcmp(Case,"restricted_bind")?1:0;
        tcheck(tsubmit(C,Alpha,TopicLength,Alias,Qos,0,Cap?1:Payload,Timeout?0:!strcmp(Case,"partial_timeout")?100:5000,&F[0],false)==0,"first alias admission");Count=1;
        if(Cancel || Timeout || Snapshot){tcheck(tsubmit(C,NULL,0,Alias,0,1,5,5000,&F[1],false)==0 && tsubmit(C,Bravo,11,Alias,0,2,5,5000,&F[2],false)==0 && tsubmit(C,NULL,0,Alias,0,3,5,5000,&F[3],false)==0,"queued topic snapshots");Count=4;if(Cancel)tcheck(xrtFutureCancel(F[0]),"cancel unsent binding");xrtAtomic32Store(&G.Release,1,XMEMORY_RELEASE);}
        else if(Partial || PeerClose){if(!strcmp(Case,"partial_cancel")){tmarker(C,"cancel");tcheck(xrtFutureCancel(F[0]),"cancel partial frame");}}
        else if(!Restricted){
            bool Remap=!strcmp(Case,"remap"),Reject=!strcmp(Case,"reject_bind"),BindRestricted=!strcmp(Case,"restricted_bind");
            tcheck(tsubmit(C,Remap?Bravo:BindRestricted || Reject?NULL:Alpha,Remap?11:BindRestricted || Reject?0:TopicLength,Alias,BindRestricted || Reject?0:Qos,1,Cap?20:Payload,5000,&F[1],false)==0,"second alias admission");Count=2;
            if(!Cap && !BindRestricted && !Reject){tcheck(tsubmit(C,NULL,0,Alias,Qos,2,Payload,5000,&F[2],false)==0,"compact input admission");Count=3;}
        }
        if(!Partial && !PeerClose && !Restricted)tmarker(C,"done");
    }
    xmqttclientstatus ExpectedClose=Partial?!strcmp(Case,"partial_timeout")?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_CANCELLED:PeerClose?XMQTT_STATUS_TRANSPORT:XMQTT_STATUS_CLOSED;
    size_t Accepted[10]={0};for(size_t I=0;I<Count;++I){const xmqttwriteresult* R=tresult(F[I]);Accepted[I]=R->AcceptedBytes;xmqttclientstatus Expected=Partial?ExpectedClose:PeerClose?XMQTT_STATUS_TRANSPORT:(Cancel || Timeout) && !I?Cancel?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_TIMEOUT:Flow && I==1 && strcmp(Case,"flow")?!strcmp(Case,"flow_cancel")?XMQTT_STATUS_CANCELLED:XMQTT_STATUS_TIMEOUT:Restricted || (!strcmp(Case,"restricted_bind") && !I)?XMQTT_STATUS_RESTRICTED:!strcmp(Case,"reject_bind") && !I?XMQTT_STATUS_REJECTED:XMQTT_STATUS_OK;
        tcheck(R->Status==Expected,"typed per-operation result");if(Expected==XMQTT_STATUS_RESTRICTED || ((Cancel || Timeout || Flow) && Expected!=XMQTT_STATUS_OK))tcheck(!R->AcceptedBytes && R->Delivery==XMQTT_NOT_SENT,"unsent operation never claims delivery");
        if(Partial){tcheck(R->AcceptedBytes && R->AcceptedBytes<1048576 && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"partial frame never reports complete/success");}if(Qos2)tcheck(R->PubcompReceived,"QoS2 final confirmation");}
    if(!Partial && !PeerClose){if(Graceful){tcheck(xmqttClientDisconnect(C,NULL,5000,&Closed)==0 && tresult(Closed)->Status==XMQTT_STATUS_OK,"graceful alias drain");ExpectedClose=XMQTT_STATUS_OK;}else tcheck(xmqttClientClose(C),"Close");}
    tcheck(xmqttClientWaitFor(C,6000)==XWAIT_OK,"physical retirement");xmqttclientinfo Info;xmqttClientInfo(C,&Info);tcheck(Info.CloseStatus==ExpectedClose && !Info.Requests && !Info.OutgoingBytes && Info.Accepted==Info.Completed,"physical accounting");tcheck(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"all owners destroyed");
    printf("{\"case\":\"%s\",\"accepted\":[",Case);for(size_t I=0;I<Count;++I)printf("%s%zu",I?",":"",Accepted[I]);puts("]}");for(size_t I=0;I<Count;++I)xrtFutureDestroy(F[I]);xrtFutureDestroy(Ready);xrtFutureDestroy(Signal);xrtFutureDestroy(Closed);free(Long);return 0;
}
