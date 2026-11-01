/* Real sender; independent Python socket peer drives both acknowledgement phases. */
#include <xmqtt.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(bool B,const char* M){if(!B){fprintf(stderr,"[FAIL] QoS2: %s (%s)\n",M,xrtErrorMessage(xrtGetError()));exit(1);}}
static const xmqttwriteresult* result(xfuture* F){check(F && xrtFutureWaitFor(F,7000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"typed result");return xrtFutureValue(F);}
static xfuture* publish(xmqttclient* C,uint8 Qos,uint16 Id,int64 Timeout,size_t Bytes){xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)(Qos<<1);P.Identifier=Id;P.Body.Publish.Topic=XRT_STR_LITERAL("qos/two");unsigned char* Data=Bytes?malloc(Bytes):NULL;if(Bytes){check(Data!=NULL,"payload");memset(Data,'x',Bytes);P.Body.Publish.Payload=(xbytesview){Data,Bytes};}xfuture* F=NULL;check(xmqttClientPublishAsync(C,&P,Timeout,&F)==XMQTT_ADMISSION_ACCEPTED,"publication admission");free(Data);return F;}
int main(int Argc,char** Argv){if(Argc!=4)return 2;unsigned Port=(unsigned)strtoul(Argv[1],NULL,10),Version=(unsigned)strtoul(Argv[2],NULL,10);const char* Mode=Argv[3];
    bool Window=!strcmp(Mode,"window"),NegativeWindow=!strcmp(Mode,"negative_window"),Partial=!strcmp(Mode,"partial_publish"),Tiny=Partial || !strcmp(Mode,"byte") || !strcmp(Mode,"partial_repeat");
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&E);check(Engine && xrtNetEngineStart(Engine),"Engine");xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=(xmqttversion)Version;
    if(Tiny){K.WriteBytes=K.WriteChunkBytes=1;K.Stream.WriteLimit=1;K.Stream.WriteHighWater=1;K.Stream.WriteLowWater=0;}
    if(!strcmp(Mode,"metadata_rec"))K.MaxObjects=3;
    if(!strcmp(Mode,"metadata_comp"))K.MaxObjects=4;
    xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.CleanStart=true;Connect.Body.Connect.ClientId=XRT_STR_LITERAL("qos2-probe");xmqttproperty PI={XMQTT_PROPERTY_REQUEST_PROBLEM_INFO,XMQTT_PROPERTY_BYTE,{0}};if(!strcmp(Mode,"problem"))Connect.Properties=(xmqttpropertylist){&PI,1};
    xnetaddr A;xrtNetAddrParse(&A,"127.0.0.1",(uint16)Port);xmqttclient* C=NULL;check(xmqttClientConnect(Engine,&A,1,&K,&Connect,&C),"factory");xfuture* Ready=xmqttClientReady(C);check(((const xmqttreadyresult*)result(Ready))->Status==XMQTT_STATUS_OK,"CONNACK");
    xfuture* F=publish(C,2,7,!strncmp(Mode,"timeout_",8)?100:5000,Partial?32768:0),*Second=NULL,*Zero=NULL,*Closed=NULL;
    if(Window || NegativeWindow){Second=publish(C,2,8,5000,0);if(Window)Zero=publish(C,0,0,5000,0);}
    if(!strcmp(Mode,"graceful"))check(xmqttClientDisconnectAsync(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"graceful drains QoS2");
    if(!strcmp(Mode,"cancel_comp") || !strcmp(Mode,"close_comp")){double End=xrtTimer()+5;xmqttclientinfo I;do{xmqttClientInfo(C,&I);check(xrtTimer()<End,"phase marker");if(!I.Received)xrtSleep(1);}while(!I.Received);xmqttmessage* M=NULL;check(xmqttClientTakeMessage(C,&M),"phase marker owner");xmqttMessageRelease(M);
        if(!strcmp(Mode,"cancel_comp"))check(xrtFutureCancel(F),"phase cancellation");else check(xmqttClientClose(C),"phase close");}
    const xmqttwriteresult* R=result(F);xmqttclientstatus Expected=!strcmp(Mode,"reject_rec") || !strcmp(Mode,"reject_comp") || NegativeWindow || !strcmp(Mode,"changed_negative")?XMQTT_STATUS_REJECTED:!strncmp(Mode,"timeout_",8)?XMQTT_STATUS_TIMEOUT:!strcmp(Mode,"cancel_comp")?XMQTT_STATUS_CANCELLED:!strcmp(Mode,"close_comp")?XMQTT_STATUS_CLOSED:!strncmp(Mode,"metadata_",9)?XMQTT_STATUS_LIMIT:Partial || !strcmp(Mode,"wrong_id") || !strcmp(Mode,"wrong_kind") || !strcmp(Mode,"problem")?XMQTT_STATUS_PROTOCOL:XMQTT_STATUS_OK;
    if(R->Status!=Expected)fprintf(stderr,"mode=%s status=%d rec=%u comp=%u reason=%u bytes=%zu\n",Mode,R->Status,R->PubrecReceived?1u:0u,R->PubcompReceived?1u:0u,R->Reason,R->AcceptedBytes);
    check(R->Status==Expected && R->Identifier==7 && !R->Puback,"first sender outcome");
    if(Expected==XMQTT_STATUS_OK)check(R->PubrecReceived && R->PubcompReceived && R->Pubrec && R->Pubcomp && R->Delivery==XMQTT_PEER_ACCEPTED,"full handshake only after PUBCOMP");
    if(!strcmp(Mode,"reject_rec") || NegativeWindow || !strcmp(Mode,"changed_negative"))check(R->PubrecReason==0x87 && R->Reason==0x87 && R->PubrecReceived && !R->PubcompReceived && R->Delivery==XMQTT_PEER_REJECTED,"negative receipt ends exchange");
    if(!strcmp(Mode,"reject_comp"))check(R->PubrecReason==0 && R->Reason==0x92 && R->PubcompReceived,"negative completion after receipt");
    if(!strcmp(Mode,"metadata_rec"))check(R->PubrecReceived && !R->Pubrec && !R->PubcompReceived && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"scalar receipt survives metadata quota");
    if(!strcmp(Mode,"metadata_comp"))check(R->Pubrec && R->PubcompReceived && !R->Pubcomp && R->Delivery==XMQTT_PEER_ACCEPTED,"scalar completion survives metadata quota");
    if(!strcmp(Mode,"timeout_comp") || !strcmp(Mode,"cancel_comp") || !strcmp(Mode,"close_comp"))check(R->PubrecReceived && !R->PubcompReceived && R->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,"receipt is not completion");
    if(Second)check(result(Second)->Status==XMQTT_STATUS_OK,"following flow after credit release");
    if(Zero)check(result(Zero)->Status==XMQTT_STATUS_OK,"QoS0 bypasses exhausted credit");
    if(!strcmp(Mode,"late_comp")){/* Same explicit id may reopen only after every real prior native lease. */
        double End=xrtTimer()+5;xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=4;P.Identifier=7;P.Body.Publish.Topic=XRT_STR_LITERAL("qos/two");
        for(;;){xmqttclientadmission S=xmqttClientPublishAsync(C,&P,5000,&Second);if(S==XMQTT_ADMISSION_ACCEPTED)break;check(S==XMQTT_ADMISSION_OVERLOADED && xrtTimer()<End,"physical identifier retirement");xrtThreadYield();}check(result(Second)->Status==XMQTT_STATUS_OK,"same id reused without old ACK completing new flow");}
    if(!strcmp(Mode,"partial_repeat"))check(xmqttClientDisconnectAsync(C,NULL,5000,&Closed)==XMQTT_ADMISSION_ACCEPTED,"flush repeated partial release before final packet");
    if(Closed)check(result(Closed)->Status==XMQTT_STATUS_OK,"graceful completion");else check(xmqttClientClose(C),"final cleanup close");check(xmqttClientWaitFor(C,7000)==XWAIT_OK,"physical retirement");xmqttclientinfo I;xmqttClientInfo(C,&I);check(!I.Requests && !I.OutgoingBytes && I.Accepted==I.Completed,"retired accounting");
    printf("{\"case\":\"%s\",\"version\":%u,\"status\":%d,\"rec\":%u,\"comp\":%u,\"bytes\":%zu}\n",Mode,Version,R->Status,R->PubrecReceived?1u:0u,R->PubcompReceived?1u:0u,R->AcceptedBytes);
    check(xmqttClientDestroy(C) && xrtNetEngineStop(Engine) && xrtNetEngineDestroy(Engine),"destruction");if(R->Pubrec)check(xmqttMessagePacket(R->Pubrec)->Type==XMQTT_PACKET_PUBREC,"retained receipt packet");if(R->Pubcomp)check(xmqttMessagePacket(R->Pubcomp)->Type==XMQTT_PACKET_PUBCOMP,"retained completion packet");if(!strcmp(Mode,"props")){const xmqttpacket* Packets[2]={xmqttMessagePacket(R->Pubrec),xmqttMessagePacket(R->Pubcomp)};const char* Texts[2]={"received","completed"};for(size_t J=0;J<2;++J){const xmqttpropertylist* L=&Packets[J]->Properties;check(L->Count==2 && L->Data[0].Id==XMQTT_PROPERTY_REASON_STRING && L->Data[0].Value.Text.Size==strlen(Texts[J]) && !memcmp(L->Data[0].Value.Text.Data,Texts[J],strlen(Texts[J])) && L->Data[1].Id==XMQTT_PROPERTY_USER && L->Data[1].Value.Pair.Name.Size==1 && L->Data[1].Value.Pair.Name.Data[0]=='k' && L->Data[1].Value.Pair.Value.Size==1 && L->Data[1].Value.Pair.Value.Data[0]=='v',"retained owned phase properties");}}xrtFutureDestroy(Closed);xrtFutureDestroy(Second);xrtFutureDestroy(Zero);xrtFutureDestroy(F);xrtFutureDestroy(Ready);return 0;
}
