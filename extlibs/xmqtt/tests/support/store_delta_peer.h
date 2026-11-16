#ifndef XMQTT_TEST_STORE_DELTA_PEER_H
#define XMQTT_TEST_STORE_DELTA_PEER_H
#include "../test.h"
#include <xmqtt/store_delta.h>
#include <xmqtt/journal.h>
enum {SD_COUNT=16,SD_WIRE=256,SD_BUFFER=4096};
typedef struct sdsource {xmqttstoredelta Ops[SD_COUNT];unsigned char Wire[4][SD_WIRE];size_t Sizes[4];} sdsource;
static inline void sdStatus(xmqttstoredeltastatus Got,xmqttstoredeltastatus Want,cstr Why)
{if(Got!=Want)fprintf(stderr,"delta status%d expected%d %s error%s\n",Got,Want,Why,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");testRequire(Got==Want,Why);}
static inline xmqttstoredeltaconfig sdConfig(xmqttversion V)
{xmqttstoredeltaconfig C;testRequire(xmqttStoreDeltaConfigInit(&C),"delta config");C.Version=V;return C;}
static inline void sdSource(xmqttversion V,sdsource* S)
{
    memset(S,0,sizeof(*S));xmqttproperty P[4]={{0}};P[0].Id=XMQTT_PROPERTY_USER;P[0].Type=XMQTT_PROPERTY_STRING_PAIR;P[0].Value.Pair.Name=XRT_STR_LITERAL("meta");P[0].Value.Pair.Value=XRT_STR_LITERAL("value");P[1].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT;P[1].Type=XMQTT_PROPERTY_BYTE;P[1].Value.Number=1;P[2].Id=XMQTT_PROPERTY_CORRELATION_DATA;P[2].Type=XMQTT_PROPERTY_BINARY;P[2].Value.Binary=XRT_BYTES_LITERAL("a\0b");P[3].Id=XMQTT_PROPERTY_SUBSCRIPTION_ID;P[3].Type=XMQTT_PROPERTY_VARIABLE;P[3].Value.Number=1;
    for(unsigned I=0;I<4;++I){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=I==0?2:I==3?0:4;Q.Identifier=I==0?7:I==1?8:I==2?17:0;Q.Body.Publish.Topic=XRT_STR_LITERAL("delta/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){P,I>=2?4:3};xmqttdirection D=I<2?XMQTT_CLIENT_TO_SERVER:XMQTT_SERVER_TO_CLIENT;
        testRequire(V==XMQTT_VERSION_5?xmqttPacketWrite5(S->Wire[I],SD_WIRE,&Q,D,NULL,&S->Sizes[I]):xmqttPacketWrite311(S->Wire[I],SD_WIRE,&Q,D,NULL,&S->Sizes[I]),"canonical source PUBLISH native encoder");}
    for(unsigned I=0;I<SD_COUNT;++I)S->Ops[I].Epoch=1;
    S->Ops[0].Type=XMQTT_STORE_BIND;S->Ops[0].WallMilliseconds=1000;S->Ops[0].Data=XRT_BYTES_LITERAL("delta-client");
    S->Ops[1].Type=XMQTT_STORE_OPEN;S->Ops[1].WallMilliseconds=1001;S->Ops[1].Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX;
    S->Ops[2]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUT,.Flags=3,.Identifier=7,.Epoch=1,.Key=11,.Data={S->Wire[0],S->Sizes[0]}};
    S->Ops[3]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_ATTEMPT,.Identifier=7,.Epoch=1,.Key=11};
    S->Ops[4]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUT,.Flags=3,.Identifier=8,.Epoch=1,.Key=12,.Data={S->Wire[1],S->Sizes[1]}};
    S->Ops[5]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUBREL,.Reason=V==XMQTT_VERSION_5?0x10:0,.Identifier=8,.Epoch=1,.Key=12};
    S->Ops[6]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_COMPLETE,.Flags=XMQTT_PACKET_PUBCOMP,.Identifier=8,.Epoch=1,.Key=12};
    S->Ops[7]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_COMPLETE,.Flags=XMQTT_PACKET_PUBACK,.Identifier=7,.Epoch=1,.Key=11};
    S->Ops[8]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=17,.Epoch=1,.Key=21,.Data={S->Wire[2],S->Sizes[2]}};
    S->Ops[9]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=22,.Data={S->Wire[3],S->Sizes[3]}};
    S->Ops[10]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_PUBREL,.Identifier=17,.Epoch=1,.Key=21};
    S->Ops[11]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_COMPLETE,.Identifier=17,.Epoch=1,.Key=21};
    S->Ops[12]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Flags=1,.Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX,.Epoch=1,.WallMilliseconds=2000};
    S->Ops[13]=(xmqttstoredelta){.Type=XMQTT_STORE_CLEAR,.Reason=2,.Epoch=1,.WallMilliseconds=2001};
    S->Ops[14]=(xmqttstoredelta){.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=21};S->Ops[15]=(xmqttstoredelta){.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=22};
}
static inline size_t sdWrite(const xmqttstoredeltaconfig* C,sdsource* S,unsigned char* B,size_t Capacity)
{xmqttproperty P[4];size_t N=0,M=0;sdStatus(xmqttStoreDeltaSize(C,S->Ops,SD_COUNT,P,4,&N),XMQTT_STORE_DELTA_DONE,"complete delta batch measure");sdStatus(xmqttStoreDeltaWrite(C,S->Ops,SD_COUNT,P,4,B,Capacity,&M),XMQTT_STORE_DELTA_DONE,"complete delta batch write");testRequire(N==M,"exact delta size");return N;}
static inline void sdCheck(const xmqttstoredeltaconfig* C,xbytesview Input)
{
    xmqttstoredelta Ops[SD_COUNT];xmqttproperty P[4];xmqttstoredeltadocument D={0};sdStatus(xmqttStoreDeltaRead(C,Input,Ops,SD_COUNT,P,4,&D),XMQTT_STORE_DELTA_DONE,"full delta read");testRequire(D.Count==SD_COUNT&&D.Operations==Ops&&D.Raw.Data==Input.Data&&D.Raw.Size==Input.Size,"exact borrowed transaction");sdsource S;sdSource(C->Version,&S);
    for(unsigned I=0;I<SD_COUNT;++I){const xmqttstoredelta* O=Ops+I,*Want=S.Ops+I;testRequire(O->Type==Want->Type&&O->Flags==Want->Flags&&O->Reason==Want->Reason&&O->Identifier==Want->Identifier&&O->Expiry==Want->Expiry&&O->Epoch==Want->Epoch&&O->Key==Want->Key&&O->WallMilliseconds==Want->WallMilliseconds&&O->Data.Size==Want->Data.Size&&(!O->Data.Size||!memcmp(O->Data.Data,Want->Data.Data,O->Data.Size)),"every typed record/value/body exact");if(O->Data.Size)testRequire(O->Data.Data>=Input.Data&&O->Data.Data+O->Data.Size<=Input.Data+Input.Size,"all output body views borrow immutable input");}
    memset(P,0xcc,sizeof(P));testRequire(Ops[14].Type==XMQTT_STORE_INBOX_ACK&&Ops[13].Type==XMQTT_STORE_CLEAR&&!memcmp(Ops[8].Data.Data,S.Wire[2],S.Sizes[2]),"operation wire does not borrow transient property scratch; Inbox ACK remains distinct after reset");
}
#endif
