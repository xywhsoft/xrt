#include <xrpc/frame.h>
#include <string.h>
#include <xrt/error.h>
#if defined(XRPC_FEATURE_XRPC_FRAME)
struct xrpcframereader {
    xrpcframeconfig Config;bytes Header,Body;size_t HeaderCapacity,BodyCapacity;
    size_t HeaderBytes,BodyBytes,HeaderStored,BodyStored,Length,Line,Fields,Peak;
    unsigned Ending;bool InBody,Ready,Ended;xrpcframestatus Failure;
    unsigned char Binary[4];
};
static bool xrpcFrameError(xerrkind K,cstr Text)
{xrtSetErrorInfo(K,"xrt.rpc.frame",11001,Text);return false;}
XRPC_API bool xrpcFrameConfigInit(xrpcframeconfig* C)
{if(!xrtMemRangeValid(C,sizeof(*C)))return xrpcFrameError(XERR_ARGUMENT,"invalid frame config output");*C=(xrpcframeconfig){sizeof(*C),XRPC_FRAME_CONTENT_LENGTH,1024u*1024u,8192,4096,64,4u*1024u*1024u};return true;}
static bool xrpcFrameConfig(const xrpcframeconfig* In,xrpcframeconfig* C)
{
    if(In){if(!xrtMemRangeValid(In,sizeof(*In)))return false;*C=*In;}else xrpcFrameConfigInit(C);
    return C->Size==sizeof(*C) && (C->Format==XRPC_FRAME_CONTENT_LENGTH || C->Format==XRPC_FRAME_UINT32_BE) &&
        C->MaxBodyBytes<=1024u*1024u*1024u && C->MaxHeaderBytes>=4 && C->MaxHeaderBytes<=1024u*1024u && C->MaxLineBytes>=2 && C->MaxLineBytes<=C->MaxHeaderBytes && C->MaxFields && C->MaxFields<=65536 &&
        C->MaxStorageBytes>=sizeof(xrpcframereader) && (uint64)C->MaxStorageBytes<=UINT64_C(4294967296);
}
static size_t xrpcFrameStorage(const xrpcframereader* R)
{return sizeof(*R)+R->HeaderCapacity+R->BodyCapacity;}
static bool xrpcFrameOutput(const xrpcframereader* R,const void* P,size_t N)
{return R && xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(R,sizeof(*R),P,N) && !xrtMemRangesOverlap(R->Header,R->HeaderCapacity,P,N) && !xrtMemRangesOverlap(R->Body,R->BodyCapacity,P,N);}
XRPC_API bool xrpcFrameReaderCreate(const xrpcframeconfig* Config,xrpcframereader** Out)
{
    xrpcframeconfig C;if(!xrpcFrameConfig(Config,&C) || !xrtMemRangeValid(Out,sizeof(*Out)) || (Config && xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))))return xrpcFrameError(XERR_ARGUMENT,"invalid frame reader config/output");
    xrpcframereader* R=xrtCalloc(1,sizeof(*R));if(!R)return false;R->Config=C;R->Peak=sizeof(*R);*Out=R;return true;
}
XRPC_API void xrpcFrameReaderDestroy(xrpcframereader* R)
{if(R){xrtFree(R->Header);xrtFree(R->Body);xrtFree(R);}}
static xrpcframestatus xrpcFrameGrow(xrpcframereader* R,bool Head,size_t Need,bool Exact)
{
    bytes* P=Head?&R->Header:&R->Body;size_t* Capacity=Head?&R->HeaderCapacity:&R->BodyCapacity;
    size_t Max=Head?R->Config.MaxHeaderBytes:R->Config.MaxBodyBytes;
    if(Need>Max)return XRPC_FRAME_LIMIT;
    if(Need<=*Capacity)return XRPC_FRAME_READY;
    size_t Live=xrpcFrameStorage(R),Available=R->Config.MaxStorageBytes-Live;
    if(Need>Available)return XRPC_FRAME_LIMIT;
    size_t Target=Need;if(!Exact){Target=*Capacity?*Capacity:128;while(Target<Need && Target<=Max/2)Target*=2;if(Target<Need)Target=Need;if(Target>Max)Target=Max;if(Target>Available)Target=Available;}
    size_t Peak=Live+Target;bytes B=xrtRealloc(*P,Target);if(!B)return XRPC_FRAME_MEMORY;*P=B;*Capacity=Target;if(Peak>R->Peak)R->Peak=Peak;return XRPC_FRAME_READY;
}
XRPC_API xrpcframestatus xrpcFrameReaderReserve(xrpcframereader* R,size_t Head,size_t Body)
{
    if(!R || (R->Config.Format==XRPC_FRAME_UINT32_BE && Head))return xrpcFrameError(XERR_ARGUMENT,"invalid frame reserve"),XRPC_FRAME_ERROR;
    /* Reserve both atomically: allocate separate candidates before replacement.
     * This path is cold and accounts old plus both candidate capacities. */
    if(Head>R->Config.MaxHeaderBytes || Body>R->Config.MaxBodyBytes)return XRPC_FRAME_LIMIT;
    size_t H=Head>R->HeaderCapacity?Head:0,B=Body>R->BodyCapacity?Body:0,Live=xrpcFrameStorage(R);
    if(H>R->Config.MaxStorageBytes-Live || B>R->Config.MaxStorageBytes-Live-H)return XRPC_FRAME_LIMIT;
    bytes NewH=H?xrtMalloc(H):NULL;
    if(H && !NewH)return XRPC_FRAME_MEMORY;
    if(H && NewH && Live+H>R->Peak)R->Peak=Live+H;
    bytes NewB=B?xrtMalloc(B):NULL;
    if((!H || NewH) && B && NewB && Live+H+B>R->Peak)R->Peak=Live+H+B;
    if((H && !NewH) || (B && !NewB)){xrtFree(NewH);xrtFree(NewB);return XRPC_FRAME_MEMORY;}
    if(H){if(R->HeaderStored)memcpy(NewH,R->Header,R->HeaderStored);xrtFree(R->Header);R->Header=NewH;R->HeaderCapacity=H;}
    if(B){if(R->BodyStored)memcpy(NewB,R->Body,R->BodyStored);xrtFree(R->Body);R->Body=NewB;R->BodyCapacity=B;}
    if(Live+H+B>R->Peak)R->Peak=Live+H+B;
    return XRPC_FRAME_READY;
}
static unsigned xrpcFrameLower(unsigned C){return C>='A' && C<='Z'?C+('a'-'A'):C;}
static bool xrpcFrameEqual(cbytes P,size_t N,cstr S)
{size_t I=0;for(;I<N && S[I];++I)if(xrpcFrameLower(P[I])!=(unsigned char)S[I])return false;return I==N && !S[I];}
static bool xrpcFrameToken(unsigned C)
{return (C>='a' && C<='z') || (C>='A' && C<='Z') || (C>='0' && C<='9') || (C && strchr("!#$%&'*+-.^_`|~",(int)C)!=NULL);}
static bool xrpcFrameOWS(unsigned C){return C==' ' || C=='\t';}
static xrpcframestatus xrpcFrameType(cbytes P,size_t N)
{
    size_t I=0;while(I<N && P[I]!=';' && !xrpcFrameOWS(P[I]))++I;
    if(!xrpcFrameEqual(P,I,"application/vscode-jsonrpc") && !xrpcFrameEqual(P,I,"application/json"))return XRPC_FRAME_UNSUPPORTED;
    bool Charset=false;while(I<N){while(I<N && xrpcFrameOWS(P[I]))++I;if(I==N)break;if(P[I++]!=';')return XRPC_FRAME_PROTOCOL;
        while(I<N && xrpcFrameOWS(P[I]))++I;
        size_t Name=I;while(I<N && xrpcFrameToken(P[I]))++I;size_t NameBytes=I-Name;if(!NameBytes)return XRPC_FRAME_PROTOCOL;
        while(I<N && xrpcFrameOWS(P[I]))++I;
        if(I==N || P[I++]!='=')return XRPC_FRAME_PROTOCOL;
        while(I<N && xrpcFrameOWS(P[I]))++I;
        bool IsCharset=xrpcFrameEqual(P+Name,NameBytes,"charset");unsigned char Value[5];size_t Count=0;
        if(I<N && P[I]=='"'){++I;bool Closed=false;while(I<N){unsigned C=P[I++];if(C=='"'){Closed=true;break;}if(C=='\\'){if(I==N)return XRPC_FRAME_PROTOCOL;C=P[I++];}if(Count<sizeof(Value))Value[Count]=(unsigned char)C;++Count;}if(!Closed)return XRPC_FRAME_PROTOCOL;}
        else {size_t Begin=I;while(I<N && xrpcFrameToken(P[I])){if(Count<sizeof(Value))Value[Count]=P[I];++I;++Count;}if(I==Begin)return XRPC_FRAME_PROTOCOL;}
        if(IsCharset){if(Charset)return XRPC_FRAME_PROTOCOL;Charset=true;if(Count>sizeof(Value) || (!xrpcFrameEqual(Value,Count,"utf-8") && !xrpcFrameEqual(Value,Count,"utf8")))return XRPC_FRAME_UNSUPPORTED;}
    }return XRPC_FRAME_READY;
}
static xrpcframestatus xrpcFrameHead(xrpcframereader* R,cbytes P,size_t N)
{
    bool Length=false,Type=false;size_t I=0,Fields=0;R->Length=0;
    while(I+1<N){size_t Begin=I;while(I+1<N && !(P[I]=='\r' && P[I+1]=='\n'))++I;size_t End=I;I+=2;if(End==Begin)return Length && I==N?XRPC_FRAME_READY:XRPC_FRAME_PROTOCOL;
        if(++Fields>R->Config.MaxFields)return XRPC_FRAME_LIMIT;
        size_t Colon=Begin;while(Colon<End && xrpcFrameToken(P[Colon]))++Colon;if(Colon==Begin || Colon==End || P[Colon]!=':')return XRPC_FRAME_PROTOCOL;
        size_t V=Colon+1;while(V<End && xrpcFrameOWS(P[V]))++V;while(End>V && xrpcFrameOWS(P[End-1]))--End;
        if(xrpcFrameEqual(P+Begin,Colon-Begin,"content-length")){
            if(Length || V==End)return XRPC_FRAME_PROTOCOL;
            Length=true;size_t L=0;
            for(size_t J=V;J<End;++J){if(P[J]<'0' || P[J]>'9')return XRPC_FRAME_PROTOCOL;unsigned D=P[J]-'0';if(L>(SIZE_MAX-D)/10)return XRPC_FRAME_LIMIT;L=L*10+D;}
            if(L>R->Config.MaxBodyBytes)return XRPC_FRAME_LIMIT;
            R->Length=L;
        }else if(xrpcFrameEqual(P+Begin,Colon-Begin,"content-type")){if(Type)return XRPC_FRAME_PROTOCOL;Type=true;xrpcframestatus S=xrpcFrameType(P+V,End-V);if(S!=XRPC_FRAME_READY)return S;}
        else if(xrpcFrameEqual(P+Begin,Colon-Begin,"transfer-encoding") || xrpcFrameEqual(P+Begin,Colon-Begin,"content-encoding"))return XRPC_FRAME_UNSUPPORTED;
    }return XRPC_FRAME_PROTOCOL;
}
static void xrpcFrameNext(xrpcframereader* R)
{R->HeaderBytes=R->BodyBytes=R->HeaderStored=R->BodyStored=R->Length=R->Line=R->Fields=0;R->Ending=0;R->InBody=R->Ready=R->Ended=false;R->Failure=XRPC_FRAME_MORE;}
static xrpcframestatus xrpcFrameScan(xrpcframereader* R,unsigned C)
{
    if(++R->HeaderBytes>R->Config.MaxHeaderBytes || ++R->Line>R->Config.MaxLineBytes)return XRPC_FRAME_LIMIT;
    if(R->Ending==1 || R->Ending==3){if(C!='\n' || !R->Fields)return XRPC_FRAME_PROTOCOL;R->Line=0;if(R->Ending==3){R->Ending=4;return XRPC_FRAME_READY;}R->Ending=2;return XRPC_FRAME_MORE;}
    if(C=='\r'){R->Ending=R->Ending==2?3:1;return XRPC_FRAME_MORE;}
    if(C=='\n' || (C<32 && C!='\t') || C>126)return XRPC_FRAME_PROTOCOL;
    if(R->Ending==2){if(++R->Fields>R->Config.MaxFields)return XRPC_FRAME_LIMIT;}else if(R->HeaderBytes==1)R->Fields=1;
    R->Ending=0;return XRPC_FRAME_MORE;
}
XRPC_API xrpcframestatus xrpcFrameReaderFeed(xrpcframereader* R,xbytesview In,size_t* Used,xrpcframeview* Out)
{
    if(!xrpcFrameOutput(R,Used,sizeof(*Used)) || !xrpcFrameOutput(R,Out,sizeof(*Out)) || !xrpcFrameOutput(R,In.Data,In.Size) || xrtMemRangesOverlap(Used,sizeof(*Used),Out,sizeof(*Out)) || xrtMemRangesOverlap(In.Data,In.Size,Used,sizeof(*Used)) || xrtMemRangesOverlap(In.Data,In.Size,Out,sizeof(*Out)))return xrpcFrameError(XERR_ARGUMENT,"invalid frame feed spans/aliases"),XRPC_FRAME_ERROR;
    size_t I=0;*Used=0;if(R->Failure!=XRPC_FRAME_MORE)return R->Failure;if(R->Ended)return XRPC_FRAME_END;if(R->Ready)xrpcFrameNext(R);
    xrpcframestatus S=XRPC_FRAME_MORE;
    if(!R->InBody){
        if(R->Config.Format==XRPC_FRAME_UINT32_BE){while(I<In.Size && R->HeaderBytes<4)R->Binary[R->HeaderBytes++]=In.Data[I++];if(R->HeaderBytes<4)goto More;R->Length=((size_t)R->Binary[0]<<24)|((size_t)R->Binary[1]<<16)|((size_t)R->Binary[2]<<8)|R->Binary[3];if(R->Length>R->Config.MaxBodyBytes){S=XRPC_FRAME_LIMIT;goto Failed;}R->InBody=true;}
        else {size_t Before=R->HeaderBytes,Start=I;while(I<In.Size){S=xrpcFrameScan(R,In.Data[I++]);if(S!=XRPC_FRAME_MORE)break;}
            if(S!=XRPC_FRAME_MORE && S!=XRPC_FRAME_READY)goto Failed;
            if(Before || S!=XRPC_FRAME_READY){S=xrpcFrameGrow(R,true,R->HeaderBytes,false);if(S!=XRPC_FRAME_READY)goto Failed;if(I>Start)memcpy(R->Header+Before,In.Data+Start,I-Start);R->HeaderStored=R->HeaderBytes;}
            if(R->Ending!=4)goto More;
            S=xrpcFrameHead(R,Before?R->Header:In.Data+Start,R->HeaderBytes);if(S!=XRPC_FRAME_READY)goto Failed;R->InBody=true;
        }
    }
    {size_t Remaining=R->Length-R->BodyBytes,Available=In.Size-I,Take=Remaining<Available?Remaining:Available;
        if(!R->BodyBytes && Available>=Remaining){xbytesview View={Remaining?In.Data+I:NULL,Remaining};I+=Remaining;R->Ready=true;R->BodyBytes=R->Length;*Out=(xrpcframeview){View,R->HeaderBytes,R->HeaderBytes+R->Length,false};*Used=I;return XRPC_FRAME_READY;}
        if(Take){S=xrpcFrameGrow(R,false,R->BodyBytes+Take,false);if(S!=XRPC_FRAME_READY)goto Failed;memcpy(R->Body+R->BodyBytes,In.Data+I,Take);R->BodyBytes+=Take;R->BodyStored=R->BodyBytes;I+=Take;}
        if(R->BodyBytes==R->Length){R->Ready=true;*Out=(xrpcframeview){{R->Body,R->Length},R->HeaderBytes,R->HeaderBytes+R->Length,true};*Used=I;return XRPC_FRAME_READY;}
    }
More:*Used=I;return XRPC_FRAME_MORE;
Failed:R->Failure=S;*Used=I;return S;
}
XRPC_API xrpcframestatus xrpcFrameReaderEnd(xrpcframereader* R)
{if(!R)return xrpcFrameError(XERR_ARGUMENT,"frame reader is NULL"),XRPC_FRAME_ERROR;if(R->Failure!=XRPC_FRAME_MORE)return R->Failure;if(R->Ended)return XRPC_FRAME_END;R->Ended=true;if(!R->Ready && R->HeaderBytes){R->Failure=XRPC_FRAME_TRUNCATED;return R->Failure;}return XRPC_FRAME_END;}
XRPC_API bool xrpcFrameReaderReset(xrpcframereader* R)
{if(!R)return xrpcFrameError(XERR_ARGUMENT,"frame reader is NULL");xrpcFrameNext(R);return true;}
XRPC_API bool xrpcFrameReaderInfo(const xrpcframereader* R,xrpcframeinfo* Out)
{if(!xrpcFrameOutput(R,Out,sizeof(*Out)) || Out->Size!=sizeof(*Out))return xrpcFrameError(XERR_ARGUMENT,"invalid frame info output");*Out=(xrpcframeinfo){sizeof(*Out),sizeof(*R),R->HeaderCapacity,R->BodyCapacity,xrpcFrameStorage(R),R->Peak,R->HeaderBytes,R->BodyBytes,R->Length,R->InBody,R->Ready,R->Ended,R->Failure};return true;}
static bool xrpcFrameMeasure(size_t Length,const xrpcframeconfig* Config,xrpcframeconfig* C,size_t* Header)
{
    if(!xrpcFrameConfig(Config,C))return xrpcFrameError(XERR_ARGUMENT,"invalid frame config");
    if(Length>C->MaxBodyBytes)return xrpcFrameError(XERR_RANGE,"frame body limit");
    size_t Digits=1;for(size_t N=Length;N>=10;N/=10)++Digits;*Header=C->Format==XRPC_FRAME_UINT32_BE?4:20+Digits;
    if(*Header>C->MaxHeaderBytes || (C->Format==XRPC_FRAME_CONTENT_LENGTH && *Header-2>C->MaxLineBytes))return xrpcFrameError(XERR_RANGE,"frame header limit");
    return true;
}
XRPC_API bool xrpcFrameHeaderSize(size_t Length,const xrpcframeconfig* Config,size_t* Out)
{
    xrpcframeconfig C;size_t N;if(!xrtMemRangeValid(Out,sizeof(*Out)) || (Config && xrtMemRangesOverlap(Config,sizeof(*Config),Out,sizeof(*Out))))return xrpcFrameError(XERR_ARGUMENT,"invalid frame size output");if(!xrpcFrameMeasure(Length,Config,&C,&N))return false;*Out=N;return true;
}
static void xrpcFrameEmit(size_t Length,xrpcframeformat Format,bytes Out,size_t N)
{
    if(Format==XRPC_FRAME_UINT32_BE){for(unsigned I=0;I<4;++I)Out[I]=(unsigned char)(Length>>((3-I)*8));return;}
    memcpy(Out,"Content-Length: ",16);size_t I=N-4;do{Out[--I]=(unsigned char)('0'+Length%10);Length/=10;}while(Length);memcpy(Out+N-4,"\r\n\r\n",4);
}
static bool xrpcFrameWriteImpl(xbytesview Body,size_t Length,bool Complete,const xrpcframeconfig* Config,bytes Out,size_t Capacity,size_t* Written)
{
    xrpcframeconfig C;size_t Head;if(!xrpcFrameMeasure(Length,Config,&C,&Head))return false;size_t N=Head+(Complete?Length:0);
    if(!xrtMemRangeValid(Out,Capacity) || !xrtMemRangeValid(Out,N) || Capacity<N || !xrtMemRangeValid(Written,sizeof(*Written)) || (Complete && !xrtMemRangeValid(Body.Data,Body.Size)) ||
        xrtMemRangesOverlap(Out,N,Written,sizeof(*Written)) || (Config && (xrtMemRangesOverlap(Config,sizeof(*Config),Out,N) || xrtMemRangesOverlap(Config,sizeof(*Config),Written,sizeof(*Written)))) ||
        (Complete && (xrtMemRangesOverlap(Body.Data,Body.Size,Out,N) || xrtMemRangesOverlap(Body.Data,Body.Size,Written,sizeof(*Written)))))return xrpcFrameError(XERR_ARGUMENT,"invalid frame write capacity/spans/aliases");
    xrpcFrameEmit(Length,C.Format,Out,Head);if(Complete && Length)memcpy(Out+Head,Body.Data,Length);*Written=N;return true;
}
XRPC_API bool xrpcFrameHeaderWrite(size_t Length,const xrpcframeconfig* C,bytes Out,size_t Capacity,size_t* Written)
{return xrpcFrameWriteImpl((xbytesview){NULL,0},Length,false,C,Out,Capacity,Written);}
XRPC_API bool xrpcFrameWrite(xbytesview Body,const xrpcframeconfig* C,bytes Out,size_t Capacity,size_t* Written)
{return xrpcFrameWriteImpl(Body,Body.Size,true,C,Out,Capacity,Written);}
#endif
