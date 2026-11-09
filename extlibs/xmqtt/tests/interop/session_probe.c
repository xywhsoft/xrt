#include <xmqtt.h>
#include <xrt/net.h>
#include <xrt/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Test-only synchronous socket driver, not a new product transport/runtime. */
static xnetsocket Socket;static double Deadline;static unsigned char Input[512];static xmqttproperty Props[8];static xmqttversion Version;
static void spCheck(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] session TCP probe: %s (%s)\n",Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static void spSend(cbytes P,size_t N)
{
    while(N){size_t W=0;xnetresult R=xrtNetSocketSend(Socket,P,N,&W);spCheck(R==XNET_RESULT_OK || R==XNET_RESULT_AGAIN,"socket send");P+=W;N-=W;if(N){spCheck(xrtTimer()<Deadline,"send deadline");if(!W)xrtSleep(1);}}
}
static void spRead(bytes P,size_t N)
{
    while(N){size_t W=0;xnetresult R=xrtNetSocketRecv(Socket,P,N,&W);spCheck(R==XNET_RESULT_OK || R==XNET_RESULT_AGAIN,"socket read");P+=W;N-=W;if(N){spCheck(xrtTimer()<Deadline,"read deadline");if(!W)xrtSleep(1);}}
}
static void spEOF(void)
{
    for(;;){unsigned char B;size_t N=0;xnetresult R=xrtNetSocketRecv(Socket,&B,1,&N);if(R==XNET_RESULT_CLOSED)return;spCheck(R==XNET_RESULT_AGAIN && !N && xrtTimer()<Deadline,"peer EOF after first receive PUBREC");xrtSleep(1);}
}
static xmqttpacket spPacket(void)
{
    size_t At=1,N=0,Shift=0;spRead(Input,1);unsigned char B;
    do{spCheck(At<5,"bounded header");spRead(&B,1);Input[At++]=B;N|=(size_t)(B&127u)<<Shift;Shift+=7;}while(B&128u);
    spCheck(N<=sizeof(Input)-At,"bounded input");spRead(Input+At,N);xmqttcodecbuffers Buffers={Props,8,NULL,0};xmqttpacket P;
    spCheck((Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){Input,At+N},XMQTT_SERVER_TO_CLIENT,NULL,&Buffers,&P,NULL):xmqttPacketRead311((xbytesview){Input,At+N},XMQTT_SERVER_TO_CLIENT,NULL,&Buffers,&P,NULL))==XMQTT_CODEC_DONE,"full independent peer packet validation");return P;
}
static void spControl(xmqttpackettype Type,uint16 Id,uint8 Reason)
{
    xmqttpacket P;xmqttPacketInit(Type,&P);P.Identifier=Id;P.Reason=Reason;unsigned char B[32];size_t N=0;
    spCheck(Version==XMQTT_VERSION_5?xmqttPacketWrite5(B,sizeof(B),&P,XMQTT_CLIENT_TO_SERVER,NULL,&N):xmqttPacketWrite311(B,sizeof(B),&P,XMQTT_CLIENT_TO_SERVER,NULL,&N),"control encoding");spSend(B,N);
}
static void spFrame(xmqttsession* S,xmqttsessionframe* F,bool Complete)
{
    spSend(F->Prefix.Data,F->Prefix.Size);spSend(F->Tail.Data,F->Tail.Size);
    if(Complete){spCheck(xmqttSessionSent(S,F)==XMQTT_SESSION_DONE,"actual complete socket frame");xmqttSessionPacketRelease(F->Packet);F->Packet=NULL;}
}
int main(int Argc,char** Argv)
{
    if(Argc!=4)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);unsigned V=(unsigned)strtoul(Argv[2],NULL,10),Scenario=(unsigned)strtoul(Argv[3],NULL,10);
    if(!Port || Port>65535 || (V!=5 && V!=311) || Scenario>5)return 2;
    Version=V==5?XMQTT_VERSION_5:XMQTT_VERSION_311;xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.Version=Version;K.ClientId=XRT_STR_LITERAL("session-wire");xmqttsession* S=NULL;spCheck(xmqttSessionCreate(&K,&S),"session create");
    const xmqttsessionpacket* Held=NULL;xmqttsessionframe Old={0};unsigned Delivered=0;
    for(unsigned Round=0;Round<2;++Round){Deadline=xrtTimer()+10;xnetaddr Address;xrtNetAddrLoopback(&Address,XNET_FAMILY_IPV4,(uint16)Port);Socket=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);
        spCheck(Socket && xrtNetSocketConnect(Socket,&Address)==XNET_RESULT_OK && xrtNetSocketSet(Socket,XNET_OPTION_NONBLOCK,1) && xrtNetSocketSet(Socket,XNET_OPTION_NO_DELAY,1),"loopback connection");
        xmqttpacket Connect;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=K.ClientId;xmqttproperty E={0};E.Id=XMQTT_PROPERTY_SESSION_EXPIRY;E.Type=XMQTT_PROPERTY_UINT32;E.Value.Number=60;
        if(V==5)Connect.Properties=(xmqttpropertylist){&E,1};
        unsigned char Wire[128];size_t Bytes=0;spCheck(V==5?xmqttPacketWrite5(Wire,sizeof(Wire),&Connect,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes):xmqttPacketWrite311(Wire,sizeof(Wire),&Connect,XMQTT_CLIENT_TO_SERVER,NULL,&Bytes),"CONNECT encoding");spSend(Wire,Bytes);
        xmqttpacket Ack=spPacket();xmqttnegotiated N;xmqttsessionstatus Open=xmqttSessionOpen(S,&Connect,&Ack,xrtTimer(),&N);
        if(Round && Scenario==5){spCheck(Open==(V==5?XMQTT_SESSION_RESET:XMQTT_SESSION_STATE_ERROR),"missing server session policy");if(V==5){spControl(XMQTT_PACKET_DISCONNECT,0,0);spCheck(xrtNetSocketClose(Socket),"reset close");Socket=NULL;spCheck(xmqttSessionEnd(S,NULL,xrtTimer())==XMQTT_SESSION_DONE,"reset network end");}else {spCheck(xrtNetSocketClose(Socket),"mismatch close");Socket=NULL;xrtClearError();}break;}
        spCheck(Open==XMQTT_SESSION_DONE && N.SessionPresent==(Round!=0),"fresh/resumed negotiation");
        if(Scenario==4){
            xmqttpacket P=spPacket();spCheck(P.Type==XMQTT_PACKET_PUBLISH && P.Identifier==321 && P.Body.Publish.Payload.Size==3 && !memcmp(P.Body.Publish.Payload.Data,"r\0x",3),"incoming binary QoS2");
            xmqttsessionstatus Received=xmqttSessionReceivePrepare(S,P.Identifier);spCheck(Received==(Round?XMQTT_SESSION_DUPLICATE:XMQTT_SESSION_DONE),"cross-connection receive suppression");
            if(!Round){spCheck(xmqttSessionReceiveCommit(S,P.Identifier)==XMQTT_SESSION_DONE,"local delivery commit");++Delivered;}spControl(XMQTT_PACKET_PUBREC,321,0);
            if(!Round)spEOF();else {for(unsigned Repeat=0;Repeat<2;++Repeat){P=spPacket();spCheck(P.Type==XMQTT_PACKET_PUBREL && P.Identifier==321,"incoming PUBREL");uint8 Reason=255;spCheck(xmqttSessionReceiveRelease(S,321,&Reason)==(Repeat?XMQTT_SESSION_EMPTY:XMQTT_SESSION_DONE),"known/unknown PUBREL state");spControl(XMQTT_PACKET_PUBCOMP,321,Reason);if(!Repeat)spCheck(xmqttSessionReceiveComplete(S,321)==XMQTT_SESSION_DONE,"PUBCOMP actually emitted");}}
        }else {
            if(!Round){xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=Scenario==0 || Scenario==5?2:4;P.Identifier=31;P.Body.Publish.Topic=XRT_STR_LITERAL("wire/session");P.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");spCheck(xmqttSessionPublish(S,&P,&Held)==XMQTT_SESSION_DONE,"immutable outgoing");}
            xmqttsessionframe F;spCheck(xmqttSessionNext(S,&F)==XMQTT_SESSION_DONE,"required first/retry frame");
            if(!Round && Scenario==2){spSend(F.Prefix.Data,F.Prefix.Size);spSend(F.Tail.Data,6);Old=F;}
            else {spFrame(S,&F,true);
                if(!Round && (Scenario==1 || Scenario==3)){Ack=spPacket();spCheck(xmqttSessionAcknowledge(S,&Ack)==XMQTT_SESSION_DONE && Ack.Type==XMQTT_PACKET_PUBREC,"first PUBREC");if(Scenario==3){spCheck(xmqttSessionNext(S,&Old)==XMQTT_SESSION_DONE,"first PUBREL plan");spSend(Old.Prefix.Data,1);}}
                if(Round){Ack=spPacket();spCheck(xmqttSessionAcknowledge(S,&Ack)==XMQTT_SESSION_DONE,"resumed acknowledgement");if(Scenario==2){spCheck(Ack.Type==XMQTT_PACKET_PUBREC && xmqttSessionNext(S,&F)==XMQTT_SESSION_DONE,"resumed QoS2 transition");spFrame(S,&F,true);Ack=spPacket();spCheck(xmqttSessionAcknowledge(S,&Ack)==XMQTT_SESSION_DONE && Ack.Type==XMQTT_PACKET_PUBCOMP,"resumed QoS2 completion");}}}
        }
        if(Round)spControl(XMQTT_PACKET_DISCONNECT,0,0);
        spCheck(xrtNetSocketClose(Socket),"actual socket close");Socket=NULL;spCheck(xmqttSessionEnd(S,NULL,xrtTimer())==XMQTT_SESSION_DONE,"terminated network boundary");
    }
    if(Scenario==4)spCheck(Delivered==1,"one volatile local delivery across reconnect");
    spCheck(xmqttSessionDestroy(S),"session unique owner destruction");if(Held)spCheck(xmqttSessionPacket(Held)->Body.Publish.Payload.Size==3,"retained payload beyond session");
    if(Old.Packet){spCheck(Old.Prefix.Data[0]==(Scenario==3?0x62:0x34),"immutable old partial frame");xmqttSessionPacketRelease(Old.Packet);}xmqttSessionPacketRelease(Held);
    puts("[PASS] public session state over two actual TCP connections; literal-peer replay/dedup/reset and physical retained lifetime");return 0;
}
