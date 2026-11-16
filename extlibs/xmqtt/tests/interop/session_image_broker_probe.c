#include <xmqtt.h>
#include <xmqtt/session_image.h>
#include <xrt/time.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
static void imageBrokerCheck(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] broker image: %s (%s)\n",Text,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static void imageBrokerClock(xmqttsessionimageconfig* C)
{xtime Wall=xrtNow();imageBrokerCheck(Wall>=XRT_TIME_UNIX_EPOCH,"trusted test wall clock");C->WallMilliseconds=(uint64)(Wall-XRT_TIME_UNIX_EPOCH);C->MonotonicTime=xrtTimer();}
static void imageBrokerReplace(const xmqttsessionconfig* K,xmqttsession** S)
{
    xmqttsessionimageconfig C;xmqttSessionImageConfigInit(&C);C.Binding=XRT_BYTES_LITERAL("independent broker/principal/slot");imageBrokerClock(&C);size_t Bytes=0,Written=0;imageBrokerCheck(xmqttSessionImageSize(*S,&C,&Bytes)==XMQTT_SESSION_DONE,"measure actual post-network Session image");unsigned char* Data=xrtMalloc(Bytes);imageBrokerCheck(Data&&xmqttSessionImageWrite(*S,&C,Data,Bytes,&Written)==XMQTT_SESSION_DONE&&Written==Bytes,"write actual Session image");imageBrokerCheck(xmqttSessionDestroy(*S),"destroy original Session before replacement");*S=NULL;imageBrokerClock(&C);imageBrokerCheck(xmqttSessionImageRestore(K,&C,(xbytesview){Data,Bytes},S)==XMQTT_SESSION_DONE,"fresh Session from independent broker state image");xrtSecureZero(Data,Bytes);xrtFree(Data);fprintf(stderr,"[IMAGE] %zu bytes; original Session destroyed, restored input erased\n",Bytes);
}
#define XMQTT_SESSION_BROKER_REPLACE_SESSION imageBrokerReplace
#include "session_broker_probe.c"
