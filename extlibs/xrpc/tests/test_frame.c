#include <xrpc/frame.h>
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void frRequire(bool OK,cstr Text)
{if(!OK){fprintf(stderr,"[FAIL] frame: %s\n",Text);exit(1);}}
static xrpcframeconfig frConfig(unsigned Format)
{xrpcframeconfig C;frRequire(xrpcFrameConfigInit(&C),"config");C.Format=(xrpcframeformat)Format;return C;}
static xrpcframereader* frCreate(xrpcframeconfig* C)
{xrpcframereader* R=NULL;frRequire(xrpcFrameReaderCreate(C,&R),"reader");return R;}
static xrpcframeinfo frInfo(xrpcframereader* R)
{xrpcframeinfo I={0};I.Size=sizeof(I);frRequire(xrpcFrameReaderInfo(R,&I),"info");return I;}
static size_t frWire(unsigned Format,bytes Out,cbytes Body,size_t N)
{
    size_t H;if(Format==XRPC_FRAME_UINT32_BE){H=4;for(unsigned I=0;I<4;++I)Out[I]=(unsigned char)(N>>((3-I)*8));}
    else {int S=snprintf((char*)Out,64,"Content-Length: %zu\r\n\r\n",N);frRequire(S>0,"literal header");H=(size_t)S;}
    if(N)memcpy(Out+H,Body,N);
    return H+N;
}
static void frRoundtrip(void)
{
    unsigned char Body[1024],Wire[1200];for(size_t I=0;I<sizeof(Body);++I)Body[I]=(unsigned char)(I*73);size_t Sizes[]={0,1,6,13,127,128,255,1024};
    for(unsigned F=1;F<=2;++F){xrpcframeconfig C=frConfig(F);xrpcframereader* R=frCreate(&C);
        for(size_t S=0;S<sizeof(Sizes)/sizeof(*Sizes);++S){size_t N=Sizes[S],Total=frWire(F,Wire,Body,N),Head=Total-N;
            unsigned char Encoded[1200];size_t Written=99,H=99;frRequire(xrpcFrameHeaderSize(N,&C,&H) && H==Head && xrpcFrameWrite((xbytesview){Body,N},&C,Encoded,sizeof(Encoded),&Written) && Written==Total && !memcmp(Encoded,Wire,Total),"independent canonical bytes");
            for(size_t Split=0;Split<=Total;++Split){frRequire(xrpcFrameReaderReset(R),"reset");xrpcframeview V,Frozen;memset(&V,0x5a,sizeof(V));Frozen=V;size_t Used=99;xrpcframestatus A=xrpcFrameReaderFeed(R,(xbytesview){Wire,Split},&Used,&V);frRequire(Used==Split && A==(Split==Total?XRPC_FRAME_READY:XRPC_FRAME_MORE),"prefix status/consume");
                if(A!=XRPC_FRAME_READY){frRequire(!memcmp(&V,&Frozen,sizeof(V)),"partial frame published");A=xrpcFrameReaderFeed(R,(xbytesview){Wire+Split,Total-Split},&Used,&V);frRequire(A==XRPC_FRAME_READY && Used==Total-Split,"remaining frame");}
                frRequire(V.Body.Size==N && V.HeaderBytes==Head && V.WireBytes==Total && (!N || !memcmp(V.Body.Data,Body,N)),"binary frame");frRequire(V.Buffered==(Split>Head && Split<Total),"borrow/copy decision");
                if(N && !V.Buffered)frRequire(V.Body.Data==Wire+Head,"complete body not borrowed directly");
                frRequire(xrpcFrameReaderEnd(R)==XRPC_FRAME_END && xrpcFrameReaderFeed(R,(xbytesview){Wire,Total},&Used,&V)==XRPC_FRAME_END && !Used,"frame boundary EOF");}
        }xrpcFrameReaderDestroy(R);
    }
}
static void frCoalesced(void)
{
    for(unsigned F=1;F<=2;++F){xrpcframeconfig C=frConfig(F);xrpcframereader* R=frCreate(&C);unsigned char W[256];size_t A=frWire(F,W,(cbytes)"one",3),B=frWire(F,W+A,(cbytes)"two\0binary",10),D=frWire(F,W+A+B,NULL,0),At=0;size_t Lengths[]={3,10,0};
        for(unsigned I=0;I<3;++I){size_t Used=99;xrpcframeview V;frRequire(xrpcFrameReaderFeed(R,(xbytesview){W+At,A+B+D-At},&Used,&V)==XRPC_FRAME_READY && V.Body.Size==Lengths[I] && Used==V.WireBytes,"coalesced boundary");At+=Used;}frRequire(At==A+B+D && xrpcFrameReaderEnd(R)==XRPC_FRAME_END,"coalesced end");xrpcFrameReaderDestroy(R);
    }
}
static void frHeaders(void)
{
    struct {cstr Wire;xrpcframestatus Status;} Cases[]={
        {"cOnTeNt-LeNgTh:\t0002 \t\r\nX-Trace: ok\r\n\r\n{}",XRPC_FRAME_READY},
        {"Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n{}",XRPC_FRAME_READY},
        {"Content-Type: APPLICATION/JSON; name=abc; CHARSET=\"u\\tf-8\"; extra=\"a;b\\\"c\"\r\nContent-Length: 2\r\n\r\n{}",XRPC_FRAME_READY},
        {"Content-Length: 2\r\nContent-Type: application/json;charset=utf8\r\n\r\n{}",XRPC_FRAME_READY},
        {"\r\n",XRPC_FRAME_PROTOCOL},{"X: y\r\n\r\n",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\n\n{}",XRPC_FRAME_PROTOCOL},{"Content-Length: 2\rx",XRPC_FRAME_PROTOCOL},
        {"Content-Length: +2\r\n\r\n{}",XRPC_FRAME_PROTOCOL},{"Content-Length: -2\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2 0\r\n\r\n{}",XRPC_FRAME_PROTOCOL},{"Content-Length: \r\n\r\n",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length : 2\r\n\r\n{}",XRPC_FRAME_PROTOCOL},{" Content-Length: 2\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\r\n folded: true\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 184467440737095516160\r\n\r\n",XRPC_FRAME_LIMIT},
        {"Content-Length: 1073741825\r\n\r\n",XRPC_FRAME_LIMIT},
        {"Content-Length: 2\r\nTransfer-Encoding: identity\r\n\r\n{}",XRPC_FRAME_UNSUPPORTED},
        {"Content-Length: 2\r\nContent-Encoding: gzip\r\n\r\n{}",XRPC_FRAME_UNSUPPORTED},
        {"Content-Length: 2\r\nContent-Type: text/plain\r\n\r\n{}",XRPC_FRAME_UNSUPPORTED},
        {"Content-Length: 2\r\nContent-Type: application/json;charset=utf-16\r\n\r\n{}",XRPC_FRAME_UNSUPPORTED},
        {"Content-Length: 2\r\nContent-Type: application/json;charset=utf8;charset=utf8\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\r\nContent-Type: application/json; x=\"open\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\r\nContent-Type: application/json;\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\r\nContent-Type: application/json\r\nContent-Type: application/json\r\n\r\n{}",XRPC_FRAME_PROTOCOL},
        {"Content-Length: 2\r\nX: \x80\r\n\r\n{}",XRPC_FRAME_PROTOCOL},{"Content-Length: 2\r\nX: \x01\r\n\r\n{}",XRPC_FRAME_PROTOCOL}
    };
    xrpcframereader* R=frCreate(NULL);for(size_t C=0;C<sizeof(Cases)/sizeof(*Cases);++C){size_t N=strlen(Cases[C].Wire);
        for(unsigned Pass=0;Pass<3;++Pass){size_t Chunk=Pass==0?1:Pass==1?7:N;xrpcFrameReaderReset(R);xrpcframeview V,Frozen;memset(&V,0x6b,sizeof(V));Frozen=V;size_t At=0;xrpcframestatus S=XRPC_FRAME_MORE;
            while(At<N && S==XRPC_FRAME_MORE){size_t Take=N-At<Chunk?N-At:Chunk,Used=99;S=xrpcFrameReaderFeed(R,(xbytesview){(cbytes)Cases[C].Wire+At,Take},&Used,&V);frRequire(Used<=Take,"header consumed bounds");At+=Used;}
            if(S!=Cases[C].Status){fprintf(stderr,"header case=%zu chunk=%zu got=%d expected=%d\n",C,Chunk,(int)S,(int)Cases[C].Status);frRequire(false,"header classification");}
            if(S==XRPC_FRAME_READY)frRequire(V.Body.Size==2 && !memcmp(V.Body.Data,"{}",2),"header body");else {frRequire(!memcmp(&V,&Frozen,sizeof(V)),"bad header published view");size_t Used=99;frRequire(xrpcFrameReaderFeed(R,(xbytesview){(cbytes)"next",4},&Used,&V)==S && !Used && xrpcFrameReaderEnd(R)==S,"sticky failure");}
        }
    }xrpcFrameReaderDestroy(R);
}
static void frEOF(void)
{
    unsigned char W[128];for(unsigned F=1;F<=2;++F){size_t N=frWire(F,W,(cbytes)"abcdef",6);xrpcframeconfig C=frConfig(F);xrpcframereader* R=frCreate(&C);
        for(size_t Prefix=0;Prefix<N;++Prefix){xrpcFrameReaderReset(R);size_t Used=99;xrpcframeview V;frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,Prefix},&Used,&V)==XRPC_FRAME_MORE && Used==Prefix,"EOF prefix");frRequire(xrpcFrameReaderEnd(R)==(Prefix?XRPC_FRAME_TRUNCATED:XRPC_FRAME_END),"EOF classification");frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},&Used,&V)==(Prefix?XRPC_FRAME_TRUNCATED:XRPC_FRAME_END) && !Used && frInfo(R).Ended,"EOF terminal visibility");}
        xrpcFrameReaderDestroy(R);
    }
}
static void frLargeDeclarations(void)
{
    size_t Values[]={0,1,9,10,99,100,999,1000,65535,65536,1024u*1024u*1024u};unsigned char Header[128],Expected[128];
    for(unsigned Format=1;Format<=2;++Format){xrpcframeconfig C=frConfig(Format);C.MaxBodyBytes=1024u*1024u*1024u;xrpcframereader* R=frCreate(&C);
        for(size_t I=0;I<sizeof(Values)/sizeof(*Values);++I){size_t N=Values[I],Bytes=99,Used=99;frRequire(xrpcFrameHeaderWrite(N,&C,Header,sizeof(Header),&Bytes),"large length header");size_t ExpectedBytes=0;
            if(Format==1){int S=snprintf((char*)Expected,sizeof(Expected),"Content-Length: %zu\r\n\r\n",N);frRequire(S>0,"large decimal oracle");ExpectedBytes=(size_t)S;}else {ExpectedBytes=4;for(unsigned J=0;J<4;++J)Expected[J]=(unsigned char)(N>>((3-J)*8));}
            frRequire(Bytes==ExpectedBytes && !memcmp(Header,Expected,Bytes),"length digit transition");xrpcFrameReaderReset(R);xrpcframeview V;frRequire(xrpcFrameReaderFeed(R,(xbytesview){Header,Bytes},&Used,&V)==(N?XRPC_FRAME_MORE:XRPC_FRAME_READY) && Used==Bytes && !frInfo(R).BodyCapacity,"declared length allocated absent body");}
        xrpcFrameReaderDestroy(R);
    }
}
static void frLimits(void)
{
    const unsigned char W[]="Content-Length: 2\r\n\r\n{}";const size_t N=sizeof(W)-1,H=N-2;xrpcframeconfig C=frConfig(1);xrpcframereader* R=frCreate(&C);size_t Base=frInfo(R).BaseBytes;xrpcFrameReaderDestroy(R);
    for(unsigned Kind=0;Kind<5;++Kind){C=frConfig(1);if(Kind==0)C.MaxBodyBytes=1;else if(Kind==1)C.MaxHeaderBytes=H-1,C.MaxLineBytes=H-1;else if(Kind==2)C.MaxLineBytes=H-3;else if(Kind==3)C.MaxStorageBytes=Base;else C.MaxBodyBytes=0;
        R=frCreate(&C);size_t Used=99;xrpcframeview V;if(Kind==3){frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},&Used,&V)==XRPC_FRAME_READY && frInfo(R).StorageBytes==Base,"whole-frame borrowed storage");xrpcFrameReaderReset(R);frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,1},&Used,&V)==XRPC_FRAME_LIMIT,"fragment head storage cap");}
        else frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},&Used,&V)==XRPC_FRAME_LIMIT,"wire cap");
        xrpcFrameReaderDestroy(R);}
    C=frConfig(1);C.MaxHeaderBytes=H;C.MaxLineBytes=H-2;C.MaxBodyBytes=2;C.MaxFields=1;R=frCreate(&C);size_t Used=99;xrpcframeview V;frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},&Used,&V)==XRPC_FRAME_READY,"exact wire caps");xrpcFrameReaderReset(R);const unsigned char Extra[]="Content-Length: 2\r\nX: a\r\n\r\n{}";frRequire(xrpcFrameReaderFeed(R,(xbytesview){Extra,sizeof(Extra)-1},&Used,&V)==XRPC_FRAME_LIMIT,"field cap");xrpcFrameReaderDestroy(R);
    C=frConfig(2);C.MaxBodyBytes=6;C.MaxStorageBytes=Base+8;R=frCreate(&C);unsigned char Binary[16];size_t B=frWire(2,Binary,(cbytes)"abcdef",6);
    frRequire(xrpcFrameReaderReserve(R,0,4)==XRPC_FRAME_READY,"exact initial reserve");frRequire(xrpcFrameReaderFeed(R,(xbytesview){Binary,7},&Used,&V)==XRPC_FRAME_MORE,"retained partial body");
    frRequire(xrpcFrameReaderReserve(R,0,5)==XRPC_FRAME_LIMIT && frInfo(R).BodyBytes==3 && frInfo(R).BodyCapacity==4,"old+new peak quota");frRequire(xrpcFrameReaderFeed(R,(xbytesview){Binary+7,B-7},&Used,&V)==XRPC_FRAME_LIMIT,"grow peak quota");xrpcFrameReaderDestroy(R);
    C=frConfig(2);C.MaxBodyBytes=0;R=frCreate(&C);memset(Binary,0,4);frRequire(xrpcFrameReaderFeed(R,(xbytesview){Binary,4},&Used,&V)==XRPC_FRAME_READY && !V.Body.Size,"zero-body-only profile");xrpcFrameReaderDestroy(R);
}
static void frReserveAndFaults(void)
{
    xrpcframereader* R=frCreate(NULL);unsigned char W[256];size_t N=frWire(1,W,(cbytes)"abcdefghijklmnop",16),Used=99;xrpcframeview V;
    frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},&Used,&V)==XRPC_FRAME_READY && !V.Buffered,"borrow then reserve");frRequire(xrpcFrameReaderReserve(R,64,32)==XRPC_FRAME_READY,"reserve after borrowed frame");xrpcFrameReaderReset(R);
    frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N-8},&Used,&V)==XRPC_FRAME_MORE,"reserve partial body");frRequire(xrpcFrameReaderReserve(R,128,64)==XRPC_FRAME_READY,"active reserve");memset(W,'?',N-8);frRequire(xrpcFrameReaderFeed(R,(xbytesview){(cbytes)"ijklmnop",8},&Used,&V)==XRPC_FRAME_READY && V.Buffered && !memcmp(V.Body.Data,"abcdefghijklmnop",16),"reserve preserved copied prefix");xrpcFrameReaderDestroy(R);
    for(unsigned Kind=0;Kind<3;++Kind){R=frCreate(NULL);xrpcframeinfo Before=frInfo(R);xallocationbudget* B=NULL;xallocationscope Scope={0};frRequire(xrtAllocationBudgetCreate(Kind==2?64:1,&B) && xrtAllocationBudgetEnter(B,&Scope),"fault scope");
        if(Kind==2)frRequire(xrpcFrameReaderReserve(R,64,64)==XRPC_FRAME_MEMORY,"atomic reserve second OOM");
        else {N=frWire(1,W,(cbytes)"body",4);size_t Take=Kind?N-2:1;frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,Take},&Used,&V)==XRPC_FRAME_MEMORY,"head/body allocation OOM");}
        frRequire(xrtAllocationBudgetLeave(&Scope),"fault leave");xrtClearError();xrpcframeinfo After=frInfo(R);frRequire(After.HeaderCapacity==Before.HeaderCapacity && After.BodyCapacity==Before.BodyCapacity,"failed reserve changed buffers");
        frRequire(xrpcFrameReaderReserve(R,128,128)==XRPC_FRAME_READY,"reserve after unbuffered failure");frRequire(xrpcFrameReaderReset(R),"fault reset");xrpcFrameReaderDestroy(R);xallocationbudgetinfo I={0};I.Size=sizeof(I);frRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.LiveCount,"OOM leaked owners");xrtAllocationBudgetFree(B);}
    unsigned OK=0,Failed=0;for(size_t L=1;L<512;L+=7){xallocationbudget* B=NULL;xallocationscope Scope={0};frRequire(xrtAllocationBudgetCreate(L,&B) && xrtAllocationBudgetEnter(B,&Scope),"constructor quota");R=(void*)(uintptr_t)1;bool Made=xrpcFrameReaderCreate(NULL,&R);frRequire(xrtAllocationBudgetLeave(&Scope),"constructor leave");if(Made){++OK;xrpcFrameReaderDestroy(R);}else {++Failed;frRequire(R==(void*)(uintptr_t)1,"constructor output");xrtClearError();}xallocationbudgetinfo I={0};I.Size=sizeof(I);frRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes,"constructor leaked");xrtAllocationBudgetFree(B);}frRequire(OK && Failed,"constructor prefixes");
}
static void frAliasesAndWrites(void)
{
    xrpcframereader* R=frCreate(NULL);unsigned char W[128],Out[128],Saved[128];size_t N=frWire(1,W,(cbytes)"ab\0c",4),Used=99;xrpcframeview V,Frozen;memset(&V,0x5c,sizeof(V));Frozen=V;
    frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},(void*)&V,&V)==XRPC_FRAME_ERROR && !memcmp(&V,&Frozen,sizeof(V)),"used/view alias");xrtClearError();
    frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N},&Used,(void*)R)==XRPC_FRAME_ERROR && Used==99 && !frInfo(R).HeaderBytes,"reader alias state");xrtClearError();
    frRequire(xrpcFrameReaderFeed(R,(xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1),4},&Used,&V)==XRPC_FRAME_ERROR && Used==99,"wrapped input");xrtClearError();
    frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,N-1},&Used,&V)==XRPC_FRAME_MORE && xrpcFrameReaderFeed(R,(xbytesview){W+N-1,1},&Used,&V)==XRPC_FRAME_READY && V.Buffered,"buffer alias fixture");size_t Last=Used;
    frRequire(xrpcFrameReaderFeed(R,V.Body,&Used,&V)==XRPC_FRAME_ERROR && Used==Last,"input owned-buffer alias");xrtClearError();xrpcFrameReaderDestroy(R);
    for(unsigned F=1;F<=2;++F){xrpcframeconfig C=frConfig(F);size_t Head=0;frRequire(xrpcFrameHeaderSize(4,&C,&Head),"writer size");
        for(size_t Cap=0;Cap<Head+4;++Cap){memset(Out,0x77,sizeof(Out));memcpy(Saved,Out,sizeof(Out));size_t Written=99;frRequire(!xrpcFrameWrite((xbytesview){(cbytes)"ab\0c",4},&C,Out,Cap,&Written) && Written==99 && !memcmp(Out,Saved,sizeof(Out)),"atomic short write");xrtClearError();}
        size_t Written=99;memcpy(Out,"body",4);frRequire(!xrpcFrameWrite((xbytesview){Out,4},&C,Out,sizeof(Out),&Written) && Written==99 && !memcmp(Out,"body",4),"writer overlap");xrtClearError();frRequire(!xrpcFrameHeaderWrite(4,&C,Out,sizeof(Out),(void*)Out),"writer size alias");xrtClearError();
        frRequire(!xrpcFrameHeaderWrite(4,&C,Out,SIZE_MAX,&Written) && Written==99 && !memcmp(Out,"body",4),"writer wrapped capacity");xrtClearError();
        frRequire(xrpcFrameHeaderWrite(4,&C,Out,sizeof(Out),&Written) && Written==Head,"header-only writer");}
    xrpcframeconfig C=frConfig(1);xrpcframereader* Bad=(void*)(uintptr_t)1;C.Size=0;frRequire(!xrpcFrameReaderCreate(&C,&Bad) && Bad==(void*)(uintptr_t)1,"bad config");xrtClearError();
}
static void frWarm(void)
{
    unsigned char W[128],Out[128];for(unsigned F=1;F<=2;++F){xrpcframeconfig C=frConfig(F);xrpcframereader* R=frCreate(&C);frRequire(xrpcFrameReaderReserve(R,F==1?128:0,128)==XRPC_FRAME_READY,"warm reserve");size_t N=frWire(F,W,(cbytes)"binary\0payload",14);xrtSetErrorKind(XERR_STATE);const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope Scope={0};frRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"warm quota");xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
        for(unsigned I=0;I<10000;++I){xrpcFrameReaderReset(R);size_t Used=99;xrpcframeview V;frRequire(xrpcFrameReaderFeed(R,(xbytesview){W,1},&Used,&V)==XRPC_FRAME_MORE && Used==1,"warm header");frRequire(xrpcFrameReaderFeed(R,(xbytesview){W+1,N-2},&Used,&V)==XRPC_FRAME_MORE && Used==N-2,"warm body");frRequire(xrpcFrameReaderFeed(R,(xbytesview){W+N-1,1},&Used,&V)==XRPC_FRAME_READY && V.Buffered && V.Body.Size==14 && !memcmp(V.Body.Data,"binary\0payload",14),"warm finish");frRequire(xrpcFrameWrite(V.Body,&C,Out,sizeof(Out),&Used) && Used==N && xrtGetError()==Error,"warm writer/error");}
        xrtMemStatsGet(&After);xrtMemStatsEnable(false);frRequire(xrtAllocationBudgetLeave(&Scope) && After.BlockAllocCalls==Before.BlockAllocCalls,"warm heap allocation");xallocationbudgetinfo I={0};I.Size=sizeof(I);frRequire(xrtAllocationBudgetInfo(B,&I) && !I.PeakBytes && !I.Rejections,"warm quota attempts");xrtAllocationBudgetFree(B);xrtClearError();xrpcFrameReaderDestroy(R);}
}
static void frMutations(void)
{
    uint32 Seed=0x87654321;unsigned char W[128];xrpcframereader* R=frCreate(NULL);
    for(unsigned I=0;I<12000;++I){size_t N=frWire(1,W,(cbytes)"{}\0binary",9);Seed=Seed*1664525u+1013904223u;W[Seed%N]^=(unsigned char)(1u<<(Seed%8));xrpcFrameReaderReset(R);size_t At=0;xrpcframestatus S=XRPC_FRAME_MORE;
        while(At<N && S==XRPC_FRAME_MORE){size_t Used=99,Take=(Seed%7)+1;if(Take>N-At)Take=N-At;xrpcframeview V,Frozen;memset(&V,0x4c,sizeof(V));Frozen=V;S=xrpcFrameReaderFeed(R,(xbytesview){W+At,Take},&Used,&V);frRequire(Used<=Take,"mutation consumed bounds");At+=Used;if(S==XRPC_FRAME_READY)frRequire(V.Body.Size<=1024u*1024u && V.WireBytes==V.HeaderBytes+V.Body.Size,"mutation view bounds");else frRequire(!memcmp(&V,&Frozen,sizeof(V)),"mutation partial publication");}
        if(S==XRPC_FRAME_MORE)frRequire(xrpcFrameReaderEnd(R)==XRPC_FRAME_TRUNCATED,"mutation partial EOF");}
    xrpcFrameReaderDestroy(R);
}
int main(void)
{frRoundtrip();frCoalesced();frHeaders();frEOF();frLargeDeclarations();frLimits();frReserveAndFaults();frAliasesAndWrites();frWarm();frMutations();puts("[PASS] stream framing: independent bytes, all split points, coalescing, strict headers/media, caps/peak quotas, borrowed/copied views, sticky EOF/failures, aliases, OOM and warmed no-heap cycles");return 0;}
