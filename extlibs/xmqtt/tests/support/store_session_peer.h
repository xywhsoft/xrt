#ifndef XMQTT_TEST_STORE_SESSION_PEER_H
#define XMQTT_TEST_STORE_SESSION_PEER_H
#include "store_state_peer.h"
#include <xmqtt/store_session.h>
static inline xmqttsessionconfig ssSessionConfig(xmqttversion V)
{xmqttsessionconfig K;testRequire(xmqttSessionConfigInit(&K),"import Session config");K.Version=V;K.ClientId=XRT_STR_LITERAL("delta-client");K.MaxOutgoing=8;K.MaxIncoming=8;K.MaxBytes=1048576;return K;}
static inline xmqttstoresessionconfig ssConfig(double Mono,uint64 Wall)
{xmqttstoresessionconfig C;testRequire(xmqttStoreSessionConfigInit(&C),"import config");C.Binding=XRT_BYTES_LITERAL("state/broker/principal/slot");C.Generation=9;C.MonotonicTime=Mono;C.WallMilliseconds=Wall;return C;}
static inline void ssStatus(xmqttsessionstatus Got,xmqttsessionstatus Want,cstr Why)
{if(Got!=Want)fprintf(stderr,"import status%d expected%d %s error%s\n",Got,Want,Why,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");testRequire(Got==Want,Why);}
static inline xmqttsessioninfo ssInfo(xmqttsession* S)
{xmqttsessioninfo I={0};testRequire(xmqttSessionInfo(S,&I),"imported Session info");return I;}
/* Three outgoing packets: attempted QoS1, accepted-PUBREC QoS2, unattempted
 * QoS1. Two receive identities: PUBREL received and only PUBLISH accepted.
 * Every payload/property field originates from native packet codecs. */
static inline xmqttstorestate* ssRecover(xmqttversion V,bool Live,uint32 Expiry)
{
    jpdrive Drive={0};jpfile F=jpFile(&Drive);stsource Source;stSource(V,&Source);xmqttstoredelta O[10]={Source.Ops[0],Source.Ops[1]};O[1].Expiry=V==XMQTT_VERSION_5?Expiry:UINT32_MAX;
    unsigned char Wires[5][256];size_t Sizes[5];xmqttproperty P[5]={{0}};
    P[0].Id=XMQTT_PROPERTY_USER;P[0].Type=XMQTT_PROPERTY_STRING_PAIR;P[0].Value.Pair.Name=XRT_STR_LITERAL("");P[0].Value.Pair.Value=XRT_STR_LITERAL("meta");
    P[1].Id=XMQTT_PROPERTY_CORRELATION_DATA;P[1].Type=XMQTT_PROPERTY_BINARY;P[1].Value.Binary=XRT_BYTES_LITERAL("a\0b");
    P[2].Id=XMQTT_PROPERTY_CONTENT_TYPE;P[2].Type=XMQTT_PROPERTY_STRING;P[2].Value.Text=XRT_STR_LITERAL("type/utf8");
    P[3].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT;P[3].Type=XMQTT_PROPERTY_BYTE;P[3].Value.Number=1;
    P[4].Id=XMQTT_PROPERTY_SUBSCRIPTION_ID;P[4].Type=XMQTT_PROPERTY_VARIABLE;P[4].Value.Number=3;
    for(unsigned I=0;I<5;++I){xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Identifier=(uint16)(I<3?7+I:14+I);Q.Flags=I==1||I>=3?4:2;Q.Body.Publish.Topic=XRT_STR_LITERAL("store/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){P,I<3?4u:5u};
        testRequire(V==XMQTT_VERSION_5?xmqttPacketWrite5(Wires[I],256,&Q,I<3?XMQTT_CLIENT_TO_SERVER:XMQTT_SERVER_TO_CLIENT,NULL,&Sizes[I]):xmqttPacketWrite311(Wires[I],256,&Q,I<3?XMQTT_CLIENT_TO_SERVER:XMQTT_SERVER_TO_CLIENT,NULL,&Sizes[I]),"import canonical source packet");}
    O[2]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUT,.Flags=3,.Identifier=7,.Epoch=1,.Key=1,.Data={Wires[0],Sizes[0]}};
    O[3]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUT,.Flags=3,.Identifier=8,.Epoch=1,.Key=2,.Data={Wires[1],Sizes[1]}};
    O[4]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUBREL,.Identifier=8,.Epoch=1,.Key=2,.Reason=V==XMQTT_VERSION_5?0x10:0};
    O[5]=(xmqttstoredelta){.Type=XMQTT_STORE_OUT_PUT,.Identifier=9,.Epoch=1,.Key=3,.Data={Wires[2],Sizes[2]}};
    for(unsigned I=6;I<8;++I)O[I]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Identifier=(uint16)(I+11),.Epoch=1,.Key=I-2,.Data={Wires[I-3],Sizes[I-3]}};
    O[8]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_PUBREL,.Identifier=17,.Epoch=1,.Key=4};
    O[9]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=1,.Expiry=O[1].Expiry,.WallMilliseconds=2000};
    xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=jpCreate(F,&JC);xmqttstoredeltaconfig D=sdConfig(V);xmqttproperty Scratch[8];unsigned char Buffer[4096];size_t N=0;
    sdStatus(xmqttStoreDeltaWrite(&D,O,Live?9:10,Scratch,8,Buffer,sizeof(Buffer),&N),XMQTT_STORE_DELTA_DONE,"native import source transaction");uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){Buffer,N},&Seq),XMQTT_JOURNAL_DONE,"import source durably committed");testRequire(xmqttJournalDestroy(J),"source writer released");J=jpReopen(F,&JC);
    xmqttstorestateconfig C=stConfig(V);C.WallMilliseconds=3000;xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&C,&S),XMQTT_STORE_DONE,"complete-prefix import source");testRequire(xmqttJournalDestroy(J),"source journal released");jpFileClose(F);memset(Wires,0xcc,sizeof(Wires));memset(Buffer,0xcc,sizeof(Buffer));return S;
}
static inline xmqttsessionstatus ssOpen(xmqttsession* S,xmqttversion V,bool Present,bool Clean,double Now)
{xmqttpacket C,A;xmqttPacketInit(XMQTT_PACKET_CONNECT,&C);xmqttPacketInit(XMQTT_PACKET_CONNACK,&A);C.Body.Connect.ClientId=XRT_STR_LITERAL("delta-client");C.Body.Connect.CleanStart=Clean;A.SessionPresent=Present;xmqttproperty P={0};P.Id=XMQTT_PROPERTY_SESSION_EXPIRY;P.Type=XMQTT_PROPERTY_UINT32;P.Value.Number=60;if(V==XMQTT_VERSION_5)C.Properties=(xmqttpropertylist){&P,1};xmqttnegotiated N;return xmqttSessionOpen(S,&C,&A,Now,&N);}
static inline void ssClose(xmqttsession* S,double Now)
{if(ssInfo(S).Connected)ssStatus(xmqttSessionEnd(S,NULL,Now),XMQTT_SESSION_DONE,"actual passive Session termination");testRequire(xmqttSessionDestroy(S),"passive Session unique owner retired");}
#endif
