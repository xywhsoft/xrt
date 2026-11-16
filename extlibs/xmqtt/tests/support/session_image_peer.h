#ifndef XMQTT_TEST_SESSION_IMAGE_PEER_H
#define XMQTT_TEST_SESSION_IMAGE_PEER_H
#include "../test.h"
#include <xmqtt/session_image.h>
#include <xrt/crc32.h>
#include <math.h>
static inline void miStatus(xmqttsessionstatus Got,xmqttsessionstatus Want,cstr Text)
{if(Got!=Want)fprintf(stderr,"image status%d expected%d: %s (%s)\n",Got,Want,Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");testRequire(Got==Want,Text);}
static inline xmqttsessionconfig miSessionConfig(xmqttversion V)
{xmqttsessionconfig K;testRequire(xmqttSessionConfigInit(&K),"image Session config");K.Version=V;K.ClientId=XRT_STR_LITERAL("image-client");K.MaxOutgoing=8;K.MaxIncoming=8;K.MaxBytes=1024u*1024u;return K;}
static inline xmqttsessionimageconfig miConfig(double Now,uint64 Wall)
{xmqttsessionimageconfig C;testRequire(xmqttSessionImageConfigInit(&C),"image config");C.Binding=XRT_BYTES_LITERAL("b\0p");C.MonotonicTime=Now;C.WallMilliseconds=Wall;return C;}
static inline xmqttsessioninfo miInfo(xmqttsession* S)
{xmqttsessioninfo I;testRequire(xmqttSessionInfo(S,&I),"image restored Session info");return I;}
static inline xmqttsession* miCreate(xmqttversion V)
{xmqttsessionconfig K=miSessionConfig(V);xmqttsession* S=NULL;testRequire(xmqttSessionCreate(&K,&S),"image source Session");return S;}
static inline xmqttsessionstatus miOpen(xmqttsession* S,xmqttversion V,bool Clean,bool Present,uint32 Expiry,double Now)
{xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=XRT_STR_LITERAL("image-client");C.Body.Connect.CleanStart=Clean;A.SessionPresent=Present;xmqttproperty P={0};P.Id=XMQTT_PROPERTY_SESSION_EXPIRY;P.Type=XMQTT_PROPERTY_UINT32;P.Value.Number=Expiry;if(V==XMQTT_VERSION_5)C.Properties=(xmqttpropertylist){&P,1};xmqttnegotiated N;return xmqttSessionOpen(S,&C,&A,Now,&N);}
static inline const xmqttsessionpacket* miPublish(xmqttsession* S,unsigned Qos,uint16 Id)
{xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Flags=(uint8)(Qos<<1);P.Identifier=Id;P.Body.Publish.Topic=XRT_STR_LITERAL("image/topic");P.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");const xmqttsessionpacket* R=NULL;miStatus(xmqttSessionPublish(S,&P,&R),XMQTT_SESSION_DONE,"image source outgoing snapshot");return R;}
static inline xmqttsessionframe miNext(xmqttsession* S,xmqttpackettype Type,uint16 Id,bool Duplicate)
{xmqttsessionframe F;miStatus(xmqttSessionNext(S,&F),XMQTT_SESSION_DONE,"restored next frame");testRequire(F.Type==Type&&F.Identifier==Id&&F.Prefix.Size&&((F.Prefix.Data[0]&8u)!=0)==Duplicate,"restored stage/id/DUP");return F;}
static inline void miSent(xmqttsession* S,xmqttsessionframe F)
{miStatus(xmqttSessionSent(S,&F),XMQTT_SESSION_DONE,"restored full frame emitted");xmqttSessionPacketRelease(F.Packet);}
static inline void miAck(xmqttsession* S,xmqttpackettype Type,uint16 Id,uint8 Reason)
{xmqttpacket A;xmqttPacketInit(Type,&A);A.Identifier=Id;A.Reason=Reason;miStatus(xmqttSessionAcknowledge(S,&A),XMQTT_SESSION_DONE,"restored acknowledgement");}
static inline void miStop(xmqttsession* S,double Now)
{if(miInfo(S).Connected)miStatus(xmqttSessionEnd(S,NULL,Now),XMQTT_SESSION_DONE,"image source network end");testRequire(xmqttSessionDestroy(S),"image Session physical state release");}
static inline xmqttsession* miPopulate(xmqttversion V,bool Live)
{
    xmqttsession* S=miCreate(V);miStatus(miOpen(S,V,false,false,60,1),XMQTT_SESSION_DONE,"source persistent open");const xmqttsessionpacket* P=miPublish(S,1,7);xmqttSessionPacketRelease(P);miSent(S,miNext(S,XMQTT_PACKET_PUBLISH,7,false));P=miPublish(S,2,8);xmqttSessionPacketRelease(P);miSent(S,miNext(S,XMQTT_PACKET_PUBLISH,8,false));miAck(S,XMQTT_PACKET_PUBREC,8,V==XMQTT_VERSION_5?0x10:0);P=miPublish(S,1,9);xmqttSessionPacketRelease(P);
    for(uint16 Id=17;Id<=19;++Id){miStatus(xmqttSessionReceivePrepare(S,Id),XMQTT_SESSION_DONE,"source receive prepare");if(Id!=19)miStatus(xmqttSessionReceiveCommit(S,Id),XMQTT_SESSION_DONE,"source receive commit");}uint8 Reason=0xff;miStatus(xmqttSessionReceiveRelease(S,18,&Reason),XMQTT_SESSION_DONE,"source received PUBREL");testRequire(!Reason,"source known PUBREL reason");
    if(!Live){miStatus(xmqttSessionEnd(S,NULL,10.25),XMQTT_SESSION_DONE,"source actual network termination");}return S;
}
static inline void miReplay(xmqttsession* S,xmqttversion V,double Now)
{
    xmqttsessioninfo I=miInfo(S);testRequire(!I.Connected&&I.HasState&&I.ActiveOutgoing==3&&I.Incoming==2&&!I.Prepared,"restored protocol obligations and excluded invisible preparation");miStatus(miOpen(S,V,false,true,60,Now),XMQTT_SESSION_DONE,"fresh resumed MQTT negotiation");xmqttsessionframe F=miNext(S,XMQTT_PACKET_PUBREL,8,false);testRequire(F.Generation==2&&F.Prefix.Size==4&&F.Prefix.Data[0]==0x62,"generation advances and PUBREL remains exact fixed flags");miSent(S,F);miAck(S,XMQTT_PACKET_PUBCOMP,8,0);F=miNext(S,XMQTT_PACKET_PUBLISH,7,true);miSent(S,F);miAck(S,XMQTT_PACKET_PUBACK,7,0);F=miNext(S,XMQTT_PACKET_PUBLISH,9,false);miSent(S,F);miAck(S,XMQTT_PACKET_PUBACK,9,0);
    for(uint16 Id=17;Id<=18;++Id){miStatus(xmqttSessionReceivePrepare(S,Id),XMQTT_SESSION_DUPLICATE,"restored receive suppresses duplicate identity");uint8 Reason=0xff;miStatus(xmqttSessionReceiveRelease(S,Id,&Reason),XMQTT_SESSION_DONE,"restored PUBREL known");testRequire(!Reason,"restored known PUBREL reason");miStatus(xmqttSessionReceiveComplete(S,Id),XMQTT_SESSION_DONE,"restored complete PUBCOMP");}
    miStatus(xmqttSessionReceivePrepare(S,19),XMQTT_SESSION_DONE,"invisible prepared receive was not restored");miStatus(xmqttSessionReceiveAbort(S,19),XMQTT_SESSION_DONE,"abort invisible receive");const xmqttsessionpacket* P=miPublish(S,1,0);xmqttsessionpacketinfo PI;testRequire(xmqttSessionPacketInfo(P,&PI)&&PI.Identifier==10,"next automatic identifier preserved");xmqttSessionPacketRelease(P);
}
static inline size_t miWrite(xmqttsession* S,const xmqttsessionimageconfig* C,unsigned char* B,size_t Capacity)
{size_t Size=0,Written=0;miStatus(xmqttSessionImageSize(S,C,&Size),XMQTT_SESSION_DONE,"image exact measurement");miStatus(xmqttSessionImageWrite(S,C,B,Capacity,&Written),XMQTT_SESSION_DONE,"image atomic write");testRequire(Size==Written&&Written>=68,"image exact write bytes");return Written;}
#endif
