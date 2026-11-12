#include <xredis/command_keys.h>
#include "../internal/cluster_reply.h"
#include "../internal/cluster_slot.h"
#include <xrt/memory.h>
#include <xrt/atomic.h>
#include <stdlib.h>
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
enum {XRK_INDEX=1,XRK_KEYWORD,XRK_RANGE,XRK_KEYNUM};
typedef struct xrediskeyspec {
    xbytesview Keyword;int64 Index,Last,Step,Limit,Number,First;
    uint32 Flags;unsigned Begin,Find;bool Unsupported;
} xrediskeyspec;
typedef struct xrediskeycommand {
    xbytesview Name;int64 Arity,First,Last,Step;size_t Spec,Count;
    bool Movable,Modern,Children,Unsupported;
} xrediskeycommand;
struct xrediscommandcatalog {
    xatomic32 References;xredisreply* Reply;xrediscommandkeylimits Limits;
    xrediscommandcataloginfo Info;size_t Buckets;
    xrediskeycommand* Commands;xrediskeyspec* Specs;uint32* Hash;
};
typedef struct xrediskeysbuild {
    xredisreply* Reply;const xrediscommandkeylimits* Limits;xrediscommandcatalog* Out;
    size_t Commands,Specs,Depth;
} xrediskeysbuild;
static bool xredisKeysError(cstr Text)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_REPLY_ARGUMENT,Text);return false;}
static unsigned xredisKeysLower(unsigned C){return C>='A' && C<='Z'?C+32:C;}
static bool xredisKeysEqual(xbytesview A,xbytesview B)
{if(A.Size!=B.Size)return false;for(size_t I=0;I<A.Size;++I)if(xredisKeysLower(A.Data[I])!=xredisKeysLower(B.Data[I]))return false;return true;}
static bool xredisKeysWord(xbytesview A,cstr B)
{return xredisKeysEqual(A,(xbytesview){(cbytes)B,strlen(B)});}
static bool xredisKeysText(xredisreplyvalue V)
{return (V.Type==XREDIS_REPLY_SIMPLE_STRING || V.Type==XREDIS_REPLY_BULK_STRING) && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisKeysArray(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_ARRAY && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisKeysSequence(xredisreplyvalue V)
{return (xredisKeysArray(V) || V.Type==XREDIS_REPLY_SET) && !(V.Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisKeysNull(xredisreplyvalue V)
{return V.Type==XREDIS_REPLY_NULL || (V.Type==XREDIS_REPLY_BULK_STRING && (V.Flags&XREDIS_REPLY_NULL_VALUE));}
static xredisreplyvalue xredisKeysGet(const xredisreply* R,size_t Index)
{xredisreplyvalue V={0};xredisReplyGet(R,Index,&V);return V;}
static bool xredisKeysFields(const xredisreply* R,xredisreplyvalue V,const cstr* Names,size_t Count,size_t* Fields)
{
    if((V.Type!=XREDIS_REPLY_MAP && !xredisKeysArray(V)) || V.ChildCount%2)return false;
    for(size_t I=0;I<Count;++I)Fields[I]=XREDIS_REPLY_NONE;
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue K=xredisKeysGet(R,I),D=xredisKeysGet(R,K.Next);if(!xredisKeysText(K))return false;
        for(size_t J=0;J<Count;++J)if(K.Data.Size==strlen(Names[J]) && !memcmp(K.Data.Data,Names[J],K.Data.Size)){if(Fields[J]!=XREDIS_REPLY_NONE)return false;Fields[J]=K.Next;break;}
        I=D.Next;}
    return true;
}
static bool xredisKeysInteger(const xredisreply* R,size_t Index,int64* N)
{if(Index==XREDIS_REPLY_NONE)return false;xredisreplyvalue V=xredisKeysGet(R,Index);if(V.Type!=XREDIS_REPLY_INTEGER)return false;*N=V.Integer;return true;}
static bool xredisKeysFlags(const xredisreply* R,xredisreplyvalue V,xrediskeyspec* S,xrediskeycommand* C)
{
    if(!xredisKeysSequence(V))return false;
    static const cstr Words[]={"RO","RW","OW","RM","access","update","insert","delete","not_key","variable_flags"};
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue W=xredisKeysGet(R,I);if(!xredisKeysText(W))return false;
        if(C){if(xredisKeysWord(W.Data,"movablekeys"))C->Movable=true;}
        else {size_t J=0;for(;J<sizeof(Words)/sizeof(*Words);++J)if(xredisKeysWord(W.Data,Words[J])){S->Flags|=1u<<J;break;}
            if(J==sizeof(Words)/sizeof(*Words))S->Unsupported=true;}
        I=W.Next;
    }
    return true;
}
static bool xredisKeysRule(const xredisreply* R,size_t Index,bool Begin,xrediskeyspec* S)
{
    static const cstr Names[]={"type","spec"};size_t F[2];if(!xredisKeysFields(R,xredisKeysGet(R,Index),Names,2,F) || F[0]==XREDIS_REPLY_NONE || F[1]==XREDIS_REPLY_NONE)return false;
    xredisreplyvalue Type=xredisKeysGet(R,F[0]),Spec=xredisKeysGet(R,F[1]);if(!xredisKeysText(Type))return false;
    if(Begin && xredisKeysWord(Type.Data,"index")){static const cstr N[]={"index"};size_t A[1];S->Begin=XRK_INDEX;
        return xredisKeysFields(R,Spec,N,1,A) && xredisKeysInteger(R,A[0],&S->Index) && S->Index>=0;}
    if(Begin && xredisKeysWord(Type.Data,"keyword")){static const cstr N[]={"keyword","startfrom"};size_t A[2];S->Begin=XRK_KEYWORD;
        if(!xredisKeysFields(R,Spec,N,2,A) || A[0]==XREDIS_REPLY_NONE || !xredisKeysInteger(R,A[1],&S->Index))return false;
        xredisreplyvalue K=xredisKeysGet(R,A[0]);if(!xredisKeysText(K) || !K.Data.Size)return false;S->Keyword=K.Data;return true;}
    if(!Begin && xredisKeysWord(Type.Data,"range")){static const cstr N[]={"lastkey","keystep","limit"};size_t A[3];S->Find=XRK_RANGE;
        return xredisKeysFields(R,Spec,N,3,A) && xredisKeysInteger(R,A[0],&S->Last) && xredisKeysInteger(R,A[1],&S->Step) && xredisKeysInteger(R,A[2],&S->Limit) && S->Step>0 && S->Limit>=0;}
    if(!Begin && xredisKeysWord(Type.Data,"keynum")){static const cstr N[]={"keynumidx","firstkey","keystep"};size_t A[3];S->Find=XRK_KEYNUM;
        return xredisKeysFields(R,Spec,N,3,A) && xredisKeysInteger(R,A[0],&S->Number) && xredisKeysInteger(R,A[1],&S->First) && xredisKeysInteger(R,A[2],&S->Step) && S->Number>=0 && S->First>=0 && S->Step>0;}
    S->Unsupported=true;return true; /* An unfamiliar rule's spec stays opaque. */
}
static xrediskeysstatus xredisKeysSpecs(xrediskeysbuild* B,xredisreplyvalue V,xrediskeycommand* C)
{
    if(!xredisKeysSequence(V))return XREDIS_KEYS_MALFORMED;
    if(V.ChildCount>B->Limits->MaxSpecifications-B->Specs)return XREDIS_KEYS_LIMIT;
    C->Spec=B->Specs;C->Count=V.ChildCount;static const cstr N[]={"flags","begin_search","find_keys","notes"};
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue X=xredisKeysGet(B->Reply,I);size_t F[4];xrediskeyspec S={0};
        if(!xredisKeysFields(B->Reply,X,N,4,F) || F[0]==XREDIS_REPLY_NONE || F[1]==XREDIS_REPLY_NONE || F[2]==XREDIS_REPLY_NONE ||
            !xredisKeysFlags(B->Reply,xredisKeysGet(B->Reply,F[0]),&S,NULL) || !xredisKeysRule(B->Reply,F[1],true,&S) || !xredisKeysRule(B->Reply,F[2],false,&S) ||
            (F[3]!=XREDIS_REPLY_NONE && !xredisKeysText(xredisKeysGet(B->Reply,F[3]))))return XREDIS_KEYS_MALFORMED;
        C->Unsupported|=S.Unsupported;if(B->Out)B->Out->Specs[B->Specs]=S;++B->Specs;I=X.Next;
    }
    return XREDIS_KEYS_OK;
}
static xrediskeysstatus xredisKeysCommands(xrediskeysbuild* B,xredisreplyvalue V,size_t Level,xbytesview Parent)
{
    if(!xredisKeysSequence(V))return XREDIS_KEYS_MALFORMED;
    if(Level>B->Limits->MaxDepth && V.ChildCount)return XREDIS_KEYS_LIMIT;
    for(size_t I=V.FirstChild;I!=XREDIS_REPLY_NONE;){xredisreplyvalue X=xredisKeysGet(B->Reply,I);I=X.Next;if(xredisKeysNull(X))continue;
        if(!xredisKeysArray(X) || X.ChildCount<6)return XREDIS_KEYS_MALFORMED;
        if(B->Commands>=B->Limits->MaxCommands)return XREDIS_KEYS_LIMIT;
        size_t F[10],J=0;for(size_t K=X.FirstChild;K!=XREDIS_REPLY_NONE && J<10;){F[J++]=K;K=xredisKeysGet(B->Reply,K).Next;}
        xredisreplyvalue Name=xredisKeysGet(B->Reply,F[0]);if(!xredisKeysText(Name) || !Name.Data.Size)return XREDIS_KEYS_MALFORMED;
        if(Name.Data.Size>B->Limits->MaxNameBytes)return XREDIS_KEYS_LIMIT;
        size_t Depth=1;bool Part=false;for(size_t K=0;K<Name.Data.Size;++K){unsigned Ch=Name.Data.Data[K];if(Ch=='|'){if(!Part)return XREDIS_KEYS_MALFORMED;Part=false;++Depth;}else {if(Ch<=32 || Ch==127)return XREDIS_KEYS_MALFORMED;Part=true;}}
        if(!Part)return XREDIS_KEYS_MALFORMED;
        if(Depth>B->Limits->MaxDepth)return XREDIS_KEYS_LIMIT;
        if(Depth>B->Depth)B->Depth=Depth;
        if(Parent.Size){if(Name.Data.Size<=Parent.Size+1 || Name.Data.Data[Parent.Size]!='|' || !xredisKeysEqual((xbytesview){Name.Data.Data,Parent.Size},Parent) || memchr(Name.Data.Data+Parent.Size+1,'|',Name.Data.Size-Parent.Size-1))return XREDIS_KEYS_MALFORMED;}
        xrediskeycommand C={0};C.Name=Name.Data;
        if(!xredisKeysInteger(B->Reply,F[1],&C.Arity) || !C.Arity || C.Arity==INT64_MIN || !xredisKeysFlags(B->Reply,xredisKeysGet(B->Reply,F[2]),NULL,&C) ||
            !xredisKeysInteger(B->Reply,F[3],&C.First) || !xredisKeysInteger(B->Reply,F[4],&C.Last) || !xredisKeysInteger(B->Reply,F[5],&C.Step) ||
            C.First<0 || (C.First && C.Step<=0) || (!C.First && (C.Last || C.Step)))return XREDIS_KEYS_MALFORMED;
        size_t At=B->Commands++;if(J>8){C.Modern=true;xrediskeysstatus S=xredisKeysSpecs(B,xredisKeysGet(B->Reply,F[8]),&C);if(S!=XREDIS_KEYS_OK)return S;}
        if(J>9){xredisreplyvalue Children=xredisKeysGet(B->Reply,F[9]);if(!xredisKeysSequence(Children))return XREDIS_KEYS_MALFORMED;C.Children=Children.ChildCount!=0;
            xrediskeysstatus S=xredisKeysCommands(B,Children,Level+1,C.Name);if(S!=XREDIS_KEYS_OK)return S;}
        if(B->Out)B->Out->Commands[At]=C;
    }
    return XREDIS_KEYS_OK;
}
static uint64 xredisKeysHash(uint64 H,xbytesview V)
{for(size_t I=0;I<V.Size;++I){H^=xredisKeysLower(V.Data[I]);H*=1099511628211ull;}return H;}
static bool xredisKeysCharge(size_t* Used,size_t N,size_t Limit)
{if(N>Limit-*Used)return false;*Used+=N;return true;}
static bool xredisKeysOutput(const xrediscommandcatalog* C,const void* P,size_t N)
{return C && __xredisClusterReplyOutput(C->Reply,P,N) && !xrtMemRangesOverlap(C,C->Info.StorageBytes,P,N);}
XREDIS_API bool xredisCommandKeyLimitsInit(xrediscommandkeylimits* K)
{
    if(!xrtMemRangeValid(K,sizeof(*K)))return xredisKeysError("invalid command key limits output");
    *K=(xrediscommandkeylimits){sizeof(*K),8192,32768,256,16,1048576,16u*1024u*1024u,64u*1024u*1024u,65536,64u*1024u*1024u,65536,64u*1024u*1024u,1048576};return true;
}
XREDIS_API xrediskeysstatus xredisCommandCatalogCreate(xredisreply* R,const xrediscommandkeylimits* Config,xrediscommandcatalog** Out)
{
    if(!__xredisClusterReplyOutput(R,Out,sizeof(*Out)) || (Config && (!__xredisClusterReplyOutput(R,Config,sizeof(*Config)) || Config->Size!=sizeof(*Config) || xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out)))))return xredisKeysError("invalid command catalogue arguments"),XREDIS_KEYS_ERROR;
    xrediscommandkeylimits K;if(Config)K=*Config;else xredisCommandKeyLimitsInit(&K);
    if(K.MaxCommands>65535 || K.MaxSpecifications>262144 || K.MaxNameBytes>4096 || K.MaxDepth>16 || K.MaxArguments>1048576 || K.MaxKeys>1048576 || K.MaxOwnedBytes>1073741824 || K.MaxPayloadBytes>1073741824 || K.MaxArgumentBytes>1073741824 || K.MaxKeyBytes>1073741824 || K.MaxSearchWork>1073741824)return xredisKeysError("invalid command key bounds"),XREDIS_KEYS_ERROR;
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);if(I.Nodes>K.MaxReplyNodes || I.PayloadBytes>K.MaxPayloadBytes)return XREDIS_KEYS_LIMIT;
    xredisreplyvalue Root=xredisKeysGet(R,I.Root);if(Root.Type==XREDIS_REPLY_SIMPLE_ERROR || Root.Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_KEYS_SERVER_ERROR;
    xrediskeysbuild B={0};B.Reply=R;B.Limits=&K;xrediskeysstatus S=xredisKeysCommands(&B,Root,1,(xbytesview){0});if(S!=XREDIS_KEYS_OK)return S;
    size_t Buckets=2;while(Buckets<2*B.Commands)Buckets*=2;
    size_t Bytes=sizeof(xrediscommandcatalog)+B.Commands*sizeof(xrediskeycommand)+B.Specs*sizeof(xrediskeyspec)+Buckets*sizeof(uint32);
    if(Bytes>K.MaxOwnedBytes || I.ReservedBytes>K.MaxOwnedBytes-Bytes)return XREDIS_KEYS_LIMIT;
    xrediscommandcatalog* C=xrtCalloc(1,Bytes);if(!C)return XREDIS_KEYS_MEMORY;
    C->Commands=(xrediskeycommand*)(C+1);C->Specs=(xrediskeyspec*)(C->Commands+B.Commands);C->Hash=(uint32*)(C->Specs+B.Specs);C->Buckets=Buckets;C->Limits=K;
    C->Info=(xrediscommandcataloginfo){sizeof(C->Info),B.Commands,B.Specs,B.Depth,Bytes,I.ReservedBytes};
    xrediskeysbuild Fill={0};Fill.Reply=R;Fill.Limits=&K;Fill.Out=C;S=xredisKeysCommands(&Fill,Root,1,(xbytesview){0});
    size_t Work=0;
    if(S==XREDIS_KEYS_OK)for(size_t N=0;N<B.Commands;++N){xbytesview Name=C->Commands[N].Name;if(!xredisKeysCharge(&Work,Name.Size+1,K.MaxSearchWork)){S=XREDIS_KEYS_LIMIT;break;}size_t H=(size_t)xredisKeysHash(14695981039346656037ull,Name)&(Buckets-1);
        while(C->Hash[H]){if(!xredisKeysCharge(&Work,Name.Size+1,K.MaxSearchWork)){S=XREDIS_KEYS_LIMIT;break;}if(xredisKeysEqual(Name,C->Commands[C->Hash[H]-1].Name)){S=XREDIS_KEYS_MALFORMED;break;}H=(H+1)&(Buckets-1);}if(S!=XREDIS_KEYS_OK)break;C->Hash[H]=(uint32)N+1;}
    if(S!=XREDIS_KEYS_OK || !xredisReplyRetain(R)){xrtFree(C);return S!=XREDIS_KEYS_OK?S:XREDIS_KEYS_ERROR;}
    C->Reply=R;xrtAtomic32Init(&C->References,1);*Out=C;return XREDIS_KEYS_OK;
}
XREDIS_API bool xredisCommandCatalogRetain(xrediscommandcatalog* C)
{
    if(!C)return xredisKeysError("command catalogue is NULL");
    uint32 N=xrtAtomic32Load(&C->References,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX){xrtSetErrorInfo(XERR_RANGE,"xrt.redis",XREDIS_REPLY_LIMIT,"command catalogue reference overflow");return false;}if(xrtAtomic32CompareExchange(&C->References,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XREDIS_API void xredisCommandCatalogRelease(xrediscommandcatalog* C)
{if(C && xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)==1){xredisReplyRelease(C->Reply);xrtFree(C);}}
XREDIS_API bool xredisCommandCatalogInfo(const xrediscommandcatalog* C,xrediscommandcataloginfo* I)
{if(!xredisKeysOutput(C,I,sizeof(*I)) || I->Size!=sizeof(*I))return xredisKeysError("invalid command catalogue info output");*I=C->Info;return true;}
typedef struct xrediskeyseval {
    const xrediscommandcatalog* Catalog;const xbytesview* Arguments;size_t Count;
    xrediscommandkey* Output;size_t Keys,Bytes,Work;uint16 Slot;
    bool HasSlot,Cross,Variable,Fill;
} xrediskeyseval;
static bool xredisKeysInvocation(xbytesview Name,const xbytesview* A,size_t Count)
{
    size_t At=0;for(size_t I=0;I<Count;++I){size_t End=At;while(End<Name.Size && Name.Data[End]!='|')++End;
        if(!xredisKeysEqual((xbytesview){Name.Data+At,End-At},A[I]))return false;
        if(I+1==Count)return End==Name.Size;
        if(End==Name.Size)return false;
        At=End+1;}
    return false;
}
static xrediskeysstatus xredisKeysLookup(xrediskeyseval* E,size_t* Command)
{
    const xrediscommandcatalog* C=E->Catalog;uint64 Hash=14695981039346656037ull;size_t Bytes=0,Best=SIZE_MAX;
    for(size_t I=0;I<E->Count && I<C->Info.LookupDepth;++I){xbytesview A=E->Arguments[I];
        if((I && Bytes==C->Limits.MaxNameBytes) || A.Size>C->Limits.MaxNameBytes-Bytes-(I!=0))break;
        if(!xredisKeysCharge(&E->Work,A.Size+1,C->Limits.MaxSearchWork))return XREDIS_KEYS_LIMIT;
        if(I){Hash^='|';Hash*=1099511628211ull;++Bytes;}Bytes+=A.Size;Hash=xredisKeysHash(Hash,A);
        size_t Bucket=(size_t)Hash&(C->Buckets-1);
        while(C->Hash[Bucket]){size_t At=C->Hash[Bucket]-1;
            if(!xredisKeysCharge(&E->Work,Bytes+2*C->Commands[At].Name.Size+1,C->Limits.MaxSearchWork))return XREDIS_KEYS_LIMIT;
            if(xredisKeysInvocation(C->Commands[At].Name,E->Arguments,I+1)){Best=At;break;}Bucket=(Bucket+1)&(C->Buckets-1);}
        if(Best!=SIZE_MAX && !C->Commands[Best].Children)break;
    }
    if(Best==SIZE_MAX)return XREDIS_KEYS_UNKNOWN;
    *Command=Best;return XREDIS_KEYS_OK;
}
static bool xredisKeysRelative(int64 Index,size_t Count,size_t* Out)
{
    if(Index>=0){if((uint64)Index>=Count)return false;*Out=(size_t)Index;return true;}
    uint64 Back=(uint64)(-(Index+1))+1;if(Back>Count)return false;*Out=Count-(size_t)Back;return true;
}
static bool xredisKeysNumber(xbytesview V,uint64* Out)
{
    if(!V.Size || V.Size>20 || (V.Size>1 && V.Data[0]=='0'))return false;
    uint64 N=0;for(size_t I=0;I<V.Size;++I){unsigned Ch=V.Data[I];if(Ch<'0' || Ch>'9' || N>(UINT64_MAX-(Ch-'0'))/10)return false;N=N*10+Ch-'0';}*Out=N;return true;
}
static xrediskeysstatus xredisKeysAdd(xrediskeyseval* E,size_t At,uint32 Flags)
{
    xbytesview A=E->Arguments[At];const xrediscommandkeylimits* K=&E->Catalog->Limits;
    if(E->Keys>=K->MaxKeys || A.Size>K->MaxKeyBytes-E->Bytes)return XREDIS_KEYS_LIMIT;
    E->Bytes+=A.Size;
    if(E->Fill)E->Output[E->Keys]=(xrediscommandkey){At,Flags};
    else {uint16 Slot=xredisClusterSlotRaw(A);if(E->HasSlot && E->Slot!=Slot)E->Cross=true;else E->Slot=Slot;E->HasSlot=true;}
    ++E->Keys;E->Variable|=(Flags&XREDIS_KEY_VARIABLE)!=0;return XREDIS_KEYS_OK;
}
static xrediskeysstatus xredisKeysExtract(xrediskeyseval* E,const xrediskeyspec* S)
{
    size_t Begin=0;
    if(S->Begin==XRK_INDEX){if((uint64)S->Index>=E->Count)return XREDIS_KEYS_ARGUMENTS;Begin=(size_t)S->Index;}
    else {size_t At;if(!xredisKeysRelative(S->Index,E->Count,&At))return XREDIS_KEYS_OK;bool Found=false;
        for(;;){xbytesview A=E->Arguments[At];size_t Cost=1+(A.Size==S->Keyword.Size?A.Size:0);
            if(!xredisKeysCharge(&E->Work,Cost,E->Catalog->Limits.MaxSearchWork))return XREDIS_KEYS_LIMIT;
            if(xredisKeysEqual(A,S->Keyword)){Begin=At+1;Found=true;break;}
            if(S->Index<0){if(!At)break;--At;}else if(++At==E->Count)break;
        }
        if(!Found)return XREDIS_KEYS_OK;
        if(Begin>=E->Count)return XREDIS_KEYS_ARGUMENTS;
    }
    size_t First=Begin,Last=0;uint64 Step=(uint64)S->Step;
    if(S->Find==XRK_RANGE){
        if(S->Last==-1 && S->Limit>1){size_t Keys=(size_t)((E->Count-Begin)/(uint64)S->Limit);if(!Keys)return XREDIS_KEYS_OK;Last=Begin+Keys-1;}
        else if(S->Last<0){if(!xredisKeysRelative(S->Last,E->Count,&Last))return XREDIS_KEYS_ARGUMENTS;}
        else {if((uint64)S->Last>=E->Count-Begin)return XREDIS_KEYS_ARGUMENTS;Last=Begin+(size_t)S->Last;}
        if(Last<First)return XREDIS_KEYS_OK;
    }else {
        if((uint64)S->Number>=E->Count-Begin)return XREDIS_KEYS_ARGUMENTS;
        uint64 N=0;
        if(!xredisKeysNumber(E->Arguments[Begin+(size_t)S->Number],&N) || N>E->Count)return XREDIS_KEYS_ARGUMENTS;
        if(!N)return XREDIS_KEYS_OK;
        if((uint64)S->First>=E->Count-Begin)return XREDIS_KEYS_ARGUMENTS;
        First=Begin+(size_t)S->First;
        if(N-1>(E->Count-1-First)/Step)return XREDIS_KEYS_ARGUMENTS;
        Last=First+(size_t)((N-1)*Step);
    }
    for(size_t At=First;;){xrediskeysstatus Status=xredisKeysAdd(E,At,S->Flags);if(Status!=XREDIS_KEYS_OK)return Status;if(Step>Last-At)break;At+=(size_t)Step;}
    return XREDIS_KEYS_OK;
}
static xrediskeysstatus xredisKeysEvaluatePass(xrediskeyseval* E,const xrediskeycommand* Command)
{
    if(Command->Modern){for(size_t I=0;I<Command->Count;++I){xrediskeysstatus S=xredisKeysExtract(E,&E->Catalog->Specs[Command->Spec+I]);if(S!=XREDIS_KEYS_OK)return S;}}
    else if(Command->First){xrediskeyspec S={0};S.Begin=XRK_INDEX;S.Find=XRK_RANGE;S.Index=Command->First;S.Step=Command->Step;S.Last=Command->Last;
        /* Legacy positive lastkey is absolute, unlike a modern relative range. */
        if(S.Last>=0){if(S.Last<S.Index)return XREDIS_KEYS_ARGUMENTS;S.Last-=S.Index;}
        return xredisKeysExtract(E,&S);}
    return XREDIS_KEYS_OK;
}
XREDIS_API xrediskeysstatus xredisCommandCatalogEvaluate(const xrediscommandcatalog* C,const xbytesview* A,size_t Count,xrediscommandkey* Keys,size_t Capacity,xrediscommandkeyresult* Out)
{
    if(!xredisKeysOutput(C,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xredisKeysError("invalid key extraction result output"),XREDIS_KEYS_ERROR;
    if(Count>C->Limits.MaxArguments || Capacity>C->Limits.MaxKeys)return XREDIS_KEYS_LIMIT;
    size_t KeyBytes=Capacity*sizeof(*Keys);
    if((!Keys && Capacity) || (KeyBytes && (!xredisKeysOutput(C,Keys,KeyBytes) || xrtMemRangesOverlap(Keys,KeyBytes,Out,sizeof(*Out)))) || !xrtMemRangeValid(A,Count*sizeof(*A)) ||
        xrtMemRangesOverlap(A,Count*sizeof(*A),Out,sizeof(*Out)) || (KeyBytes && xrtMemRangesOverlap(A,Count*sizeof(*A),Keys,KeyBytes)))return xredisKeysError("invalid key extraction spans"),XREDIS_KEYS_ERROR;
    size_t Bytes=0;for(size_t I=0;I<Count;++I){if(!xrtMemRangeValid(A[I].Data,A[I].Size) || xrtMemRangesOverlap(A[I].Data,A[I].Size,Out,sizeof(*Out)) || (KeyBytes && xrtMemRangesOverlap(A[I].Data,A[I].Size,Keys,KeyBytes)))return xredisKeysError("invalid key extraction argument/alias"),XREDIS_KEYS_ERROR;
        if(!xredisKeysCharge(&Bytes,A[I].Size,C->Limits.MaxArgumentBytes))return XREDIS_KEYS_LIMIT;}
    if(!Count)return XREDIS_KEYS_ARGUMENTS;
    xrediskeyseval E={0};E.Catalog=C;E.Arguments=A;E.Count=Count;size_t At=0;xrediskeysstatus Status=xredisKeysLookup(&E,&At);if(Status!=XREDIS_KEYS_OK)return Status;
    const xrediskeycommand* Command=&C->Commands[At];
    if((Command->Arity>0 && (uint64)Command->Arity!=Count) || (Command->Arity<0 && (uint64)(-Command->Arity)>Count))return XREDIS_KEYS_ARGUMENTS;
    if(Command->Children)return XREDIS_KEYS_UNKNOWN;
    if(Command->Unsupported || ((!Command->Modern || !Command->Count) && Command->Movable) || (Command->Modern && !Command->Count && Command->First))return XREDIS_KEYS_UNSUPPORTED;
    Status=xredisKeysEvaluatePass(&E,Command);if(Status!=XREDIS_KEYS_OK)return Status;
    if(E.Cross)return XREDIS_KEYS_CROSS_SLOT;
    if(Keys && E.Keys>Capacity)return XREDIS_KEYS_LIMIT;
    xrediscommandkeyresult Result={sizeof(Result),At,E.Keys,E.Slot,E.HasSlot,!Command->Modern,E.Variable};
    if(Keys && E.Keys){xrediskeyseval Fill={0};Fill.Catalog=C;Fill.Arguments=A;Fill.Count=Count;Fill.Output=Keys;Fill.Fill=true;
        Status=xredisKeysEvaluatePass(&Fill,Command);if(Status!=XREDIS_KEYS_OK)abort();}
    *Out=Result;return E.Keys?XREDIS_KEYS_OK:XREDIS_KEYS_NONE;
}
#endif
