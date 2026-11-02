#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void qcheck(bool OK,const char* Message)
{if(!OK){fprintf(stderr,"[FAIL] QoS1 probe: %s (%s)\n",Message,xrtErrorMessage(xrtGetError()));exit(1);}}
static const xmqttwriteresult* qresult(xfuture* F)
{qcheck(F && xrtFutureWaitFor(F,6000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed publication result");return xrtFutureValue(F);}
static xfuture* qpublish(xmqttclient* C,uint8 Qos,uint16 Id,int64 Timeout)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)(Qos<<1);P.Identifier=Id;P.Body.Publish.Topic=XRT_STR_LITERAL("qos/one");P.Body.Publish.Payload=XRT_BYTES_LITERAL("original");xfuture* F=NULL;
    qcheck(xmqttClientPublishAsync(C,&P,Timeout,&F)==XMQTT_ADMISSION_ACCEPTED && P.Identifier==Id,"immutable auto/explicit id admission");return F;
}
static void qack(const xmqttwriteresult* R,uint16 Id,uint8 Reason)
{
    qcheck(R->Status==(Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED) && R->Delivery==(Reason<0x80?XMQTT_PEER_ACCEPTED:XMQTT_PEER_REJECTED) && R->Identifier==Id && R->Reason==Reason && R->Puback,"correlated PUBACK result");
    const xmqttpacket* A=xmqttMessagePacket(R->Puback);qcheck(A->Type==XMQTT_PACKET_PUBACK && A->Identifier==Id && A->Reason==Reason,"retained PUBACK descriptor");
}
int main(int Argc,char** Argv)
{
    qcheck(Argc==4,"usage: qos1_probe PORT VERSION CASE");const char* Case=Argv[3];unsigned Version=(unsigned)atoi(Argv[2]);
    bool Window=!strcmp(Case,"window"),Unsent=!strcmp(Case,"unsent_ack"),Busy=!strcmp(Case,"id_busy"),Reuse=!strcmp(Case,"id_reuse"),Reverse=!strcmp(Case,"reverse"),Auto=!strcmp(Case,"auto");
    bool Timeout=!strcmp(Case,"timeout"),Cancel=!strcmp(Case,"cancel"),Metadata=!strcmp(Case,"metadata"),Private=!strcmp(Case,"private"),Duplicate=!strcmp(Case,"duplicate_ack"),Partial=!strcmp(Case,"partial_ack");
    bool Bad=Unsent || !strcmp(Case,"unknown_ack") || !strcmp(Case,"zero_ack") || !strcmp(Case,"bad_reason") || !strcmp(Case,"wrong_kind") || !strcmp(Case,"problem_info");
    uint8 Reason=!strcmp(Case,"reject")?0x87:!strcmp(Case,"no_subscribers")?0x10:0;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);qcheck(Engine && xrtNetEngineStart(Engine),"Engine");
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;K.InitialInputBytes=5;K.SetupTimeout=2000;K.Codec.MaxPacketBytes=8192;
    if(Metadata)K.MaxObjects=3;
    if(Private)K.MaxPrivateBytes=20000;
    if(!strcmp(Case,"tiny")){K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;K.WriteBytes=K.WriteChunkBytes=1;K.ReadBytes=1;}
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("qos-one");Connect.Body.Connect.KeepAlive=Window?1:0;
    xmqttproperty Problem={XMQTT_PROPERTY_REQUEST_PROBLEM_INFO,XMQTT_PROPERTY_BYTE,{0}};
    if(!strcmp(Case,"problem_info") || !strcmp(Case,"problem_info_clean"))Connect.Properties=(xmqttpropertylist){&Problem,1};
    xnetaddr Address;xmqttclient* C=NULL;qcheck(xrtNetAddrParse(&Address,"127.0.0.1",(uint16)atoi(Argv[1])) && xmqttClientConnect(Engine,&Address,1,&K,&Connect,&C),"connection factory");
    xfuture* Ready=xmqttClientReady(C);qcheck(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xmqttreadyresult*)xrtFutureValue(Ready))->Status==XMQTT_STATUS_OK,"readiness");
    xfuture *F=NULL,*G=NULL,*Zero=NULL;const xmqttwriteresult *R=NULL,*S=NULL;
    uint16 First=Auto?65535:Busy || Reuse?77:1,Second=257;
    if(Private){xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Body.Publish.Topic=XRT_STR_LITERAL("qos/one");xfuture* Out=(xfuture*)(uintptr_t)1;
        qcheck(xmqttClientPublishAsync(C,&P,1000,&Out)==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1,"cold private-cap overload/output atomicity");
    }else {
        F=qpublish(C,1,First,Timeout?200:5000);
        if(Window || Unsent || Reverse || Auto)G=qpublish(C,1,Auto?0:Second,5000);
        if(Busy){xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Identifier=First;P.Body.Publish.Topic=XRT_STR_LITERAL("qos/one");xfuture* Out=(xfuture*)(uintptr_t)1;
            qcheck(xmqttClientPublishAsync(C,&P,1000,&Out)==XMQTT_ADMISSION_OVERLOADED && Out==(xfuture*)(uintptr_t)1,"busy id/output atomicity");}
        if(Window || Unsent || Busy)Zero=qpublish(C,0,0,5000);
        if(Cancel){xmqttmessage* M=NULL;double End=xrtTimer()+5;while(!xmqttClientTakeMessage(C,&M)){qcheck(xrtTimer()<End,"broker-seen cancellation marker");xrtSleep(1);}xmqttMessageRelease(M);qcheck(xrtFutureCancel(F),"Future cancellation");}
        R=qresult(F);if(G)S=qresult(G);if(Zero)qcheck(qresult(Zero)->Status==XMQTT_STATUS_OK,"QoS0 control marker proceeds at exhausted window");
        if(Timeout || Cancel || Partial || Bad){xmqttclientstatus Status=Timeout?XMQTT_STATUS_TIMEOUT:Cancel?XMQTT_STATUS_CANCELLED:Partial?XMQTT_STATUS_TRANSPORT:XMQTT_STATUS_PROTOCOL;
            qcheck(R->Status==Status && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER && !R->Puback,"unacknowledged publication never reports success");
            if(Unsent)qcheck(S->Status==Status && S->Delivery==XMQTT_NOT_SENT && !S->AcceptedBytes,"unsent id cannot be acknowledged");
        }else if(Metadata)qcheck(R->Status==XMQTT_STATUS_LIMIT && R->Delivery==XMQTT_PEER_ACCEPTED && R->Identifier==First && !R->Puback,"metadata cap preserves scalar acknowledgement decision");
        else {qack(R,First,Reason);if(G)qack(S,Auto?S->Identifier:Second,0);if(Auto)qcheck(S->Identifier && S->Identifier!=First,"auto id avoids explicit id");}
        if(Reuse){double End=xrtTimer()+2;xmqttclientinfo I;do{xmqttClientInfo(C,&I);qcheck(xrtTimer()<End,"physical id reservation retirement");if(I.Requests)xrtSleep(1);}while(I.Requests);
            G=qpublish(C,1,First,5000);S=qresult(G);qack(S,First,0);}
    }
    xmqttclientstatus Close=Timeout?XMQTT_STATUS_TIMEOUT:Cancel?XMQTT_STATUS_CANCELLED:Partial?XMQTT_STATUS_TRANSPORT:Metadata?XMQTT_STATUS_LIMIT:(Bad || Duplicate)?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_CLOSED;
    if(Close==XMQTT_STATUS_CLOSED)qcheck(xmqttClientClose(C),"abortive Close");
    qcheck(xmqttClientWaitFor(C,5000)==XWAIT_OK,"physical close");xmqttclientinfo I;xmqttClientInfo(C,&I);
    qcheck(I.CloseStatus==Close && !I.Requests && !I.OutgoingBytes && I.Accepted==I.Completed,"terminal status/physical accounting");
    qcheck(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"Client/Engine destruction");
    if(R && R->Puback){qack(R,First,Reason);const xmqttpacket* A=xmqttMessagePacket(R->Puback);
        if(Version==5 && strcmp(Case,"problem_info_clean")){qcheck(A->Properties.Count==3 && A->Properties.Data[0].Value.Text.Size==8 && !memcmp(A->Properties.Data[0].Value.Text.Data,"accepted",8) &&
            A->Properties.Data[1].Value.Pair.Value.Size==3 && !memcmp(A->Properties.Data[1].Value.Pair.Value.Data,"one",3) && A->Properties.Data[2].Value.Pair.Value.Size==3 && !memcmp(A->Properties.Data[2].Value.Pair.Value.Data,"two",3),"retained reason/repeated User Properties after destruction");}}
    xrtFutureDestroy(F);xrtFutureDestroy(G);xrtFutureDestroy(Zero);xrtFutureDestroy(Ready);
    printf("{\"case\":\"%s\",\"version\":%u,\"close\":%d}\n",Case,Version,(int)Close);return 0;
}
