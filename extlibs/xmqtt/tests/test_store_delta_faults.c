#include "support/store_delta_peer.h"
static void sfHandler(const xerror* Error,ptr Data){(void)Error;++*(unsigned*)Data;}
static void sfQuietSuccess(void)
{
    xmqttstoredeltaconfig C=sdConfig(XMQTT_VERSION_5);sdsource S;sdSource(C.Version,&S);unsigned char B[SD_BUFFER];size_t N=0;xmqttproperty P[4];unsigned Calls=0;
    xrtSetErrorInfo(XERR_STATE,"old.ambient.handler",1,"must not be re-notified");const xerror* Error=xrtGetError();xrtSetErrorHandler(sfHandler,&Calls);
    xmqttstoredeltastatus Status=xmqttStoreDeltaWrite(&C,S.Ops,SD_COUNT,P,4,B,sizeof(B),&N);xrtSetErrorHandler(NULL,NULL);
    printf("[AMBIENT-HANDLER] successful write callbacks=%u\n",Calls);sdStatus(Status,XMQTT_STORE_DELTA_DONE,"ambient-handler reproduction");testRequire(xrtGetError()==Error&&!Calls,"successful validation must not re-notify an existing ambient error");xrtClearError();
}
static xmqttstoredeltastatus sfRead(const xmqttstoredeltaconfig* C,xbytesview Input)
{xmqttstoredelta O[SD_COUNT];xmqttproperty P[4];xmqttstoredeltadocument D={(xmqttstoredelta*)(uintptr_t)1,777,{NULL,99}},Before=D;xmqttstoredeltastatus S=xmqttStoreDeltaRead(C,Input,O,SD_COUNT,P,4,&D);testRequire(S==XMQTT_STORE_DELTA_DONE?D.Count<=SD_COUNT:!memcmp(&D,&Before,sizeof(D)),"reader document atomic on every failure/LIMIT");xrtClearError();return S;}
static void sfWriteError(xmqttstoredeltaconfig* C,xmqttstoredelta* O,xmqttstoredeltastatus Want)
{unsigned char B[SD_BUFFER],Before[SD_BUFFER];memset(B,0xcc,sizeof(B));memcpy(Before,B,sizeof(B));size_t N=777;xmqttproperty P[4];sdStatus(xmqttStoreDeltaWrite(C,O,1,P,4,B,sizeof(B),&N),Want,"invalid typed operation");testRequire(N==777&&!memcmp(B,Before,sizeof(B)),"failed writer output and count untouched");xrtClearError();}
static void sfFields(xmqttversion V)
{
    sdsource S;sdSource(V,&S);xmqttstoredeltaconfig C=sdConfig(V);xmqttstoredelta O;
    for(unsigned I=0;I<SD_COUNT;++I){O=S.Ops[I];O.Epoch=0;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[I];O.WallMilliseconds=UINT64_MAX;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);}
    O=S.Ops[0];O.Data=XRT_BYTES_LITERAL("a\0b");sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[0];O.Key=1;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    O=S.Ops[1];O.Flags=3;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);if(V==XMQTT_VERSION_311){O=S.Ops[1];O.Expiry=1;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);}
    O=S.Ops[12];O.Flags=2;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[12];O.Reason=0x8d;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    O=S.Ops[13];O.Reason=3;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[13];O.Data=XRT_BYTES_LITERAL("hidden");sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    for(unsigned I=2;I<SD_COUNT;++I){if(I==12||I==13)continue;O=S.Ops[I];O.Key=0;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[I];O.Expiry=1;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);}
    O=S.Ops[2];O.Flags=2;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[2];O.Identifier=8;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    O=S.Ops[5];O.Reason=0x80;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[7];O.Flags=XMQTT_PACKET_PUBREC;O.Reason=0;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    O=S.Ops[7];O.Reason=0x8d;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);O=S.Ops[14];O.Identifier=17;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    O=S.Ops[2];O.Data=XRT_BYTES_LITERAL("\xc0\0");sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    if(V==XMQTT_VERSION_5){O=S.Ops[7];O.Flags=XMQTT_PACKET_PUBREC;O.Reason=0x80;xmqttproperty P[4];size_t N=0;sdStatus(xmqttStoreDeltaSize(&C,&O,1,P,4,&N),XMQTT_STORE_DELTA_DONE,"negative PUBREC is completion rather than PUBREL");
        O=S.Ops[2];unsigned char B[SD_WIRE];xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=2;Q.Identifier=7;Q.Body.Publish.Topic=XRT_STR_LITERAL("delta/topic");xmqttproperty Alias={0};Alias.Id=XMQTT_PROPERTY_TOPIC_ALIAS;Alias.Type=XMQTT_PROPERTY_UINT16;Alias.Value.Number=1;Q.Properties=(xmqttpropertylist){&Alias,1};testRequire(xmqttPacketWrite5(B,sizeof(B),&Q,XMQTT_CLIENT_TO_SERVER,NULL,&N),"legal full-topic alias input");O.Data=(xbytesview){B,N};sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
        O=S.Ops[2];memcpy(B,O.Data.Data,O.Data.Size);B[O.Data.Size-1]=0xff;O.Data.Data=B;sfWriteError(&C,&O,XMQTT_STORE_DELTA_PROTOCOL);
    }
}
static void sfBounds(xmqttversion V)
{
    xmqttstoredeltaconfig C=sdConfig(V);sdsource S;sdSource(V,&S);unsigned char B[SD_BUFFER];size_t N=sdWrite(&C,&S,B,sizeof(B));xmqttproperty P[4];xmqttstoredelta Ops[SD_COUNT];xmqttstoredeltadocument D={(xmqttstoredelta*)(uintptr_t)1,777,{0}},Before=D;size_t Count=999;
    C.MaxOperations=15;sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){B,N},Ops,SD_COUNT,P,4,&D),XMQTT_STORE_DELTA_LIMIT,"operation cap");testRequire(!memcmp(&D,&Before,sizeof(D)),"count quota document unchanged");C=sdConfig(V);C.MaxBytes=N-1;sdStatus(xmqttStoreDeltaSize(&C,S.Ops,SD_COUNT,P,4,&Count),XMQTT_STORE_DELTA_LIMIT,"byte cap no arithmetic underflow");testRequire(Count==999,"byte cap measure output");C=sdConfig(V);
    xrtSetErrorInfo(XERR_STATE,"delta.ambient",17,"preserve");const xerror* Error=xrtGetError();
    sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){B,N},Ops,SD_COUNT-1,P,4,&D),XMQTT_STORE_DELTA_LIMIT,"caller operation scratch");testRequire(xrtGetError()==Error,"record capacity preserves ambient");
    if(V==XMQTT_VERSION_5){sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){B,N},Ops,SD_COUNT,P,2,&D),XMQTT_STORE_DELTA_LIMIT,"native property scratch refusal");testRequire(xrtGetError()==Error,"native capacity diagnostic discarded without changing ambient error identity");}
    sdStatus(xmqttStoreDeltaWrite(&C,S.Ops,SD_COUNT,P,4,B,N-1,&Count),XMQTT_STORE_DELTA_LIMIT,"wire output capacity");testRequire(Count==999&&xrtGetError()==Error,"wire capacity unchanged");
    xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"actual zero-heap budget");
    sdStatus(xmqttStoreDeltaSize(&C,S.Ops,SD_COUNT,P,4,&Count),XMQTT_STORE_DELTA_DONE,"budget measure");sdStatus(xmqttStoreDeltaWrite(&C,S.Ops,SD_COUNT,P,4,B,sizeof(B),&Count),XMQTT_STORE_DELTA_DONE,"budget writer");sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){B,N},Ops,SD_COUNT,P,4,&D),XMQTT_STORE_DELTA_DONE,"budget reader");testRequire(xrtGetError()==Error,"all successful APIs preserve ambient");testRequire(xrtAllocationBudgetLeave(&Scope),"leave zero-heap budget");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&!BI.LiveCount&&!BI.LiveBytes&&!BI.PeakBytes&&!BI.Rejections,"successful codec calls attempt no XRT allocation");xrtAllocationBudgetFree(Budget);xrtClearError();
    xmqttstoredelta Bad=S.Ops[2];Bad.Flags=2;testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"diagnostic OOM scope");sfWriteError(&C,&Bad,XMQTT_STORE_DELTA_PROTOCOL);testRequire(xrtAllocationBudgetLeave(&Scope),"diagnostic OOM leave");BI=(xallocationbudgetinfo){.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(Budget,&BI)&&!BI.LiveCount&&!BI.LiveBytes,"diagnostic OOM changes neither typed status nor owners");xrtAllocationBudgetFree(Budget);
    sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){B,N},(xmqttstoredelta*)(void*)B,1,P,4,&D),XMQTT_STORE_DELTA_ERROR,"input/record scratch overlap");xrtClearError();
    sdStatus(xmqttStoreDeltaWrite(&C,S.Ops,SD_COUNT,(xmqttproperty*)(void*)S.Wire[0],1,B,sizeof(B),&Count),XMQTT_STORE_DELTA_ERROR,"scratch cannot mutate a source packet");xrtClearError();
    sdStatus(xmqttStoreDeltaWrite(&C,S.Ops,SD_COUNT,P,4,B,sizeof(B),(size_t*)(void*)B),XMQTT_STORE_DELTA_ERROR,"wire/count output overlap");xrtClearError();
    C.MaxBytes=0;sdStatus(xmqttStoreDeltaSize(&C,NULL,0,NULL,0,&Count),XMQTT_STORE_DELTA_LIMIT,"zero byte cap");C=sdConfig(V);C.MaxOperations=0;Bad.Data=(xbytesview){(const unsigned char*)UINTPTR_MAX,1};sdStatus(xmqttStoreDeltaSize(&C,&Bad,1,NULL,0,&Count),XMQTT_STORE_DELTA_LIMIT,"count quota refuses before traversing unusable deep body");
}
static void sfMutations(xmqttversion V)
{
    xmqttstoredeltaconfig C=sdConfig(V);sdsource S;sdSource(V,&S);unsigned char B[SD_BUFFER],X[SD_BUFFER];size_t N=sdWrite(&C,&S,B,sizeof(B));
    for(size_t I=0;I<N;++I)testRequire(sfRead(&C,(xbytesview){B,I})<0,"every truncated batch is invalid");
    const unsigned Positions[]={0,7,8,9,10,11,12,16,24,32,35,38,44,64,72};
    for(unsigned I=0;I<sizeof(Positions)/sizeof(*Positions);++I){memcpy(X,B,N);X[Positions[I]]^=0x80;testRequire(sfRead(&C,(xbytesview){X,N})<0,"known framing/reserved/epoch/body faults");}
    uint32 Random=0x197ad593u;unsigned Accepted=0;
    for(unsigned I=0;I<5000;++I){memcpy(X,B,N);Random=Random*1664525u+1013904223u;size_t At=Random%N;Random=Random*1664525u+1013904223u;X[At]^=(unsigned char)(1u<<(Random&7u));xmqttstoredeltastatus Status=sfRead(&C,(xbytesview){X,N});
        if(Status==XMQTT_STORE_DELTA_DONE){xmqttstoredelta Ops[SD_COUNT];xmqttproperty P[4];xmqttstoredeltadocument D;unsigned char Again[SD_BUFFER];size_t M=0;sdStatus(xmqttStoreDeltaRead(&C,(xbytesview){X,N},Ops,SD_COUNT,P,4,&D),XMQTT_STORE_DELTA_DONE,"accepted mutation reparses");sdStatus(xmqttStoreDeltaWrite(&C,Ops,D.Count,P,4,Again,sizeof(Again),&M),XMQTT_STORE_DELTA_DONE,"accepted mutation normalizes");testRequire(M==N&&!memcmp(X,Again,N),"valid altered application data is preserved; integrity belongs to journal CRC");++Accepted;}
    }
    printf("[MUTATIONS] version%d every%zu truncation and5000 seeded fields/body mutations accepted%u (no standalone integrity claim)\n",V,N,Accepted);
}
int main(void)
{sfQuietSuccess();for(unsigned I=0;I<2;++I){xmqttversion V=I?XMQTT_VERSION_311:XMQTT_VERSION_5;sfFields(V);sfBounds(V);sfMutations(V);}puts("[PASS] typed deltas semantic/bounds/alias/OOM/no-allocation/native-property/PFI checks");return 0;}
