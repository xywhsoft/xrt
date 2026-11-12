#include <xrpc/frame.h>
#include <xrpc/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif
typedef struct frpeeroutput {unsigned char Wire[65536];size_t Bytes,Count;bool Batch;} frpeeroutput;
static bool frpeerCall(const xrpcmessage* M,xrpcreply* Reply,ptr Data)
{(void)Data;if(M->Method.Raw.Size==6 && !memcmp(M->Method.Raw.Data,"\"echo\"",6))return xrpcReplyResult(Reply,M->Params.Raw.Size?M->Params.Raw:(xstrview)XRT_STR_LITERAL("null"));return false;}
static bool frpeerEmit(const xrpcoutgoing* M,bool Batch,size_t Bytes,ptr Data)
{
    frpeeroutput* O=Data;size_t Prefix=Batch?1:0;if(Bytes>sizeof(O->Wire)-O->Bytes-Prefix-1)return false;
    if(Batch)O->Wire[O->Bytes++]=(unsigned char)(O->Count?',':'[');
    size_t Written=0;
    if(!xrpcEncodeWrite(O->Wire+O->Bytes,sizeof(O->Wire)-O->Bytes,M,1,false,NULL,&Written) || Written!=Bytes)return false;
    O->Bytes+=Written;++O->Count;O->Batch=Batch;return true;
}
int main(int Argc,char** Argv)
{
    unsigned Format=Argc>1?(unsigned)strtoul(Argv[1],NULL,10):1;size_t Chunk=Argc>2?(size_t)strtoull(Argv[2],NULL,10):4096;if(Format<1 || Format>2 || !Chunk || Chunk>4096 || Argc>3)return 2;
#if defined(_WIN32)
    if(_setmode(_fileno(stdin),_O_BINARY)==-1 || _setmode(_fileno(stdout),_O_BINARY)==-1)return 3;
#endif
    xrpcframeconfig C;xrpcFrameConfigInit(&C);C.Format=(xrpcframeformat)Format;C.MaxBodyBytes=65536;xrpcframereader* R=NULL;if(!xrpcFrameReaderCreate(&C,&R))return 4;
    unsigned char Input[4096],Header[64];size_t Frames=0,Replies=0;int Code=0;for(;;){size_t N=fread(Input,1,Chunk,stdin);if(!N){if(ferror(stdin))Code=5;break;}size_t At=0;
        while(At<N){size_t Used=0;xrpcframeview V;xrpcframestatus S=xrpcFrameReaderFeed(R,(xbytesview){Input+At,N-At},&Used,&V);At+=Used;
            if(S==XRPC_FRAME_MORE){if(!Used){Code=6;goto Done;}continue;}if(S!=XRPC_FRAME_READY){Code=20+(int)S;goto Done;}
            ++Frames;frpeeroutput Out={0};xrpcmessage Scratch[64];xrpcdispatchinfo Info={0};Info.Size=sizeof(Info);xrpcdispatchconfig Dispatch;xrpcDispatchConfigInit(&Dispatch);Dispatch.Decode.MaxMessages=64;Dispatch.MaxReplies=64;Dispatch.MaxOutputBytes=sizeof(Out.Wire);
            if(!xrpcDispatch((xstrview){(cstr)V.Body.Data,V.Body.Size},&Dispatch,Scratch,64,frpeerCall,NULL,frpeerEmit,&Out,&Info)){Code=7;goto Done;}
            if(Out.Batch && Out.Count)Out.Wire[Out.Bytes++]=']';
            if(!Out.Bytes)continue;
            size_t H=0;if(!xrpcFrameHeaderWrite(Out.Bytes,&C,Header,sizeof(Header),&H) || fwrite(Header,1,H,stdout)!=H || fwrite(Out.Wire,1,Out.Bytes,stdout)!=Out.Bytes || fflush(stdout)){Code=8;goto Done;}++Replies;
        }
    }
    {xrpcframestatus End=xrpcFrameReaderEnd(R);if(!Code && End!=XRPC_FRAME_END)Code=20+(int)End;}
Done:xrpcFrameReaderDestroy(R);fprintf(stderr,"frames=%zu replies=%zu code=%d\n",Frames,Replies,Code);return Code;
}
