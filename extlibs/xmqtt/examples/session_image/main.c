#include <xmqtt.h>
#include <xmqtt/session_image.h>
#include <stdio.h>
#include <string.h>
static bool demoOpen(xmqttsession* S,bool Present,double Now)
{
    xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=XRT_STR_LITERAL("image-demo");C.Body.Connect.CleanStart=false;A.SessionPresent=Present;xmqttproperty P={0};P.Id=XMQTT_PROPERTY_SESSION_EXPIRY;P.Type=XMQTT_PROPERTY_UINT32;P.Value.Number=60;C.Properties=(xmqttpropertylist){&P,1};xmqttnegotiated N;return xmqttSessionOpen(S,&C,&A,Now,&N)==XMQTT_SESSION_DONE;
}
static void demoDrop(xmqttsession* S)
{if(S){xmqttsessioninfo I;if(xmqttSessionInfo(S,&I)&&I.Connected)(void)xmqttSessionEnd(S,NULL,1000);(void)xmqttSessionDestroy(S);}}
int main(void)
{
    int Code=1;xmqttsession *Source=NULL,*Restored=NULL;const xmqttsessionpacket* Held=NULL;xmqttsessionframe F={0};unsigned char Buffer[4096];size_t Size=0;
    xmqttsessionconfig K;xmqttSessionConfigInit(&K);K.ClientId=XRT_STR_LITERAL("image-demo");xmqttsessionimageconfig C;xmqttSessionImageConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("demo broker/principal/slot");C.MonotonicTime=2;C.WallMilliseconds=1000000;
    if(!xmqttSessionCreate(&K,&Source)||!demoOpen(Source,false,1))goto Done;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=7;Q.Body.Publish.Topic=XRT_STR_LITERAL("image/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");if(xmqttSessionPublish(Source,&Q,&Held)!=XMQTT_SESSION_DONE||xmqttSessionNext(Source,&F)!=XMQTT_SESSION_DONE)goto Done;
    /* The example drives the passive protocol boundary explicitly. A real
     * transport calls Sent only after its actual complete frame emission. */
    if(xmqttSessionSent(Source,&F)!=XMQTT_SESSION_DONE)goto Done;
    xmqttSessionPacketRelease(F.Packet);F.Packet=NULL;if(xmqttSessionEnd(Source,NULL,2)!=XMQTT_SESSION_DONE||xmqttSessionImageWrite(Source,&C,Buffer,sizeof(Buffer),&Size)!=XMQTT_SESSION_DONE||!xmqttSessionDestroy(Source)){goto Done;}Source=NULL;
    C.MonotonicTime=10;C.WallMilliseconds+=50;if(xmqttSessionImageRestore(&K,&C,(xbytesview){Buffer,Size},&Restored)!=XMQTT_SESSION_DONE)goto Done;memset(Buffer,0,sizeof(Buffer));
    if(!demoOpen(Restored,true,10)||xmqttSessionNext(Restored,&F)!=XMQTT_SESSION_DONE||F.Identifier!=7||!(F.Prefix.Data[0]&8u)||xmqttSessionSent(Restored,&F)!=XMQTT_SESSION_DONE)goto Done;
    xmqttSessionPacketRelease(F.Packet);F.Packet=NULL;xmqttPacketInit(XMQTT_PACKET_PUBACK,&Q);Q.Identifier=7;if(xmqttSessionAcknowledge(Restored,&Q)!=XMQTT_SESSION_DONE||xmqttSessionEnd(Restored,NULL,11)!=XMQTT_SESSION_DONE||!xmqttSessionDestroy(Restored)){goto Done;}Restored=NULL;
    if(!Held||xmqttSessionPacket(Held)->Body.Publish.Payload.Size!=3)goto Done;
    printf("[DATA] %zu image bytes; new clock epoch, original id7 and DUP replay\n",Size);puts("[JOIN] original/restored Sessions retired; held old packet remains independent");Code=0;
Done:
    xmqttSessionPacketRelease(F.Packet);demoDrop(Source);demoDrop(Restored);xmqttSessionPacketRelease(Held);if(Code&&xrtGetError())fprintf(stderr,"%s\n",xrtErrorMessage(xrtGetError()));return Code;
}
