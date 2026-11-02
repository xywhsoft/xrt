/* Independent Python byte peer drives this public-API process. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void scheck(bool B,const char* M)
{if(!B){fprintf(stderr,"[FAIL] subscription probe: %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static const void* sresult(xfuture* F)
{scheck(F && xrtFutureWaitFor(F,6000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed Future");return xrtFutureValue(F);}
static xatomic32 WorkerChecks;
static void snotify(xmqttclient* C,ptr Data)
{
    (void)Data;xmqttclientinfo I;xmqttClientInfo(C,&I);
    if(I.Ready && !xrtAtomic32Load(&WorkerChecks,XMEMORY_ACQUIRE)){
        xrtAtomic32Store(&WorkerChecks,1,XMEMORY_RELEASE);xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&P);xfuture* F=(xfuture*)(uintptr_t)1;
        scheck(xmqttClientSubscribe(C,&P,1,&F)==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"blocking subscribe worker rejection");xrtClearError();
        P.Type=XMQTT_PACKET_UNSUBSCRIBE;scheck(xmqttClientUnsubscribe(C,&P,1,&F)==XMQTT_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"blocking unsubscribe worker rejection");xrtClearError();
        xrtAtomic32Store(&WorkerChecks,2,XMEMORY_RELEASE);
    }
}
static void smarker(xmqttclient* C)
{
    xmqttmessage* M=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&M)){scheck(xrtTimer()<End,"peer marker");xrtSleep(1);}
    scheck(xmqttMessagePacket(M)->Body.Publish.Payload.Size==4 && !memcmp(xmqttMessagePacket(M)->Body.Publish.Payload.Data,"seen",4),"owned duplex publication");xmqttMessageRelease(M);
}
static void sidle(xmqttclient* C)
{xmqttclientinfo I;double End=xrtTimer()+5;do{xmqttClientInfo(C,&I);scheck(xrtTimer()<End,"request retirement");if(I.Requests)xrtSleep(1);}while(I.Requests);}
int main(int Argc,char** Argv)
{
    scheck(Argc==4,"usage: subscription_probe PORT VERSION CASE");unsigned Version=(unsigned)atoi(Argv[2]);const char* Case=Argv[3];bool Unsub=!strncmp(Case,"unsub_",6);const char* Name=Unsub?Case+6:Case;
    bool Mixed=!strcmp(Name,"mixed"),Reject=!strcmp(Name,"reject"),Metadata=!strcmp(Name,"metadata"),Queued=!strcmp(Name,"queued"),Tiny=!strcmp(Name,"tiny"),Graceful=!strcmp(Name,"graceful");
    bool Cancel=!strcmp(Name,"cancel"),Timeout=!strcmp(Name,"timeout"),Close=!strcmp(Name,"close"),Partial=!strcmp(Name,"partial"),Reverse=!strcmp(Name,"reverse"),Auto=!strcmp(Name,"auto"),Flow=!strcmp(Name,"flow"),Busy=!strcmp(Name,"id_busy"),Duplex=!strcmp(Name,"duplex");
    bool Restrict=!strcmp(Name,"wildcard") || !strcmp(Name,"shared") || !strcmp(Name,"subid") || !strcmp(Name,"max_packet");
    bool Overload=!strcmp(Name,"request_limit") || !strcmp(Name,"byte_limit") || !strcmp(Name,"private_limit") || !strcmp(Name,"object_limit");
    bool Invalid=!strcmp(Name,"invalid") || !strcmp(Name,"filter_limit");
    bool Bad=!strcmp(Name,"short") || !strcmp(Name,"long") || !strcmp(Name,"qos") || !strcmp(Name,"bad_reason") || !strcmp(Name,"wrong_kind") || !strcmp(Name,"wrong_id") || !strcmp(Name,"problem");
    bool Duplicate=!strcmp(Name,"duplicate"),Extended=!strcmp(Name,"extended");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);scheck(Engine && xrtNetEngineStart(Engine),"Engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;K.SetupTimeout=2000;K.InitialInputBytes=5;K.Codec.MaxPacketBytes=8192;
    if(Tiny){K.Stream.WriteLimit=K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=K.ReadBytes=1;}
    if(Metadata)K.MaxObjects=3;
    if(!strcmp(Name,"object_limit"))K.MaxObjects=2;
    if(!strcmp(Name,"byte_limit"))K.MaxOutgoingBytes=1;
    if(!strcmp(Name,"private_limit"))K.MaxPrivateBytes=20000;
    if(!strcmp(Name,"request_limit"))K.MaxRequests=1;
    if(!strcmp(Name,"filter_limit"))K.Codec.MaxTopics=2;
    if(!strcmp(Name,"worker")){xrtAtomic32Init(&WorkerChecks,0);K.Notify=snotify;}
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("subscriptions");
    xmqttproperty Problem={XMQTT_PROPERTY_REQUEST_PROBLEM_INFO,XMQTT_PROPERTY_BYTE,{0}};
    if(!strcmp(Name,"problem") || !strcmp(Name,"problem_clean"))Connect.Properties=(xmqttpropertylist){&Problem,1};
    xnetaddr Address;xmqttclient* C=NULL;scheck(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)atoi(Argv[1])) && xmqttClientConnect(Engine,&Address,1,&K,&Connect,&C),"factory");xfuture* Ready=xmqttClientReady(C);
    if(!Queued)scheck(((const xmqttreadyresult*)sresult(Ready))->Status==XMQTT_STATUS_OK,"readiness");
    xmqttsubscription Topics[3]={{XRT_STR_LITERAL("test/zero"),0},{XRT_STR_LITERAL("test/+"),1},{XRT_STR_LITERAL("test/#"),2}};
    if(Unsub)for(size_t I=0;I<3;++I)Topics[I].Options=0;
    if(!strcmp(Name,"invalid"))Topics[1].Filter=XRT_STR_LITERAL("bad/#/filter");
    if(!strcmp(Name,"shared"))Topics[1].Filter=XRT_STR_LITERAL("$share/group/test/+");
    if(Extended){Topics[0].Options=0x24;Topics[1].Options=0x19;Topics[2].Options=0x22;}
    xmqttpacket P;xmqttPacketInit(Unsub?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,&P);P.Identifier=Auto?0:77;P.Body.Topics=(xmqttsubscriptionlist){Topics,3};
    xmqttproperty Props[2]={{XMQTT_PROPERTY_SUBSCRIPTION_ID,XMQTT_PROPERTY_VARIABLE,{0}},{XMQTT_PROPERTY_USER,XMQTT_PROPERTY_STRING_PAIR,{0}}};Props[0].Value.Number=128;
    char Long[140];memset(Long,'v',sizeof(Long));Props[1].Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("key"),(xstrview){Long,sizeof(Long)}};
    if(!strcmp(Name,"subid"))P.Properties=(xmqttpropertylist){Props,1};
    if(Extended)P.Properties=(xmqttpropertylist){Props,2};
    xfuture *F=NULL,*G=NULL,*Published=NULL,*Closed=NULL,*Held=NULL;uint16 Id=P.Identifier;
    if(Flow || Busy || !strcmp(Name,"request_limit")){
        xmqttpacket Pub;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);Pub.Flags=2;Pub.Identifier=Flow?88:77;Pub.Body.Publish.Topic=XRT_STR_LITERAL("held");scheck(xmqttClientPublishAsync(C,&Pub,5000,&Held)==XMQTT_ADMISSION_ACCEPTED,"reserve publication id/credit");
    }
    if(Busy){xfuture* Out=(xfuture*)(uintptr_t)1;scheck((Unsub?xmqttClientUnsubscribeAsync(C,&P,5000,&Out):xmqttClientSubscribeAsync(C,&P,5000,&Out))==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1,"publish id excludes controls");P.Identifier=78;Id=78;}
    xfuture* Out=(xfuture*)(uintptr_t)1;xmqttclientadmission A=Unsub?xmqttClientUnsubscribeAsync(C,&P,Timeout?200:5000,&Out):xmqttClientSubscribeAsync(C,&P,Timeout?200:5000,&Out);
    if(Overload || Invalid){scheck(A==(Invalid?XMQTT_ADMISSION_ERROR:XMQTT_ADMISSION_OVERLOADED) && Out==(xfuture*)(uintptr_t)1,"admission failure output atomicity");}
    else {
        scheck(A==XMQTT_ADMISSION_ACCEPTED,"control admission");F=Out;
        /* Caller descriptors/text are no longer live inputs. */
        memset(Topics,0,sizeof(Topics));memset(Long,'x',sizeof(Long));memset(&P,0,sizeof(P));
        if(Auto || Reverse){xmqttsubscription T={XRT_STR_LITERAL("other"),0};xmqttPacketInit(Unsub?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,&P);P.Identifier=Auto?0:333;P.Body.Topics=(xmqttsubscriptionlist){&T,1};scheck((Unsub?xmqttClientUnsubscribeAsync(C,&P,5000,&G):xmqttClientSubscribeAsync(C,&P,5000,&G))==XMQTT_ADMISSION_ACCEPTED,"second control");}
        if(Cancel || Close){smarker(C);if(Cancel)scheck(xrtFutureCancel(F),"explicit cancellation");else scheck(xmqttClientClose(C),"Close after peer marker");}
        if(Graceful)scheck(xmqttClientDisconnectAsync(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful drain accepted control");
        const xmqttsubscriptionresult* R=sresult(F);if(Auto)Id=R->Identifier;
        xmqttclientstatus Expected=Bad?XMQTT_STATUS_PROTOCOL:Timeout?XMQTT_STATUS_TIMEOUT:Cancel?XMQTT_STATUS_CANCELLED:Close?XMQTT_STATUS_CLOSED:Partial?XMQTT_STATUS_TRANSPORT:Metadata?XMQTT_STATUS_LIMIT:Restrict?XMQTT_STATUS_RESTRICTED:(Mixed || Reject)?XMQTT_STATUS_REJECTED:XMQTT_STATUS_OK;
        scheck(R->Status==Expected && R->Identifier==Id && Id && R->Requested==3,"correlated status/id/count");
        if(Bad || Timeout || Cancel || Close || Partial || Restrict)scheck(!R->Acknowledged && !R->Ack && !R->Granted && !R->Rejected && (Restrict?!R->AcceptedBytes:R->AcceptedBytes>0),"unacknowledged or restricted request has no invented result");
        else {
            size_t Rejected=Reject?3:Mixed?1:0;scheck(R->Acknowledged && R->Rejected==Rejected && R->Granted==3-Rejected && (Metadata?!R->Ack:R->Ack!=NULL),"per-filter outcome including partial rejection");
            if(R->Ack){const xmqttpacket* Ack=xmqttMessagePacket(R->Ack);size_t Count=Unsub && Version==4?0:3;scheck(Ack->Identifier==Id && Ack->Type==(Unsub?XMQTT_PACKET_UNSUBACK:XMQTT_PACKET_SUBACK) && Ack->Body.Reasons.Size==Count,"owned ACK descriptor");}
        }
        if(G){const xmqttsubscriptionresult* S=sresult(G);scheck(S->Status==XMQTT_STATUS_OK && S->Requested==1 && S->Granted==1 && S->Identifier!=Id,"reverse correlation and auto ids");}
        if(Duplex)smarker(C);
        if(Held)scheck(((const xmqttwriteresult*)sresult(Held))->Status==XMQTT_STATUS_OK,"publication coexists with control at exhausted send credit");
        if(Closed)scheck(((const xmqttwriteresult*)sresult(Closed))->Status==XMQTT_STATUS_OK,"graceful waits for SUBACK/UNSUBACK");
        if(!strcmp(Name,"worker")){double End=xrtTimer()+5;while(xrtAtomic32Load(&WorkerChecks,XMEMORY_ACQUIRE)!=2){scheck(xrtTimer()<End,"worker checks finished");xrtSleep(1);}}
        if(R->Ack)scheck(xmqttMessageRetain(R->Ack),"retained ACK");
    }
    if(!Duplicate && !Bad && !Timeout && !Cancel && !Close && !Partial && !Metadata && !Graceful){if(Held)(void)sresult(Held);sidle(C);scheck(xmqttClientClose(C),"abortive close");}
    scheck(xmqttClientWaitFor(C,5000)==XWAIT_OK,"physical retirement");xmqttclientinfo Info;xmqttClientInfo(C,&Info);scheck(!Info.Requests && !Info.OutgoingBytes && Info.Completed==Info.Accepted,"physical accounting");
    const xmqttsubscriptionresult* R=F?xrtFutureValue(F):NULL;const xmqttmessage* Ack=R?R->Ack:NULL;
    scheck(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"destroy owners");xrtFutureDestroy(F);
    if(Ack){const xmqttpacket* Q=xmqttMessagePacket(Ack);size_t Count=Unsub && Version==4?0:3;scheck(Q->Body.Reasons.Size==Count,"retained reason count");
        if(Count){const unsigned char* Codes=Q->Body.Reasons.Data;scheck(Codes[0]==(Reject?(Version==5?0x87:0x80):0) && Codes[1]==(Reject || Mixed?(Version==5?0x87:0x80):Unsub?0x11:1) && Codes[2]==(Reject?(Version==5?0x87:0x80):Unsub?0:2),"owned ordered reason bytes after all owners destroyed");}
        if(Version==5 && strcmp(Name,"problem_clean")){scheck(Q->Properties.Count==3 && Q->Properties.Data[0].Value.Text.Size==8 && !memcmp(Q->Properties.Data[0].Value.Text.Data,"accepted",8) && !memcmp(Q->Properties.Data[2].Value.Pair.Value.Data,"two",3),"owned reason string and repeated User Properties");}
        xmqttMessageRelease(Ack);
    }
    xrtFutureDestroy(G);xrtFutureDestroy(Published);xrtFutureDestroy(Closed);xrtFutureDestroy(Held);xrtFutureDestroy(Ready);
    printf("{\"case\":\"%s\",\"version\":%u,\"close\":%d}\n",Case,Version,(int)Info.CloseStatus);return 0;
}
