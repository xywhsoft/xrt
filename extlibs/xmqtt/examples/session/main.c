#include <xmqtt.h>
#include <stdio.h>
/* A caller-driven protocol-state example. A transport must actually authenticate,
 * frame/validate input, serialize complete output frames and report termination.
 * Sent below simulates that boundary; it is not a network-delivery assertion. */
static bool completeFrame(xmqttsession* S,xmqttsessionframe* F)
{bool OK=xmqttSessionSent(S,F)==XMQTT_SESSION_DONE;xmqttSessionPacketRelease(F->Packet);F->Packet=NULL;return OK;}
int main(void)
{
    int Code=1;xmqttsession* S=NULL;const xmqttsessionpacket* P=NULL;xmqttsessionframe Old={0},Next={0};
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.ClientId=XRT_STR_LITERAL("session-example");
    if(!xmqttSessionCreate(&K,&S))goto Done;
    xmqttpacket Connect,Connack,Publish,Ack;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);Connect.Body.Connect.ClientId=K.ClientId;
    xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;Connect.Properties=(xmqttpropertylist){&Expiry,1};xmqttPacketInit(XMQTT_PACKET_CONNACK,&Connack);xmqttnegotiated N;
    if(xmqttSessionOpen(S,&Connect,&Connack,0,&N)!=XMQTT_SESSION_DONE)goto Done;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);Publish.Flags=4;Publish.Body.Publish.Topic=XRT_STR_LITERAL("example/session");Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("hello");
    if(xmqttSessionPublish(S,&Publish,&P)!=XMQTT_SESSION_DONE || xmqttSessionNext(S,&Old)!=XMQTT_SESSION_DONE)goto Done;
    /* The old transport can retain any prefix. Abandon that generation without
     * claiming that all its bytes were sent. Its borrowed view stays valid. */
    if(xmqttSessionEnd(S,NULL,1)!=XMQTT_SESSION_DONE)goto Done;
    Connack.SessionPresent=true;
    if(xmqttSessionOpen(S,&Connect,&Connack,2,&N)!=XMQTT_SESSION_DONE || xmqttSessionNext(S,&Next)!=XMQTT_SESSION_DONE || Next.Type!=XMQTT_PACKET_PUBLISH || !(Next.Prefix.Data[0]&8u) || Next.Identifier!=Old.Identifier)goto Done;
    if(!completeFrame(S,&Next))goto Done;
    xmqttPacketInit(XMQTT_PACKET_PUBREC,&Ack);Ack.Identifier=Old.Identifier;
    if(xmqttSessionAcknowledge(S,&Ack)!=XMQTT_SESSION_DONE || xmqttSessionNext(S,&Next)!=XMQTT_SESSION_DONE || Next.Type!=XMQTT_PACKET_PUBREL || !completeFrame(S,&Next))goto Done;
    xmqttPacketInit(XMQTT_PACKET_PUBCOMP,&Ack);Ack.Identifier=Old.Identifier;
    if(xmqttSessionAcknowledge(S,&Ack)!=XMQTT_SESSION_DONE || xmqttSessionEnd(S,NULL,3)!=XMQTT_SESSION_DONE || !xmqttSessionDestroy(S))goto Done;
    S=NULL;
    if(!xmqttSessionPacket(P) || xmqttSessionPacket(P)->Body.Publish.Payload.Size!=5 || Old.Prefix.Data[0]!=0x34)goto Done;
    puts("[PASS] partial generation -> DUP/original-id replay -> PUBREC/PUBREL/PUBCOMP; immutable views outlive session");Code=0;
Done:
    xmqttSessionPacketRelease(Old.Packet);xmqttSessionPacketRelease(Next.Packet);xmqttSessionPacketRelease(P);
    if(S){xmqttsessioninfo I;if(xmqttSessionInfo(S,&I) && I.Connected)(void)xmqttSessionEnd(S,NULL,100);if(!xmqttSessionDestroy(S))Code=1;}
    return Code;
}
