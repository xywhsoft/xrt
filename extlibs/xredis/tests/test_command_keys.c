#include "test.h"
#include "support/command_keys_fixture.h"
#include "support/command_keys_invalid.h"
#include <xrt/memory.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <string.h>
static xredisreply* ckReply(cstr Wire,size_t Fragment)
{
    xredisreplyreader* Reader=NULL;xredisreply* R=NULL;testRequire(xredisReplyReaderCreate(NULL,&Reader),"key metadata reader");size_t At=0,Bytes=strlen(Wire);
    while(At<Bytes){size_t N=Bytes-At;if(N>Fragment)N=Fragment;size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire+At,N},&Used,&R);
        testRequire(S!=XREDIS_REPLY_ERROR && (Used || S==XREDIS_REPLY_YIELD),"key metadata split");At+=Used;if(R){testRequire(At==Bytes,"single metadata reply");break;}}
    testRequire(R && xredisReplyReaderFinish(Reader),"complete key metadata");xredisReplyReaderDestroy(Reader);return R;
}
static xrediscommandcatalog* ckCatalog(cstr Wire,const xrediscommandkeylimits* K)
{xredisreply* R=ckReply(Wire,37);xrediscommandcatalog* C=NULL;testRequire(xredisCommandCatalogCreate(R,K,&C)==XREDIS_KEYS_OK,"key catalogue create");xredisReplyRelease(R);return C;}
static uint16 ckSlot(xbytesview A)
{
    size_t First=0,Last=A.Size;
    for(size_t I=0;I<A.Size;++I)if(A.Data[I]=='{'){for(size_t J=I+1;J<A.Size;++J)if(A.Data[J]=='}'){if(J>I+1){First=I+1;Last=J;}goto found;}break;}
found:;
    uint32 N=0;for(size_t I=First;I<Last;++I){N^=(uint32)A.Data[I]<<8;for(unsigned J=0;J<8;++J)N=((N<<1)^((N&32768)?0x1021u:0))&65535u;}return (uint16)(N%16384);
}
static xrediskeysstatus ckEvaluate(xrediscommandcatalog* C,const cstr* Words,size_t Count,const size_t* Expected,size_t Keys,xrediskeysstatus Status)
{
    xbytesview A[260];testRequire(Count<=260,"fixture args");for(size_t I=0;I<Count;++I)A[I]=(xbytesview){(cbytes)Words[I],strlen(Words[I])};
    xrediscommandkey Positions[260],Before[260];memset(Positions,0x6c,sizeof(Positions));memcpy(Before,Positions,sizeof(Before));xrediscommandkeyresult R;memset(&R,0x5c,sizeof(R));R.Size=sizeof(R);xrediscommandkeyresult Old=R;
    xrediskeysstatus S=xredisCommandCatalogEvaluate(C,A,Count,Positions,260,&R);testRequire(S==Status,"key classification");
    if(S==XREDIS_KEYS_OK || S==XREDIS_KEYS_NONE){testRequire(R.Keys==Keys && R.HasSlot==(Keys!=0),"key count/slot presence");for(size_t I=0;I<Keys;++I)testRequire(Positions[I].Argument==Expected[I] && R.Slot==ckSlot(A[Expected[I]]),"independent key positions/CRC");
        xrediscommandkeyresult Q={0};Q.Size=sizeof(Q);testRequire(xredisCommandCatalogEvaluate(C,A,Count,NULL,0,&Q)==S && Q.Command==R.Command && Q.Keys==R.Keys && Q.Slot==R.Slot,"slot-only query");}
    else testRequire(!memcmp(&R,&Old,sizeof(R)) && !memcmp(Before,Positions,sizeof(Before)),"failed extraction changed output");
    return S;
}
static void ckGolden(void)
{
    for(unsigned Version=0;Version<2;++Version){xrediscommandcatalog* C=ckCatalog(Version?CK_METADATA3:CK_METADATA2,NULL);xrediscommandcataloginfo Info={0};Info.Size=sizeof(Info);testRequire(xredisCommandCatalogInfo(C,&Info) && Info.Commands==20 && Info.LookupDepth==3,"flat subcommand catalogue");
        const size_t One[]={1},Two[]={1,3},Eval[]={3,4},Union[]={1,3,4},Stream[]={2,3},Reverse[]={4,5},Mod[]={3},Partial[]={2},Repeat[]={1,1};
        const cstr Get[]={"gEt","a{tag}"};ckEvaluate(C,Get,2,One,1,XREDIS_KEYS_OK);
        const cstr Mset[]={"MSET","a{tag}","x","b{tag}","y"};ckEvaluate(C,Mset,5,Two,2,XREDIS_KEYS_OK);
        const cstr E[]={"EVAL","return 1","2","a{tag}","b{tag}","argument"};ckEvaluate(C,E,6,Eval,2,XREDIS_KEYS_OK);
        const cstr Z[]={"zunionstore","out{tag}","2","a{tag}","b{tag}","weights","1","2"};ckEvaluate(C,Z,8,Union,3,XREDIS_KEYS_OK);
        const cstr X[]={"xread","sTrEaMs","a{tag}","b{tag}","0","0"};ckEvaluate(C,X,6,Stream,2,XREDIS_KEYS_OK);
        const cstr K[]={"krev","KEYS","noise","keys","a{tag}","b{tag}"};ckEvaluate(C,K,6,Reverse,2,XREDIS_KEYS_OK);
        const cstr N[]={"eval","return 1","0","not-a-key"};ckEvaluate(C,N,4,NULL,0,XREDIS_KEYS_NONE);
        const cstr Nested[]={"mod","ADMIN","READ","{tag}"};ckEvaluate(C,Nested,4,Mod,1,XREDIS_KEYS_OK);
        const cstr P[]={"partial","read","{tag}"};ckEvaluate(C,P,3,Partial,1,XREDIS_KEYS_OK);
        const cstr Config[]={"config","GET","timeout"};ckEvaluate(C,Config,3,NULL,0,XREDIS_KEYS_NONE);
        const cstr UnknownSub[]={"config","v99"};ckEvaluate(C,UnknownSub,2,NULL,0,XREDIS_KEYS_UNKNOWN);
        const cstr Channel[]={"spublish","{tag}","data"};ckEvaluate(C,Channel,3,One,1,XREDIS_KEYS_OK);
        const cstr Repeated[]={"repeated","{tag}"};ckEvaluate(C,Repeated,2,Repeat,2,XREDIS_KEYS_OK);
        for(unsigned Kind=0;Kind<4;++Kind){const cstr Names[]={"sort","legacy_dynamic","future","future_flags"};const cstr U[]={Names[Kind],"{tag}"};ckEvaluate(C,U,2,NULL,0,XREDIS_KEYS_UNSUPPORTED);}
        const cstr Unknown[]={"unknown","{tag}"};ckEvaluate(C,Unknown,2,NULL,0,XREDIS_KEYS_UNKNOWN);
        const cstr Cross[]={"mset","a{a}","x","b{b}","y"};ckEvaluate(C,Cross,5,NULL,0,XREDIS_KEYS_CROSS_SLOT);
        const cstr Short[]={"get"};ckEvaluate(C,Short,1,NULL,0,XREDIS_KEYS_ARGUMENTS);
        const cstr Overflow[]={"eval","return 1","18446744073709551616","{tag}"};ckEvaluate(C,Overflow,4,NULL,0,XREDIS_KEYS_ARGUMENTS);
        const cstr Canonical[]={"eval","return 1","01","{tag}"};ckEvaluate(C,Canonical,4,NULL,0,XREDIS_KEYS_ARGUMENTS);
        xbytesview Binary[]={XRT_BYTES_LITERAL("get"),XRT_BYTES_LITERAL("a\0{123456789}\xff")};xrediscommandkeyresult R={0};R.Size=sizeof(R);testRequire(xredisCommandCatalogEvaluate(C,Binary,2,NULL,0,&R)==XREDIS_KEYS_OK && R.Slot==12739 && R.Legacy,"binary key/legacy metadata");
        xbytesview Set[]={XRT_BYTES_LITERAL("set"),XRT_BYTES_LITERAL("{tag}"),XRT_BYTES_LITERAL("x")};xrediscommandkey Key;R.Size=sizeof(R);testRequire(xredisCommandCatalogEvaluate(C,Set,3,&Key,1,&R)==XREDIS_KEYS_OK && R.VariableFlags && Key.Flags==(XREDIS_KEY_RW|XREDIS_KEY_ACCESS|XREDIS_KEY_UPDATE|XREDIS_KEY_VARIABLE),"possible variable access flags");
        xredisCommandCatalogRelease(C);}
}
static void ckModels(void)
{
    xrediscommandcatalog* C=ckCatalog(CK_METADATA3,NULL);char Count[24];cstr Words[260];size_t Expected[260];
    for(size_t N=1;N<=64;++N){snprintf(Count,sizeof(Count),"%zu",N);
        Words[0]="EVAL";Words[1]="return 1";Words[2]=Count;for(size_t I=0;I<N;++I){Words[3+I]="stream{123456789}";Expected[I]=3+I;}Words[3+N]="argument";ckEvaluate(C,Words,N+4,Expected,N,XREDIS_KEYS_OK);
        Words[0]="MSET";for(size_t I=0;I<N;++I){Words[1+2*I]="key{123456789}";Words[2+2*I]="value";Expected[I]=1+2*I;}ckEvaluate(C,Words,2*N+1,Expected,N,XREDIS_KEYS_OK);
        Words[0]="XREAD";Words[1]="COUNT";Words[2]="7";Words[3]="streams";for(size_t I=0;I<N;++I){Words[4+I]="stream{123456789}";Words[4+N+I]="0-0";Expected[I]=4+I;}ckEvaluate(C,Words,2*N+4,Expected,N,XREDIS_KEYS_OK);
    }
    xredisCommandCatalogRelease(C);
}
static void ckAtomicAndLimits(void)
{
    xredisreply* Reply=ckReply(CK_METADATA2,1);xrediscommandcatalog* C=NULL;testRequire(xredisCommandCatalogCreate(Reply,NULL,&C)==XREDIS_KEYS_OK,"split metadata catalogue");xrediscommandcataloginfo I={0};I.Size=sizeof(I);xredisCommandCatalogInfo(C,&I);
    xrediscommandkeylimits K;xredisCommandKeyLimitsInit(&K);K.MaxOwnedBytes=I.StorageBytes+I.RetainedReplyBytes;
    xrediscommandcatalog* Exact=NULL;testRequire(xredisCommandCatalogCreate(Reply,&K,&Exact)==XREDIS_KEYS_OK,"exact retained storage bound");xredisCommandCatalogRelease(Exact);--K.MaxOwnedBytes;Exact=(void*)(uintptr_t)1;
    testRequire(xredisCommandCatalogCreate(Reply,&K,&Exact)==XREDIS_KEYS_LIMIT && Exact==(void*)(uintptr_t)1,"retained storage cap atomicity");
    xbytesview A[]={XRT_BYTES_LITERAL("mset"),XRT_BYTES_LITERAL("a{tag}"),XRT_BYTES_LITERAL("x"),XRT_BYTES_LITERAL("b{tag}"),XRT_BYTES_LITERAL("y")};
    xrediscommandkeyresult R={0};R.Size=sizeof(R);xrediscommandkeyresult Old=R;xrediscommandkey Keys[2];memset(Keys,0x72,sizeof(Keys));xrediscommandkey Before[2];memcpy(Before,Keys,sizeof(Keys));
    testRequire(xredisCommandCatalogEvaluate(C,A,5,Keys,1,&R)==XREDIS_KEYS_LIMIT && !memcmp(&R,&Old,sizeof(R)) && !memcmp(Keys,Before,sizeof(Keys)),"short output has no partial keys");
    testRequire(xredisCommandCatalogEvaluate(C,A,5,(void*)&R,1,&R)==XREDIS_KEYS_ERROR && !memcmp(&R,&Old,sizeof(R)),"key/result alias");xrtClearError();
    testRequire(!xredisCommandCatalogInfo(C,(void*)C),"info cannot overwrite catalogue");xrtClearError();
    for(unsigned Kind=0;Kind<6;++Kind){xredisCommandKeyLimitsInit(&K);if(Kind==0)K.MaxCommands=1;else if(Kind==1)K.MaxSpecifications=1;else if(Kind==2)K.MaxDepth=1;else if(Kind==3)K.MaxReplyNodes=1;else if(Kind==4)K.MaxPayloadBytes=1;else K.MaxSearchWork=1;Exact=(void*)(uintptr_t)1;testRequire(xredisCommandCatalogCreate(Reply,&K,&Exact)==XREDIS_KEYS_LIMIT && Exact==(void*)(uintptr_t)1,"metadata limits preserve output");}
    for(unsigned Kind=0;Kind<3;++Kind){xredisCommandKeyLimitsInit(&K);if(Kind==0)K.MaxKeyBytes=1;else if(Kind==1)K.MaxKeys=1;else K.MaxArgumentBytes=1;Exact=NULL;testRequire(xredisCommandCatalogCreate(Reply,&K,&Exact)==XREDIS_KEYS_OK,"runtime-limited catalogue");
        testRequire(xredisCommandCatalogEvaluate(Exact,A,5,NULL,0,&R)==XREDIS_KEYS_LIMIT && !memcmp(&R,&Old,sizeof(R)),"runtime budget atomicity");xredisCommandCatalogRelease(Exact);}
    xredisCommandCatalogRelease(C);xredisReplyRelease(Reply);
}
static void ckOOMAndWarm(void)
{
    xredisreply* Reply=ckReply(CK_METADATA3,7);unsigned Failed=0,Accepted=0;
    for(size_t Limit=1;Limit<16384;Limit+=127){xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"catalogue OOM scope");xrediscommandcatalog* C=(void*)(uintptr_t)1;xrediskeysstatus Status=xredisCommandCatalogCreate(Reply,NULL,&C);testRequire(xrtAllocationBudgetLeave(&S),"catalogue OOM leave");
        if(Status==XREDIS_KEYS_OK){++Accepted;xredisCommandCatalogRelease(C);}else{++Failed;testRequire(Status==XREDIS_KEYS_MEMORY && C==(void*)(uintptr_t)1,"catalogue OOM output");xrtClearError();}
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"catalogue OOM retirement");xrtAllocationBudgetFree(B);}
    testRequire(Accepted && Failed,"catalogue OOM boundaries exercised");xrediscommandcatalog* C=NULL;testRequire(xredisCommandCatalogCreate(Reply,NULL,&C)==XREDIS_KEYS_OK,"warm catalogue");xredisReplyRelease(Reply);
    xrtSetErrorKind(XERR_STATE);const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&S),"warm exhausted quota");
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);xbytesview A[]={XRT_BYTES_LITERAL("get"),XRT_BYTES_LITERAL("a{tag}")};
    for(unsigned I=0;I<10000;++I){xrediscommandkeyresult R={0};R.Size=sizeof(R);testRequire(xredisCommandCatalogEvaluate(C,A,2,NULL,0,&R)==XREDIS_KEYS_OK && xrtGetError()==Error,"warm extraction changed ambient error");}
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"warm extraction allocated");testRequire(xrtAllocationBudgetLeave(&S),"warm quota leave");
    xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.Rejections,"warm extraction budget");xrtAllocationBudgetFree(B);xrtClearError();xredisCommandCatalogRelease(C);
}
typedef struct ckowner {xrediscommandcatalog* C;} ckowner;
static int32 ckReader(ptr Data)
{ckowner* O=Data;xbytesview A[]={XRT_BYTES_LITERAL("get"),XRT_BYTES_LITERAL("{123456789}")};for(unsigned I=0;I<12000;++I){xrediscommandkeyresult R={0};R.Size=sizeof(R);testRequire(xredisCommandCatalogEvaluate(O->C,A,2,NULL,0,&R)==XREDIS_KEYS_OK && R.Slot==12739 && xredisCommandCatalogRetain(O->C),"concurrent immutable catalogue");xredisCommandCatalogRelease(O->C);}xredisCommandCatalogRelease(O->C);return 0;}
static void ckConcurrent(void)
{ckowner O={ckCatalog(CK_METADATA2,NULL)};xthread* T[4];for(unsigned I=0;I<4;++I){testRequire(xredisCommandCatalogRetain(O.C),"concurrent catalogue retain");T[I]=xrtThreadCreate(ckReader,&O,0);testRequire(T[I]!=NULL,"catalogue reader thread");}for(unsigned I=0;I<4;++I){testRequire(xrtThreadWaitFor(T[I],10000)==XWAIT_OK,"catalogue reader join");xrtThreadDestroy(T[I]);}xredisCommandCatalogRelease(O.C);}
static void ckMalformedAndCollisions(void)
{
    for(size_t I=0;I<sizeof(CK_BAD_METADATA)/sizeof(*CK_BAD_METADATA);++I){xredisreply* R=ckReply(CK_BAD_METADATA[I],13);xrediscommandcatalog* C=(void*)(uintptr_t)1;
        testRequire(xredisCommandCatalogCreate(R,NULL,&C)==XREDIS_KEYS_MALFORMED && C==(void*)(uintptr_t)1,"late/duplicate/typed malformed metadata publication");xredisReplyRelease(R);}
    xredisreply* R=ckReply(CK_COLLISIONS,31);xrediscommandcatalog* C=NULL;testRequire(xredisCommandCatalogCreate(R,NULL,&C)==XREDIS_KEYS_OK,"bounded collision catalogue");xredisCommandCatalogRelease(C);
    xrediscommandkeylimits K;xredisCommandKeyLimitsInit(&K);K.MaxSearchWork=4096;C=(void*)(uintptr_t)1;xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(32768,&B) && xrtAllocationBudgetEnter(B,&Scope),"collision budget");
    testRequire(xredisCommandCatalogCreate(R,&K,&C)==XREDIS_KEYS_LIMIT && C==(void*)(uintptr_t)1 && xrtAllocationBudgetLeave(&Scope),"collision probes bypassed work budget");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveCount && !Info.LiveBytes,"collision index rollback");xrtAllocationBudgetFree(B);xredisReplyRelease(R);
    R=ckReply("-NOPERM metadata\r\n",2);C=(void*)(uintptr_t)1;testRequire(xredisCommandCatalogCreate(R,NULL,&C)==XREDIS_KEYS_SERVER_ERROR && C==(void*)(uintptr_t)1,"server error was parsed as keys");xredisReplyRelease(R);
    C=ckCatalog("*1\r\n*6\r\n+get\r\n:2\r\n*0\r\n:1\r\n:1\r\n:1\r\n",NULL);xbytesview A[]={XRT_BYTES_LITERAL("get"),{NULL,0}};xrediscommandkeyresult V={0};V.Size=sizeof(V);testRequire(xredisCommandCatalogEvaluate(C,A,2,NULL,0,&V)==XREDIS_KEYS_OK && V.HasSlot && !V.Slot,"empty binary key is not keyless");
    xrediscommandkeyresult Before=V;A[1]=(xbytesview){(cbytes)&V,sizeof(V)};testRequire(xredisCommandCatalogEvaluate(C,A,2,NULL,0,&V)==XREDIS_KEYS_ERROR && !memcmp(&Before,&V,sizeof(V)),"argument/result alias");xrtClearError();
    A[1]=(xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1),8};testRequire(xredisCommandCatalogEvaluate(C,A,2,NULL,0,&V)==XREDIS_KEYS_ERROR && !memcmp(&Before,&V,sizeof(V)),"wrapped argument span");xrtClearError();xredisCommandCatalogRelease(C);
}
static void ckMutations(void)
{
    xredisreplyreaderconfig K;xredisReplyReaderConfigInit(&K);K.MaxOwnedBytes=131072;K.Reader.MaxFrameBytes=65536;K.Reader.MaxBulkBytes=32768;K.Reader.MaxLineBytes=1024;K.Reader.MaxValues=8192;K.Reader.MaxContainerItems=8192;
    unsigned char Bytes[sizeof(CK_METADATA3)];uint32 State=6716;unsigned Accepted=0,Rejected=0;
    for(unsigned I=0;I<7000;++I){memcpy(Bytes,CK_METADATA3,sizeof(Bytes));size_t Size=sizeof(Bytes)-1;
        if(I%64){for(unsigned J=0;J<1+I%9;++J){State=State*1664525u+1013904223u;size_t At=State%Size;State=State*1664525u+1013904223u;Bytes[At]=(unsigned char)(State>>24);}if(I%3==0)Size=I%Size;}
        xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;testRequire(xredisReplyReaderCreate(&K,&Reader),"mutation reader");size_t At=0;
        while(At<Size && !Reply){size_t Used=0;xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){Bytes+At,Size-At},&Used,&Reply);At+=Used;if(S==XREDIS_REPLY_ERROR)break;testRequire(Used || S==XREDIS_REPLY_YIELD,"mutation parser progress");}
        if(Reply){xrediscommandcatalog* C=(void*)(uintptr_t)1;xrediskeysstatus S=xredisCommandCatalogCreate(Reply,NULL,&C);if(S==XREDIS_KEYS_OK){++Accepted;xredisCommandCatalogRelease(C);}else {++Rejected;testRequire(C==(void*)(uintptr_t)1,"mutation partial catalogue");}xredisReplyRelease(Reply);}
        else ++Rejected;
        xredisReplyReaderDestroy(Reader);xrtClearError();
    }
    testRequire(Accepted && Rejected,"mutation acceptance/error branches");
}
#if defined(XREDIS_KEYS_WHITEBOX)
static void ckReferenceBounds(void)
{
    xrediscommandcatalog* C=ckCatalog(CK_METADATA2,NULL);xrtAtomic32Store(&C->References,UINT32_MAX,XMEMORY_RELAXED);testRequire(!xredisCommandCatalogRetain(C) && xrtAtomic32Load(&C->References,XMEMORY_RELAXED)==UINT32_MAX,"catalogue reference wrap");xrtAtomic32Store(&C->References,1,XMEMORY_RELAXED);xrtClearError();xredisCommandCatalogRelease(C);
    xredisreply* R=ckReply("*0\r\n",1);xrtAtomic32Store(&R->References,UINT32_MAX,XMEMORY_RELAXED);C=(void*)(uintptr_t)1;testRequire(xredisCommandCatalogCreate(R,NULL,&C)==XREDIS_KEYS_ERROR && C==(void*)(uintptr_t)1,"catalogue adopted overflowing reply");xrtAtomic32Store(&R->References,1,XMEMORY_RELAXED);xrtClearError();xredisReplyRelease(R);
}
#endif
int main(void)
{ckGolden();ckModels();ckAtomicAndLimits();ckOOMAndWarm();ckConcurrent();ckMalformedAndCollisions();ckMutations();
#if defined(XREDIS_KEYS_WHITEBOX)
ckReferenceBounds();
#endif
puts("[PASS] command key metadata: modern/legacy/subcommands, binary CRC, complete extraction, cross-slot, unsupported rules, atomic limits, OOM, mutations, bounded collisions and shared immutable lifetime");return 0;}
