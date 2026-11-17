#ifndef XMQTT_TEST_STORE_STATE_PEER_H
#define XMQTT_TEST_STORE_STATE_PEER_H
#include "store_delta_peer.h"
#include "journal_peer.h"
#include <xmqtt/store_state.h>
enum {ST_COUNT=21};
typedef struct stsource {sdsource Packets;xmqttstoredelta Ops[ST_COUNT];} stsource;
static inline void stStatus(xmqttstorestatus Got,xmqttstorestatus Want,cstr Why)
{if(Got!=Want)fprintf(stderr,"state status%d expected%d %s error%s\n",Got,Want,Why,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");testRequire(Got==Want,Why);}
static inline xmqttjournalconfig stJournalConfig(xmqttversion V)
{xmqttjournalconfig C=jpConfig();C.Version=V;C.Binding=XRT_BYTES_LITERAL("state/broker/principal/slot");C.Generation=9;C.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;C.MaxTransactionBytes=SD_BUFFER;return C;}
static inline xmqttstorestateconfig stConfig(xmqttversion V)
{xmqttstorestateconfig C;testRequire(xmqttStoreStateConfigInit(&C),"state config");C.Delta.Version=V;C.Binding=XRT_BYTES_LITERAL("state/broker/principal/slot");C.Generation=9;C.ClientId=XRT_STR_LITERAL("delta-client");C.WallMilliseconds=100000;C.MaxMemoryBytes=262144;C.MaxOutgoing=8;C.MaxIncoming=8;C.MaxInbox=8;return C;}
static inline void stSource(xmqttversion V,stsource* S)
{
    sdSource(V,&S->Packets);memcpy(S->Ops,S->Packets.Ops,12*sizeof(*S->Ops));uint32 Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX;
    S->Ops[12]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=1002,.Expiry=Expiry};
    S->Ops[13]=(xmqttstoredelta){.Type=XMQTT_STORE_CLEAR,.Epoch=1,.WallMilliseconds=1003};
    S->Ops[14]=(xmqttstoredelta){.Type=XMQTT_STORE_OPEN,.Epoch=2,.WallMilliseconds=1004,.Expiry=Expiry};
    S->Ops[15]=(xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=2,.Key=23,.Data={S->Packets.Wire[3],S->Packets.Sizes[3]}};
    for(unsigned I=16;I<19;++I)S->Ops[I]=(xmqttstoredelta){.Type=XMQTT_STORE_INBOX_ACK,.Epoch=I==18?2:1,.Key=21+I-16};
    S->Ops[19]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=2,.WallMilliseconds=1005,.Expiry=Expiry};
    S->Ops[20]=(xmqttstoredelta){.Type=XMQTT_STORE_CLEAR,.Epoch=2,.WallMilliseconds=100000,.Reason=V==XMQTT_VERSION_5?1:0};
}
static inline xmqttjournal* stWrite(jpfile F,xmqttversion V,const xmqttstoredelta* Ops,size_t Count,bool Split)
{
    xmqttjournalconfig JC=stJournalConfig(V);xmqttstoredeltaconfig C=sdConfig(V);xmqttjournal* J=jpCreate(F,&JC);xmqttproperty P[4];unsigned char B[SD_BUFFER];size_t At=0;
    while(At<Count){size_t N=0,Take=Split?1:Count-At;sdStatus(xmqttStoreDeltaWrite(&C,Ops+At,Take,P,4,B,sizeof(B),&N),XMQTT_STORE_DELTA_DONE,"syntactically valid committed fixture");uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){B,N},&Seq),XMQTT_JOURNAL_DONE,"actual typed transaction synced");At+=Take;}
    testRequire(xmqttJournalDestroy(J),"fixture writer released");J=NULL;jpStatus(xmqttJournalOpen(F.File,&JC,&J),XMQTT_JOURNAL_DONE,"untouched replay cursor");return J;
}
static inline xmqttstorestateinfo stInfo(const xmqttstorestate* S)
{xmqttstorestateinfo I;testRequire(xmqttStoreStateInfo(S,&I),"immutable recovery info");return I;}
static inline xmqttstoreview stView(const xmqttstorestate* S,xmqttstoreviewkind K,size_t Index)
{xmqttstoreview V;stStatus(xmqttStoreStateView(S,K,Index,&V),XMQTT_STORE_DONE,"immutable ordered wire view");return V;}
#endif
