#include "test.h"
#include <math.h>
#include <xrt/thread.h>
static void sessionStatus(xmqttsessionstatus Actual,xmqttsessionstatus Expected,cstr Text)
{
    if(Actual!=Expected)fprintf(stderr,"session status %d expected %d: %s (%s)\n",Actual,Expected,Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"no cause");
    testRequire(Actual==Expected,Text);
}
static xmqttproperty sessionNumber(xmqttpropertyid Id,uint32 Value)
{xmqttproperty P={0};P.Id=Id;P.Type=Id==XMQTT_PROPERTY_RECEIVE_MAXIMUM || Id==XMQTT_PROPERTY_TOPIC_ALIAS?XMQTT_PROPERTY_UINT16:Id==XMQTT_PROPERTY_MAXIMUM_QOS?XMQTT_PROPERTY_BYTE:XMQTT_PROPERTY_UINT32;P.Value.Number=Value;return P;}
static xmqttsession* sessionCreate(xmqttversion V,size_t Out,size_t In,size_t Bytes)
{
    xmqttsessionconfig C;testRequire(xmqttSessionConfigInit(&C),"session config");C.Version=V;C.ClientId=XRT_STR_LITERAL("session-test");C.MaxOutgoing=Out;C.MaxIncoming=In;C.MaxBytes=Bytes;
    xmqttsession* S=NULL;testRequire(xmqttSessionCreate(&C,&S) && S,"session create");return S;
}
static xmqttsessioninfo sessionInfo(xmqttsession* S)
{xmqttsessioninfo I;testRequire(xmqttSessionInfo(S,&I),"session snapshot");return I;}
static bool sessionSameInfo(const xmqttsessioninfo* A,const xmqttsessioninfo* B)
{
    return A->Size==B->Size && A->Version==B->Version && A->Connected==B->Connected && A->HasState==B->HasState &&
        A->ActiveOutgoing==B->ActiveOutgoing && A->Objects==B->Objects && A->LiveBytes==B->LiveBytes && A->BaseBytes==B->BaseBytes &&
        A->Incoming==B->Incoming && A->Prepared==B->Prepared && A->SendQuota==B->SendQuota && A->InitialSendQuota==B->InitialSendQuota &&
        A->Generation==B->Generation && A->ExpiresAt==B->ExpiresAt;
}
static xmqttsessionpacketinfo sessionPacketInfo(const xmqttsessionpacket* P)
{xmqttsessionpacketinfo I;testRequire(xmqttSessionPacketInfo(P,&I),"packet snapshot");return I;}
static xmqttsessionstatus sessionOpen(xmqttsession* S,xmqttversion V,bool Clean,bool Present,uint32 Expiry,uint16 Quota,double Now)
{
    xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=XRT_STR_LITERAL("session-test");C.Body.Connect.CleanStart=Clean;A.SessionPresent=Present;
    xmqttproperty E=sessionNumber(XMQTT_PROPERTY_SESSION_EXPIRY,Expiry),Q=sessionNumber(XMQTT_PROPERTY_RECEIVE_MAXIMUM,Quota);
    if(V==XMQTT_VERSION_5){C.Properties=(xmqttpropertylist){&E,1};A.Properties=(xmqttpropertylist){&Q,1};}
    xmqttnegotiated N;return xmqttSessionOpen(S,&C,&A,Now,&N);
}
static const xmqttsessionpacket* sessionPublish(xmqttsession* S,unsigned Qos,uint16 Id)
{
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)(Qos<<1);P.Identifier=Id;P.Body.Publish.Topic=XRT_STR_LITERAL("session/data");P.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");
    const xmqttsessionpacket* R=NULL;sessionStatus(xmqttSessionPublish(S,&P,&R),XMQTT_SESSION_DONE,"publish admission");return R;
}
static xmqttsessionframe sessionNext(xmqttsession* S)
{xmqttsessionframe F;sessionStatus(xmqttSessionNext(S,&F),XMQTT_SESSION_DONE,"next frame");return F;}
static void sessionSent(xmqttsession* S,xmqttsessionframe* F)
{sessionStatus(xmqttSessionSent(S,F),XMQTT_SESSION_DONE,"full frame emitted");xmqttSessionPacketRelease(F->Packet);F->Packet=NULL;}
static xmqttsessionstatus sessionAck(xmqttsession* S,xmqttpackettype Type,uint16 Id,uint8 Reason)
{xmqttpacket A;xmqttPacketInit(Type,&A);A.Identifier=Id;A.Reason=Reason;return xmqttSessionAcknowledge(S,&A);}
static void sessionEnd(xmqttsession* S,double Now)
{sessionStatus(xmqttSessionEnd(S,NULL,Now),XMQTT_SESSION_DONE,"network ended");}
static void sessionEmpty(xmqttsession* S)
{xmqttsessionframe F,Old;memset(&F,0xa5,sizeof(F));Old=F;sessionStatus(xmqttSessionNext(S,&F),XMQTT_SESSION_EMPTY,"no frame or no quota");testRequire(!memcmp(&F,&Old,sizeof(F)),"empty output changed");}
static void sessionWire(xmqttsessionframe* F,xmqttversion V,uint8 Flags,xmqttpackettype Type,uint16 Id)
{
    unsigned char Wire[4096];size_t N=F->Prefix.Size+F->Tail.Size;testRequire(N<=sizeof(Wire),"frame fixture capacity");memcpy(Wire,F->Prefix.Data,F->Prefix.Size);if(F->Tail.Size)memcpy(Wire+F->Prefix.Size,F->Tail.Data,F->Tail.Size);
    xmqttproperty Props[8];xmqttcodecbuffers B={Props,8,NULL,0};xmqttpacket P;
    testRequire((V==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){Wire,N},XMQTT_CLIENT_TO_SERVER,NULL,&B,&P,NULL):xmqttPacketRead311((xbytesview){Wire,N},XMQTT_CLIENT_TO_SERVER,NULL,&B,&P,NULL))==XMQTT_CODEC_DONE && P.Flags==Flags && P.Type==Type && P.Identifier==Id,"decoded emitted flags/type/original id");
    if(Type==XMQTT_PACKET_PUBLISH)testRequire(P.Body.Publish.Payload.Size==3 && !memcmp(P.Body.Publish.Payload.Data,"x\0z",3),"immutable replay payload");
}
static void testSessionFlows(xmqttversion V)
{
    xmqttsession* S=sessionCreate(V,4,4,1024*1024);sessionStatus(sessionOpen(S,V,false,false,60,2,1),XMQTT_SESSION_DONE,"fresh persistent session");
    const xmqttsessionpacket *One=sessionPublish(S,1,1),*Two=sessionPublish(S,2,65535);xmqttsessionframe F=sessionNext(S);sessionWire(&F,V,2,XMQTT_PACKET_PUBLISH,1);sessionEmpty(S);
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,1,0),XMQTT_SESSION_PROTOCOL_ERROR,"ACK before frame emission");xrtClearError();sessionSent(S,&F);
    F=sessionNext(S);sessionWire(&F,V,4,XMQTT_PACKET_PUBLISH,65535);sessionSent(S,&F);sessionEmpty(S);
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,65535,0),XMQTT_SESSION_DONE,"PUBREC stage transition");F=sessionNext(S);sessionWire(&F,V,2,XMQTT_PACKET_PUBREL,65535);
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,65535,0),XMQTT_SESSION_DUPLICATE,"coalesced duplicate PUBREC during selected PUBREL");sessionSent(S,&F);F=sessionNext(S);sessionWire(&F,V,2,XMQTT_PACKET_PUBREL,65535);sessionSent(S,&F);
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,65535,0),XMQTT_SESSION_DONE,"QoS2 final completion");sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,65535,0),XMQTT_SESSION_DUPLICATE,"late PUBCOMP creates no credit");
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,1,V==XMQTT_VERSION_5?0x87:0),V==XMQTT_VERSION_5?XMQTT_SESSION_REJECTED:XMQTT_SESSION_DONE,"QoS1 final decision");
    xmqttsessioninfo I=sessionInfo(S);testRequire(!I.ActiveOutgoing && I.Objects==2 && I.SendQuota==I.InitialSendQuota && sessionPacketInfo(Two).PubrecReceived,"held completed objects/private byte reservation");
    xmqttpacket P=*xmqttSessionPacket(One);const xmqttsessionpacket* Out=(const xmqttsessionpacket*)(uintptr_t)1;
    sessionStatus(xmqttSessionPublish(S,&P,&Out),XMQTT_SESSION_LIMIT,"id held through actual references");testRequire(Out==(const xmqttsessionpacket*)(uintptr_t)1,"collision output changed");
    xmqttSessionPacketRelease(One);One=sessionPublish(S,1,1);xmqttSessionPacketRelease(One);xmqttSessionPacketRelease(Two);sessionEnd(S,2);sessionStatus(xmqttSessionReset(S),XMQTT_SESSION_DONE,"offline administrative termination");I=sessionInfo(S);testRequire(!I.Objects && I.LiveBytes==I.BaseBytes,"all packet resources refunded");testRequire(xmqttSessionDestroy(S),"destroy flows");
}
static void testSessionReplay(xmqttversion V)
{
    for(unsigned Cut=0;Cut<6;++Cut){
        xmqttsession* S=sessionCreate(V,8,4,1024*1024);sessionStatus(sessionOpen(S,V,false,false,60,1,1),XMQTT_SESSION_DONE,"replay initial open");
        const xmqttsessionpacket* P=sessionPublish(S,2,71);xmqttsessionframe Old={0};
        if(Cut){Old=sessionNext(S);if(Cut>1)sessionSent(S,&Old);}
        if(Cut>2){sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,71,0),XMQTT_SESSION_DONE,"pre-close PUBREC");if(Cut>3){Old=sessionNext(S);if(Cut>4)sessionSent(S,&Old);}}
        if(Cut!=0 && Cut!=3)sessionEmpty(S);
        sessionEnd(S,2);sessionStatus(sessionOpen(S,V,false,true,60,1,3),XMQTT_SESSION_DONE,"resume consistent session");xmqttsessionframe F=sessionNext(S);
        sessionWire(&F,V,Cut>2?2:Cut?12:4,Cut>2?XMQTT_PACKET_PUBREL:XMQTT_PACKET_PUBLISH,71);
        if(Old.Packet){sessionStatus(xmqttSessionSent(S,&Old),XMQTT_SESSION_STATE_ERROR,"old generation token cannot finish new frame");xrtClearError();testRequire(Old.Prefix.Data[0]==(Cut>3?0x62:0x34),"old frame bytes changed after reconnect");xmqttSessionPacketRelease(Old.Packet);}
        sessionSent(S,&F);
        if(Cut<3){sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,71,0),XMQTT_SESSION_DONE,"resumed PUBLISH PUBREC");F=sessionNext(S);sessionSent(S,&F);}
        sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,71,0),XMQTT_SESSION_DONE,"resumed PUBCOMP");xmqttsessioninfo I=sessionInfo(S);testRequire(sessionPacketInfo(P).Stage==XMQTT_SESSION_COMPLETE && I.SendQuota==I.InitialSendQuota,"resume complete/credit cap");sessionEmpty(S);xmqttSessionPacketRelease(P);sessionEnd(S,4);testRequire(xmqttSessionDestroy(S),"destroy replay");
    }
}
static void testSessionIncoming(xmqttversion V)
{
    xmqttsession* S=sessionCreate(V,0,2,1024*1024);sessionStatus(sessionOpen(S,V,false,false,60,1,1),XMQTT_SESSION_DONE,"receive session");
    sessionStatus(xmqttSessionReceivePrepare(S,9),XMQTT_SESSION_DONE,"reserve incoming");sessionStatus(xmqttSessionReceivePrepare(S,9),XMQTT_SESSION_STATE_ERROR,"uncommitted receive is busy");xrtClearError();
    sessionStatus(xmqttSessionReceivePrepare(S,10),XMQTT_SESSION_DONE,"second reservation");sessionStatus(xmqttSessionReceivePrepare(S,11),XMQTT_SESSION_LIMIT,"prepared credit bounded");
    sessionStatus(xmqttSessionReceiveAbort(S,10),XMQTT_SESSION_DONE,"rollback before local delivery");sessionStatus(xmqttSessionReceiveCommit(S,9),XMQTT_SESSION_DONE,"local queue commit before PUBREC");
    sessionStatus(xmqttSessionReceivePrepare(S,9),XMQTT_SESSION_DUPLICATE,"same id no duplicate app delivery");sessionStatus(xmqttSessionReceiveAbort(S,9),XMQTT_SESSION_STATE_ERROR,"cannot roll back accepted message");xrtClearError();
    uint8 Reason=0xa5;sessionStatus(xmqttSessionReceiveRelease(S,9,&Reason),XMQTT_SESSION_DONE,"PUBREL stage");testRequire(!Reason,"known PUBCOMP reason");
    sessionStatus(xmqttSessionReceivePrepare(S,10),XMQTT_SESSION_DONE,"pending receive at transport loss");sessionEnd(S,2);testRequire(sessionInfo(S).Incoming==1 && !sessionInfo(S).Prepared,"only uncommitted input rolled back");
    sessionStatus(sessionOpen(S,V,false,true,60,1,3),XMQTT_SESSION_DONE,"restore incoming state");sessionStatus(xmqttSessionReceivePrepare(S,9),XMQTT_SESSION_DUPLICATE,"restored released id still suppresses delivery");
    sessionStatus(xmqttSessionReceiveRelease(S,9,&Reason),XMQTT_SESSION_DONE,"repeated PUBREL after reconnect");sessionStatus(xmqttSessionReceiveComplete(S,9),XMQTT_SESSION_DONE,"complete PUBCOMP emission");
    Reason=0xa5;sessionStatus(xmqttSessionReceiveRelease(S,9,&Reason),XMQTT_SESSION_EMPTY,"unknown/repeated PUBREL still answered");testRequire(Reason==(V==XMQTT_VERSION_5?0x92:0),"unknown PUBCOMP version reason");
    sessionStatus(xmqttSessionReceivePrepare(S,9),XMQTT_SESSION_DONE,"id reuse is new delivery after PUBCOMP");sessionStatus(xmqttSessionReceiveAbort(S,9),XMQTT_SESSION_DONE,"second rollback");sessionEnd(S,4);testRequire(xmqttSessionDestroy(S),"receive destroy");
}
static void testSessionSnapshot(void)
{
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,4,0,1024*1024);sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,1,0),XMQTT_SESSION_DONE,"snapshot open");
    char Topic[]="owned/topic",Name[]="tag",Value[]="value";unsigned char Data[]={'x',0,'z'};xmqttproperty Props[3]={sessionNumber(XMQTT_PROPERTY_TOPIC_ALIAS,7),{0},{0}};
    Props[1].Id=Props[2].Id=XMQTT_PROPERTY_USER;Props[1].Type=Props[2].Type=XMQTT_PROPERTY_STRING_PAIR;Props[1].Value.Pair=Props[2].Value.Pair=(xmqttstringpair){{Name,3},{Value,5}};
    xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=2;P.Identifier=4;P.Properties=(xmqttpropertylist){Props,3};P.Body.Publish.Topic=(xstrview){Topic,11};P.Body.Publish.Payload=(xbytesview){Data,3};
    const xmqttsessionpacket* R=NULL;sessionStatus(xmqttSessionPublish(S,&P,&R),XMQTT_SESSION_DONE,"owned alias-free snapshot");memset(Topic,'?',11);memset(Data,'?',3);memset(Name,'?',3);memset(Value,'?',5);memset(Props,0,sizeof(Props));
    const xmqttpacket* Q=xmqttSessionPacket(R);testRequire(Q->Properties.Count==2 && Q->Properties.Data[0].Id==XMQTT_PROPERTY_USER && Q->Body.Publish.Topic.Size==11 && !memcmp(Q->Body.Publish.Topic.Data,"owned/topic",11) && !memcmp(Q->Body.Publish.Payload.Data,"x\0z",3) && !memcmp(Q->Properties.Data[1].Value.Pair.Value.Data,"value",5),"snapshot input lifetime and repeated properties/alias removal");
    xmqttsessionframe F=sessionNext(S);sessionWire(&F,XMQTT_VERSION_5,2,XMQTT_PACKET_PUBLISH,4);sessionEnd(S,1);testRequire(xmqttSessionDestroy(S),"destroy with packet and selected native view held");
    testRequire(xmqttSessionPacket(R)->Properties.Count==2 && F.Prefix.Data[0]==0x32 && sessionPacketInfo(R).Stage==XMQTT_SESSION_DISCARDED,"views survive session owner destruction");xmqttSessionPacketRelease(F.Packet);xmqttSessionPacketRelease(R);
}
static void testSessionIdentityExpiry(xmqttversion V)
{
    xmqttsession* S=sessionCreate(V,4,4,1024*1024);
    sessionStatus(sessionOpen(S,V,false,true,60,1,1),XMQTT_SESSION_STATE_ERROR,"unowned server session");xrtClearError();
    sessionStatus(sessionOpen(S,V,false,false,1,1,1),XMQTT_SESSION_DONE,"expiry initial session");const xmqttsessionpacket* P=sessionPublish(S,1,2);
    sessionStatus(xmqttSessionExpire(S,2),XMQTT_SESSION_STATE_ERROR,"never expire connected state");sessionStatus(xmqttSessionReset(S),XMQTT_SESSION_STATE_ERROR,"never reset connected state");testRequire(!xmqttSessionDestroy(S),"never destroy connected state");xrtClearError();sessionEnd(S,2);
    if(V==XMQTT_VERSION_5){
        sessionStatus(xmqttSessionExpire(S,2.999),XMQTT_SESSION_EMPTY,"expiry not early");sessionStatus(xmqttSessionExpire(S,3),XMQTT_SESSION_DONE,"exact expiry boundary");
        sessionStatus(sessionOpen(S,V,false,true,1,1,4),XMQTT_SESSION_STATE_ERROR,"expired local state cannot resume peer");xrtClearError();testRequire(!sessionPacketInfo(P).Active && sessionInfo(S).Objects==1,"expired but physically held reservation");
        sessionStatus(sessionOpen(S,V,false,false,60,1,4),XMQTT_SESSION_DONE,"fresh state after expiry");
    }else {sessionStatus(sessionOpen(S,V,false,false,0,1,3),XMQTT_SESSION_STATE_ERROR,"311 lost-server policy preserves old state");xrtClearError();testRequire(sessionPacketInfo(P).Active,"311 mismatch did not silently discard");sessionStatus(xmqttSessionReset(S),XMQTT_SESSION_DONE,"explicit 311 reset");sessionStatus(sessionOpen(S,V,true,false,0,1,4),XMQTT_SESSION_DONE,"311 explicit clean session");}
    xmqttSessionPacketRelease(P);sessionEnd(S,5);testRequire(xmqttSessionDestroy(S),"expiry destroy");
    if(V==XMQTT_VERSION_5){S=sessionCreate(V,4,0,1024*1024);sessionStatus(sessionOpen(S,V,false,false,60,1,1),XMQTT_SESSION_DONE,"missing-server initial");P=sessionPublish(S,1,5);sessionEnd(S,2);sessionStatus(sessionOpen(S,V,false,false,60,1,3),XMQTT_SESSION_RESET,"MQTT5 missing peer discards local obligations");testRequire(sessionPacketInfo(P).Stage==XMQTT_SESSION_DISCARDED && !sessionInfo(S).ActiveOutgoing,"reset observable on held packet");xmqttSessionPacketRelease(P);sessionEnd(S,4);testRequire(xmqttSessionDestroy(S),"reset destroy");}
}
static void testSessionBoundsAndArguments(void)
{
    xmqttsessionconfig C;testRequire(xmqttSessionConfigInit(&C) && C.MaxOutgoing==128 && C.MaxIncoming==64 && C.MaxBytes==16u*1024u*1024u,"finite defaults");
    xmqttsession* Bad=(xmqttsession*)(uintptr_t)1;testRequire(!xmqttSessionCreate(&C,&Bad) && Bad==(xmqttsession*)(uintptr_t)1,"empty identity output atomicity");C.ClientId=XRT_STR_LITERAL("session-test");C.MaxOutgoing=65536;testRequire(!xmqttSessionCreate(&C,&Bad),"bounded identifier count");C.MaxOutgoing=1;C.MaxIncoming=0;C.MaxBytes=1;testRequire(!xmqttSessionCreate(&C,&Bad),"base bytes charged");xrtClearError();
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,1,0,1024*1024);xmqttsessioninfo Initial=sessionInfo(S);sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,1,1),XMQTT_SESSION_DONE,"bounded session open");const xmqttsessionpacket* P=sessionPublish(S,1,1);size_t Bytes=sessionInfo(S).LiveBytes-Initial.BaseBytes;
    xmqttpacket Q=*xmqttSessionPacket(P);const xmqttsessionpacket* Out=(const xmqttsessionpacket*)(uintptr_t)1;sessionStatus(xmqttSessionPublish(S,&Q,&Out),XMQTT_SESSION_LIMIT,"object limit");
    sessionStatus(xmqttSessionReceivePrepare(S,1),XMQTT_SESSION_LIMIT,"disabled incoming map");sessionStatus(xmqttSessionNext(S,NULL),XMQTT_SESSION_ERROR,"null output");testRequire(!xmqttSessionInfo(S,(xmqttsessioninfo*)S),"output cannot overwrite session");testRequire(!xmqttSessionPacketInfo(P,(xmqttsessionpacketinfo*)xmqttSessionPacket(P)),"output cannot overwrite packet");
    sessionStatus(xmqttSessionReceivePrepare(S,0),XMQTT_SESSION_ERROR,"zero incoming identifier");sessionStatus(xmqttSessionEnd(S,NULL,NAN),XMQTT_SESSION_STATE_ERROR,"NaN time");sessionStatus(xmqttSessionEnd(S,NULL,0),XMQTT_SESSION_STATE_ERROR,"backwards time");
    xmqttsessionframe F=sessionNext(S);xmqttsessionframe Forged=F;Forged.Prefix.Size=0;sessionStatus(xmqttSessionSent(S,&Forged),XMQTT_SESSION_ERROR,"altered frame token");xrtClearError();sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,1,0),XMQTT_SESSION_DONE,"bounded ACK");xmqttSessionPacketRelease(P);sessionEnd(S,2);testRequire(xmqttSessionDestroy(S),"bounds destroy");
    for(unsigned Exact=0;Exact<2;++Exact){S=sessionCreate(XMQTT_VERSION_5,1,0,Initial.BaseBytes+Bytes-(Exact?0:1));sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,1,1),XMQTT_SESSION_DONE,"exact byte quota open");Q.Identifier=1;Q.Body.Publish.Topic=XRT_STR_LITERAL("session/data");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");Q.Properties=(xmqttpropertylist){0};Out=(const xmqttsessionpacket*)(uintptr_t)1;
        sessionStatus(xmqttSessionPublish(S,&Q,&Out),Exact?XMQTT_SESSION_DONE:XMQTT_SESSION_LIMIT,"exact allocation byte boundary");if(Exact)xmqttSessionPacketRelease(Out);else testRequire(Out==(const xmqttsessionpacket*)(uintptr_t)1,"byte limit output unchanged");sessionEnd(S,2);testRequire(xmqttSessionDestroy(S),"byte quota destroy");}
}
static void testSessionIdentifierSpace(void)
{
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,65535,65535,64u*1024u*1024u);sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,0,65535,1),XMQTT_SESSION_DONE,"identifier-space open");
    for(uint32 I=1;I<=65535;++I){const xmqttsessionpacket* P=sessionPublish(S,1,0);testRequire(sessionPacketInfo(P).Identifier==I,"full outgoing id coverage");xmqttSessionPacketRelease(P);sessionStatus(xmqttSessionReceivePrepare(S,(uint16)I),XMQTT_SESSION_DONE,"full incoming id reserve");sessionStatus(xmqttSessionReceiveCommit(S,(uint16)I),XMQTT_SESSION_DONE,"full incoming id commit");}
    xmqttsessionframe F=sessionNext(S);sessionStatus(xmqttSessionSent(S,&F),XMQTT_SESSION_DONE,"first id sent");sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,1,0),XMQTT_SESSION_DONE,"first id ACK");testRequire(sessionInfo(S).Objects==65535,"native frame still reserves last available slot");
    xmqttSessionPacketRelease(F.Packet);const xmqttsessionpacket* P=sessionPublish(S,1,0);testRequire(sessionPacketInfo(P).Identifier==1,"identifier wrap/reuse after physical retirement");xmqttSessionPacketRelease(P);
    for(uint32 I=1;I<=65535;++I){uint8 Reason=255;sessionStatus(xmqttSessionReceiveRelease(S,(uint16)I,&Reason),XMQTT_SESSION_DONE,"full incoming id release");testRequire(!Reason,"full incoming reason");sessionStatus(xmqttSessionReceiveComplete(S,(uint16)I),XMQTT_SESSION_DONE,"full incoming id complete");}
    sessionEnd(S,2);testRequire(!sessionInfo(S).Objects && !sessionInfo(S).Incoming,"large state clears all resources");testRequire(xmqttSessionDestroy(S),"identifier-space destroy");
}
static void testSessionPeerAndNegative(void)
{
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,4,1,1024*1024);sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,4,1),XMQTT_SESSION_DONE,"collision session");
    const xmqttsessionpacket* Packets[3];for(unsigned I=0;I<3;++I){Packets[I]=sessionPublish(S,1,(uint16)(1+I*8));xmqttsessionframe F=sessionNext(S);sessionSent(S,&F);}
    const uint16 Order[]={9,17,1};for(unsigned I=0;I<3;++I)sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,Order[I],0),XMQTT_SESSION_DONE,"colliding hash-chain middle/head/tail removal");for(unsigned I=0;I<3;++I)xmqttSessionPacketRelease(Packets[I]);
    const xmqttsessionpacket* P=sessionPublish(S,2,5);xmqttsessionframe F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,5,0x87),XMQTT_SESSION_REJECTED,"negative PUBREC terminal; never PUBREL");xmqttsessionpacketinfo R=sessionPacketInfo(P);testRequire(R.PubrecReceived && R.PubrecReason==0x87 && R.Reason==0x87 && R.Stage==XMQTT_SESSION_COMPLETE,"negative PUBREC scalars");sessionEmpty(S);xmqttSessionPacketRelease(P);
    P=sessionPublish(S,2,6);F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,6,0),XMQTT_SESSION_DONE,"positive PUBREC");F=sessionNext(S);uint16 Quota=sessionInfo(S).SendQuota;
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,6,0),XMQTT_SESSION_DUPLICATE,"unsent/late PUBCOMP ignored");testRequire(sessionInfo(S).SendQuota==Quota && sessionPacketInfo(P).Active,"unsent PUBCOMP made no quota or completion");sessionSent(S,&F);
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,6,0x92),XMQTT_SESSION_REJECTED,"negative PUBCOMP terminal");xmqttSessionPacketRelease(P);
    P=sessionPublish(S,2,7);F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,7,0),XMQTT_SESSION_DONE,"old generation PUBREC");sessionEnd(S,2);
    sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,true,60,1,3),XMQTT_SESSION_DONE,"new connection quota1");const xmqttsessionpacket* Q=sessionPublish(S,1,8);F=sessionNext(S);testRequire(F.Type==XMQTT_PACKET_PUBREL && F.Identifier==7,"old PUBREL bypasses publish quota");sessionSent(S,&F);F=sessionNext(S);testRequire(F.Identifier==8,"new publication order");sessionSent(S,&F);testRequire(!sessionInfo(S).SendQuota,"new publish exhausted quota");
    sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,7,0),XMQTT_SESSION_DONE,"resumed PUBCOMP replenishes new quota");sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,7,0),XMQTT_SESSION_DUPLICATE,"duplicate COMP cannot mint another credit");testRequire(sessionInfo(S).SendQuota==1,"new quota capped at initial maximum");sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,8,0),XMQTT_SESSION_DONE,"new PUBACK capped");xmqttSessionPacketRelease(P);xmqttSessionPacketRelease(Q);
    P=sessionPublish(S,2,10);sessionEnd(S,4);
    for(unsigned Restriction=0;Restriction<2;++Restriction){xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=XRT_STR_LITERAL("session-test");xmqttproperty Exp=sessionNumber(XMQTT_PROPERTY_SESSION_EXPIRY,60),Limit=sessionNumber(Restriction?XMQTT_PROPERTY_MAXIMUM_QOS:XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,Restriction?1:2);C.Properties=(xmqttpropertylist){&Exp,1};A.SessionPresent=true;A.Properties=(xmqttpropertylist){&Limit,1};xmqttnegotiated N;
        sessionStatus(xmqttSessionOpen(S,&C,&A,5+Restriction*2,&N),XMQTT_SESSION_DONE,"resumed restricted peer");xmqttsessionframe Out;memset(&Out,0xa5,sizeof(Out));unsigned char Saved[sizeof(Out)];memcpy(Saved,&Out,sizeof(Out));sessionStatus(xmqttSessionNext(S,&Out),XMQTT_SESSION_RESTRICTED,"new peer cannot accept retained packet");testRequire(!memcmp(Saved,&Out,sizeof(Out)) && sessionPacketInfo(P).Active,"restriction cannot lose packet or alter frame output");xrtClearError();sessionEnd(S,6+Restriction*2);}
    sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,true,60,1,10),XMQTT_SESSION_DONE,"peer restored");F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,10,0),XMQTT_SESSION_DONE,"restored packet PUBREC");F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,10,0),XMQTT_SESSION_DONE,"restored packet completed");xmqttSessionPacketRelease(P);sessionEnd(S,11);testRequire(xmqttSessionDestroy(S),"peer limit destroy");
}
static void testSessionNegotiationAtomicity(void)
{
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,4,1,1024*1024);xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=XRT_STR_LITERAL("different");xmqttnegotiated N;memset(&N,0xa5,sizeof(N));unsigned char Saved[sizeof(N)];memcpy(Saved,&N,sizeof(N));
    sessionStatus(xmqttSessionOpen(S,&C,&A,1,&N),XMQTT_SESSION_STATE_ERROR,"wrong logical client identity");testRequire(!memcmp(Saved,&N,sizeof(N)) && !sessionInfo(S).HasState,"identity error atomicity");xrtClearError();C.Body.Connect.ClientId=XRT_STR_LITERAL("session-test");
    sessionStatus(xmqttSessionOpen(S,&C,&A,1,(xmqttnegotiated*)&C),XMQTT_SESSION_ERROR,"negotiated output aliases CONNECT");testRequire(C.Type==XMQTT_PACKET_CONNECT && !sessionInfo(S).Connected,"overlap did not open or overwrite");xrtClearError();
    A.Reason=0x87;sessionStatus(xmqttSessionOpen(S,&C,&A,1,&N),XMQTT_SESSION_REJECTED,"server refused");testRequire(N.Reason==0x87 && !N.Accepted && !sessionInfo(S).HasState,"refusal result without local acceptance");A.Reason=0;
    sessionStatus(xmqttSessionOpen(S,&C,&A,1,&N),XMQTT_SESSION_DONE,"original expiry zero");xmqttpacket D;xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&D);xmqttproperty E=sessionNumber(XMQTT_PROPERTY_SESSION_EXPIRY,1);D.Properties=(xmqttpropertylist){&E,1};xmqttsessioninfo Before=sessionInfo(S);
    sessionStatus(xmqttSessionEnd(S,&D,2),XMQTT_SESSION_ERROR,"cannot extend originally zero expiry at DISCONNECT");xmqttsessioninfo After=sessionInfo(S);testRequire(sessionSameInfo(&Before,&After),"invalid expiry did not terminate session");xrtClearError();E.Value.Number=0;sessionStatus(xmqttSessionEnd(S,&D,2),XMQTT_SESSION_DONE,"valid zero-expiry DISCONNECT");testRequire(!sessionInfo(S).HasState,"zero expiry discarded state");testRequire(xmqttSessionDestroy(S),"negotiation atomicity destroy");
    S=sessionCreate(XMQTT_VERSION_5,1,0,1024*1024);double Huge=1e100;
    sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,1,1,Huge),XMQTT_SESSION_DONE,"large finite clock open");sessionEnd(S,Huge);sessionStatus(xmqttSessionExpire(S,Huge),XMQTT_SESSION_EMPTY,"rounded deadline cannot expire without elapsed time");
    sessionStatus(xmqttSessionExpire(S,Huge*1.0000000000000002),XMQTT_SESSION_DONE,"large representable elapsed interval expires");testRequire(xmqttSessionDestroy(S),"large-clock destroy");
}
static void testSessionAllocation(void)
{
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,4,4,1024*1024);sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,1,1),XMQTT_SESSION_DONE,"OOM open");
    xmqttsessioninfo Before=sessionInfo(S);xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"OOM budget enter");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Body.Publish.Topic=XRT_STR_LITERAL("session/data");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");const xmqttsessionpacket* Out=(const xmqttsessionpacket*)(uintptr_t)1;
    sessionStatus(xmqttSessionPublish(S,&Q,&Out),XMQTT_SESSION_ERROR,"packet allocation failure");testRequire(Out==(const xmqttsessionpacket*)(uintptr_t)1 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"OOM output and diagnostic");xmqttsessioninfo After=sessionInfo(S);
    testRequire(sessionSameInfo(&Before,&After),"OOM changed session state");xrtClearError();
    testRequire(xrtAllocationBudgetLeave(&Scope),"OOM leave");xallocationbudgetinfo B={0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(Budget,&B) && !B.LiveBytes && B.Rejections,"OOM allocation budget fully refunded");xrtAllocationBudgetFree(Budget);
    const xmqttsessionpacket* P=sessionPublish(S,2,0);testRequire(sessionPacketInfo(P).Identifier==1,"failed allocation did not reserve id");
    xmemstats First,Last;xrtMemStatsEnable(true);xrtMemStatsGet(&First);
    xmqttsessionframe F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBREC,1,0),XMQTT_SESSION_DONE,"allocation-free PUBREC");F=sessionNext(S);sessionSent(S,&F);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBCOMP,1,0),XMQTT_SESSION_DONE,"allocation-free PUBCOMP");
    for(unsigned I=0;I<10000;++I){sessionStatus(xmqttSessionReceivePrepare(S,4),XMQTT_SESSION_DONE,"warm receive reserve");sessionStatus(xmqttSessionReceiveCommit(S,4),XMQTT_SESSION_DONE,"warm receive commit");uint8 Reason=255;sessionStatus(xmqttSessionReceiveRelease(S,4,&Reason),XMQTT_SESSION_DONE,"warm receive release");sessionStatus(xmqttSessionReceiveComplete(S,4),XMQTT_SESSION_DONE,"warm receive complete");}
    xrtMemStatsGet(&Last);xrtMemStatsEnable(false);testRequire(First.MallocCalls==Last.MallocCalls && First.CallocCalls==Last.CallocCalls && First.ReallocCalls==Last.ReallocCalls && First.BlockAllocCalls==Last.BlockAllocCalls,"ACK/replay/view/incoming paths allocated");xmqttSessionPacketRelease(P);sessionEnd(S,2);testRequire(xmqttSessionDestroy(S),"OOM destroy");
    for(unsigned Limit=1;Limit<40000;Limit+=997){testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"factory prefix OOM enter");xmqttsessionconfig C;xmqttSessionConfigInit(&C);C.ClientId=XRT_STR_LITERAL("session-test");S=NULL;
        if(xmqttSessionCreate(&C,&S)){sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,1,1),XMQTT_SESSION_DONE,"factory prefix open");sessionEnd(S,2);testRequire(xmqttSessionDestroy(S),"factory prefix destroy");}
        xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"factory prefix leave");B=(xallocationbudgetinfo){0};B.Size=sizeof(B);testRequire(xrtAllocationBudgetInfo(Budget,&B) && !B.LiveBytes,"factory prefix no leaked allocations");xrtAllocationBudgetFree(Budget);}
}
typedef struct sessionReader {xmqttsession* Session;const xmqttsessionpacket* Packet;xatomic32* Start;} sessionReader;
static int32 sessionReadThread(ptr Data)
{
    sessionReader* R=Data;while(!xrtAtomic32Load(R->Start,XMEMORY_ACQUIRE))xrtThreadYield();
    for(unsigned I=0;I<5000;++I){xmqttsessioninfo S;xmqttsessionpacketinfo P;if(!xmqttSessionPacketRetain(R->Packet) || !xmqttSessionInfo(R->Session,&S) || !xmqttSessionPacketInfo(R->Packet,&P))return 1;
        const xmqttpacket* Q=xmqttSessionPacket(R->Packet);bool OK=Q && Q->Body.Publish.Payload.Size==3 && !memcmp(Q->Body.Publish.Payload.Data,"x\0z",3);xmqttSessionPacketRelease(R->Packet);if(!OK)return 2;}
    xmqttSessionPacketRelease(R->Packet);return 0;
}
static void testSessionThreads(void)
{
    xmqttsession* S=sessionCreate(XMQTT_VERSION_5,4,4,1024*1024);sessionStatus(sessionOpen(S,XMQTT_VERSION_5,false,false,60,1,1),XMQTT_SESSION_DONE,"threaded open");const xmqttsessionpacket* P=sessionPublish(S,1,7);xmqttsessionframe F=sessionNext(S);sessionSent(S,&F);
    xatomic32 Start;xrtAtomic32Init(&Start,0);sessionReader R={S,P,&Start};xthread* Threads[8];
    for(unsigned I=0;I<8;++I){testRequire(xmqttSessionPacketRetain(P),"thread owns packet");Threads[I]=xrtThreadCreate(sessionReadThread,&R,0);testRequire(Threads[I]!=NULL,"reader thread");}
    xrtAtomic32Store(&Start,1,XMEMORY_RELEASE);sessionStatus(sessionAck(S,XMQTT_PACKET_PUBACK,7,0),XMQTT_SESSION_DONE,"concurrent packet completion");sessionEnd(S,2);sessionStatus(xmqttSessionReset(S),XMQTT_SESSION_DONE,"concurrent snapshot with reset");
    for(unsigned I=0;I<8;++I){testRequire(xrtThreadWaitFor(Threads[I],15000)==XWAIT_OK && xrtThreadExitCode(Threads[I])==0,"reader join");xrtThreadDestroy(Threads[I]);}
    testRequire(xmqttSessionDestroy(S) && xmqttSessionPacket(P)->Body.Publish.Payload.Size==3,"held packet after threads/session join");xmqttSessionPacketRelease(P);
}
int main(void)
{
    testSessionFlows(XMQTT_VERSION_5);testSessionFlows(XMQTT_VERSION_311);testSessionReplay(XMQTT_VERSION_5);testSessionReplay(XMQTT_VERSION_311);testSessionIncoming(XMQTT_VERSION_5);testSessionIncoming(XMQTT_VERSION_311);testSessionSnapshot();testSessionIdentityExpiry(XMQTT_VERSION_5);testSessionIdentityExpiry(XMQTT_VERSION_311);testSessionBoundsAndArguments();testSessionIdentifierSpace();testSessionPeerAndNegative();testSessionNegotiationAtomicity();testSessionAllocation();testSessionThreads();
    puts("[PASS] xmqtt session: QoS state/replay cuts/partial generations/identity/expiry/limits/full ids/snapshots/zero-allocation ACKs/OOM/retained physical lifetime/threads");return 0;
}
