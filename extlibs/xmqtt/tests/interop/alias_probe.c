#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct aliasprobe {xmqttmessage* Messages[16];xatomic32 Count;bool Queue,Close;size_t FirstPrivate;} aliasprobe;
static void acheck(bool B,const char* M)
{if(!B){fprintf(stderr,"[FAIL] alias probe: %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static void atake(xmqttclient* C,aliasprobe* P)
{
    xmqttmessage* M=NULL;while(xmqttClientTakeMessage(C,&M)){uint32 N=xrtAtomic32Load(&P->Count,XMEMORY_RELAXED);acheck(N<16,"bounded message collection");P->Messages[N]=M;
        if(!N){xmqttclientinfo I;xmqttClientInfo(C,&I);P->FirstPrivate=I.PrivateBytes;}
        xrtAtomic32Store(&P->Count,N+1,XMEMORY_RELEASE);if(P->Close)acheck(xmqttClientClose(C),"Close in alias notification");}
}
static void anotify(xmqttclient* C,ptr Data)
{aliasprobe* P=Data;if(!P->Queue)atake(C,P);}
static const void* aresult(xfuture* F)
{acheck(F && xrtFutureWaitFor(F,6000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed Future");return xrtFutureValue(F);}
static void apublish(xmqttclient* C,const char* Topic,const char* Body,unsigned Qos,uint16 Id,xfuture** F)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)(Qos<<1);P.Identifier=Id;P.Body.Publish.Topic=(xstrview){Topic,strlen(Topic)};P.Body.Publish.Payload=(xbytesview){(const unsigned char*)Body,strlen(Body)};
    acheck(xmqttClientPublishAsync(C,&P,5000,F)==XMQTT_ADMISSION_ACCEPTED,"outgoing control marker");
}
static void averify(const char* Case,const aliasprobe* P,unsigned Count)
{
    bool Remap=!strncmp(Case,"remap_",6),Independent=!strcmp(Case,"independent"),Dup=!strcmp(Case,"duplicate2_rebind"),Long=!strcmp(Case,"long_topic");
    for(unsigned I=0;I<Count;++I){const xmqttpacket* Q=xmqttMessagePacket(P->Messages[I]);const char* Topic="topic/alpha";
        if(Remap && I>=2)Topic=!strcmp(Case,"remap_short")?"x":!strcmp(Case,"remap_same")?"topic/bravo":"topic/much/longer/than/before";
        if(!strcmp(Case,"cache_same_capacity") && I>=1)Topic="topic/bravo";
        if(Independent && (I==1 || I==3))Topic="topic/bravo";
        if(Dup && I==1)Topic="topic/bravo";
        if(!strcmp(Case,"plain_does_not_rebind") && I==1)Topic="topic/bravo";
        acheck(Q->Type==XMQTT_PACKET_PUBLISH && Q->Body.Publish.Payload.Size==4 && !memcmp(Q->Body.Publish.Payload.Data,"body",4),"owned payload");
        if(Long){acheck(Q->Body.Publish.Topic.Size==65535,"maximum topic length");for(size_t J=0;J<65535;++J)acheck(Q->Body.Publish.Topic.Data[J]=='a',"owned maximum topic bytes");}
        else acheck(Q->Body.Publish.Topic.Size==strlen(Topic) && !memcmp(Q->Body.Publish.Topic.Data,Topic,strlen(Topic)),"resolved topic remains immutable after remap and all owners destroyed");
        xmqttpacket Raw;xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};acheck(xmqttPacketRead5(Q->Wire,XMQTT_SERVER_TO_CLIENT,NULL,&B,&Raw,NULL)==XMQTT_CODEC_DONE,"owned original wire");
        bool Compact=Remap?(I==1 || I==3):Independent?I>=2:!strcmp(Case,"plain_does_not_rebind")?I==2:!strcmp(Case,"cache_same_capacity")?I==2:I==1;
        acheck(Compact?!Raw.Body.Publish.Topic.Size:Raw.Body.Publish.Topic.Size!=0,"raw empty/full topic is preserved independently from logical topic");
        if(!strcmp(Case,"properties"))acheck(Q->Properties.Count==3 && Q->Properties.Data[0].Value.Pair.Name.Size==1 && Q->Properties.Data[0].Value.Pair.Name.Data[0]=='k' && Q->Properties.Data[0].Value.Pair.Value.Data[0]=='v' && Q->Properties.Data[2].Value.Number==128,"retained User Properties and subscription id with alias");
    }
}
int main(int Argc,char** Argv)
{
    acheck(Argc==7,"usage: alias_probe PORT CASE STATUS RECEIVED MAXIMUM REASON");const char* Case=Argv[2];xmqttclientstatus Expected=(xmqttclientstatus)atoi(Argv[3]);unsigned Target=(unsigned)atoi(Argv[4]);uint16 Maximum=(uint16)atoi(Argv[5]);uint8 Reason=(uint8)atoi(Argv[6]);bool Twice=!strcmp(Case,"per_connection");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);acheck(Engine && xrtNetEngineStart(Engine),"Engine");aliasprobe Saved={0};
    for(unsigned Phase=0;Phase<(Twice?2u:1u);++Phase){aliasprobe P={0};xrtAtomic32Init(&P.Count,0);P.Queue=!strcmp(Case,"queue_limit");P.Close=!strcmp(Case,"close_callback");
        xmqttclientconfig K;xmqttClientConfigInit(&K);K.InitialInputBytes=1024;K.SetupTimeout=2000;K.Notify=anotify;K.User=&P;
        if(!strcmp(Case,"fragmented")){K.InitialInputBytes=5;K.ReadBytes=1;K.WriteBytes=K.WriteChunkBytes=1;}
        if(!strcmp(Case,"metadata_limit"))K.MaxObjects=4;
        if(P.Queue)K.MaxMessages=1;
        if(!strcmp(Case,"message_bytes"))K.MaxMessageBytes=1;
        if(!strcmp(Case,"resolved_bytes"))K.MaxObjectBytes=969; /* Includes appended logical topic, not only compact raw wire. */
        if(!strcmp(Case,"private_table"))K.MaxPrivateBytes=20000;
        if(!strcmp(Case,"private_entry") || !strcmp(Case,"private_replace")){K.InitialInputBytes=70000;K.MaxPrivateBytes=100000;}
        xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=XRT_STR_LITERAL("alias-probe");Connect.Body.Connect.CleanStart=true;
        xmqttproperty Alias={XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,XMQTT_PROPERTY_UINT16,{0}};Alias.Value.Number=Maximum;
        if(strcmp(Case,"default_none"))Connect.Properties=(xmqttpropertylist){&Alias,1};
        xmqttclient* C=NULL;xnetaddr A;acheck(xrtNetAddrParse(&A,"127.0.0.1",(uint16)atoi(Argv[1])) && xmqttClientConnect(Engine,&A,1,&K,&Connect,&C),"connect with incoming alias capacity");xfuture* Ready=xmqttClientReady(C);acheck(((const xmqttreadyresult*)aresult(Ready))->Status==XMQTT_STATUS_OK,"readiness");
        xmqttclientinfo Info;double End=xrtTimer()+5;do{xmqttClientInfo(C,&Info);acheck(xrtTimer()<End,"startup retirement");if(Info.StartupBytes)xrtSleep(1);}while(Info.StartupBytes);
        xfuture *Start=NULL,*Duplex=NULL,*Closed=NULL;apublish(C,"go","start",0,0,&Start);acheck(((const xmqttwriteresult*)aresult(Start))->Status==XMQTT_STATUS_OK,"start marker");
        if(!strcmp(Case,"duplex"))apublish(C,"out","payload",1,88,&Duplex);
        unsigned Want=Phase?0:Target;xmqttclientstatus Status=Phase?XMQTT_STATUS_PROTOCOL:Expected;uint8 Local=Phase?0x82:Reason;
        if(Status==XMQTT_STATUS_CLOSED){
            do{xmqttClientInfo(C,&Info);acheck(xrtTimer()<End,"alias delivery wait");if(Info.Received<Want)xrtSleep(1);}while(Info.Received<Want && Info.State<XMQTT_CLIENT_CLOSING);
            if(Duplex)acheck(((const xmqttwriteresult*)aresult(Duplex))->Delivery==XMQTT_PEER_ACCEPTED,"alias receive coexists with outgoing QoS1");
            if(!strcmp(Case,"graceful")){acheck(xmqttClientDisconnect(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED && ((const xmqttwriteresult*)aresult(Closed))->Status==XMQTT_STATUS_OK,"graceful alias close");Status=XMQTT_STATUS_OK;}
            else if(!P.Close)acheck(xmqttClientClose(C),"Close");
        }
        acheck(xmqttClientWaitFor(C,6000)==XWAIT_OK,"physical retirement");xmqttClientInfo(C,&Info);atake(C,&P);
        acheck(Info.CloseStatus==Status && Info.LocalReason==Local && Info.Received==Want && xrtAtomic32Load(&P.Count,XMEMORY_ACQUIRE)==Want && !Info.Requests && !Info.OutgoingBytes && Info.Accepted==Info.Completed,"typed close, exact deliveries and physical request accounting");
        if(!strcmp(Case,"cache_same_capacity"))acheck(P.FirstPrivate==Info.PrivateBytes,"same-capacity remap allocates no private bytes");
        acheck(xmqttClientDestroy(C),"Client destroy");xrtFutureDestroy(Ready);xrtFutureDestroy(Start);xrtFutureDestroy(Duplex);xrtFutureDestroy(Closed);
        if(!Phase)Saved=P;else acheck(!Want,"new connection has no alias mappings");
    }
    acheck(xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"Engine destroy");averify(Case,&Saved,Target);for(unsigned I=0;I<Target;++I)xmqttMessageRelease(Saved.Messages[I]);
    printf("{\"case\":\"%s\",\"received\":%u}\n",Case,Target);return 0;
}
