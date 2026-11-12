/* Consume owned-server metadata and binary invocation corpus on stdin. */
#include <xredis/command_keys.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif
static void ckRequire(bool OK,cstr Text){if(!OK){fprintf(stderr,"[FAIL] key oracle: %s\n",Text);exit(1);}}
static uint32 ckU32(void)
{unsigned char B[4];ckRequire(fread(B,1,4,stdin)==4,"corpus integer");return (uint32)B[0]|((uint32)B[1]<<8)|((uint32)B[2]<<16)|((uint32)B[3]<<24);}
static void ckHex(xbytesview V)
{putchar('"');for(size_t I=0;I<V.Size;++I)printf("%02x",(unsigned)V.Data[I]);putchar('"');}
int main(void)
{
#if defined(_WIN32)
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    uint32 Bytes=ckU32();ckRequire(Bytes<=16u*1024u*1024u,"metadata byte cap");xredisreplyreader* Reader=NULL;xredisreply* R=NULL;ckRequire(xredisReplyReaderCreate(NULL,&Reader),"reader create");unsigned char Input[4096];
    while(Bytes){size_t N=Bytes;if(N>sizeof(Input))N=sizeof(Input);ckRequire(fread(Input,1,N,stdin)==N,"metadata input");size_t At=0;while(At<N){size_t Used=0;ckRequire(!R,"one metadata frame");xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){Input+At,N-At},&Used,&R);ckRequire(S!=XREDIS_REPLY_ERROR && (Used || S==XREDIS_REPLY_YIELD),"metadata parse");At+=Used;}Bytes-=(uint32)N;}
    ckRequire(R && xredisReplyReaderFinish(Reader),"metadata complete");xredisReplyReaderDestroy(Reader);xrediscommandcatalog* C=NULL;xrediskeysstatus Built=xredisCommandCatalogCreate(R,NULL,&C);xredisReplyRelease(R);
    printf("{\"status\":%d",(int)Built);if(Built!=XREDIS_KEYS_OK){puts("}");return 0;}
    xrediscommandcataloginfo Info={0};Info.Size=sizeof(Info);ckRequire(xredisCommandCatalogInfo(C,&Info),"catalogue info");printf(",\"commands\":%zu,\"specifications\":%zu,\"cases\":[",Info.Commands,Info.Specifications);
    uint32 Cases=ckU32();ckRequire(Cases<=2000,"case cap");
    for(uint32 I=0;I<Cases;++I){uint32 Count=ckU32();ckRequire(Count<=512,"argument cap");xbytesview A[512];xrediscommandkey Keys[512];
        for(uint32 J=0;J<Count;++J){size_t N=ckU32();ckRequire(N<=65536,"argument bytes");unsigned char* P=xrtMalloc(N+1);ckRequire(P && fread(P,1,N,stdin)==N,"argument input");A[J]=(xbytesview){P,N};}
        xrediscommandkeyresult V={0};V.Size=sizeof(V);xrediskeysstatus S=xredisCommandCatalogEvaluate(C,A,Count,Keys,512,&V);
        if(I)putchar(',');
        printf("{\"status\":%d",(int)S);
        if(S==XREDIS_KEYS_OK || S==XREDIS_KEYS_NONE){printf(",\"slot\":%u,\"legacy\":%s,\"variable\":%s,\"keys\":[",(unsigned)V.Slot,V.Legacy?"true":"false",V.VariableFlags?"true":"false");
            for(size_t K=0;K<V.Keys;++K){if(K)putchar(',');printf("{\"argument\":%zu,\"flags\":%u,\"key\":",Keys[K].Argument,(unsigned)Keys[K].Flags);ckHex(A[Keys[K].Argument]);putchar('}');}putchar(']');}
        putchar('}');for(uint32 J=0;J<Count;++J)xrtFree((ptr)A[J].Data);
    }
    ckRequire(fgetc(stdin)==EOF,"no trailing corpus");puts("]}");xredisCommandCatalogRelease(C);return 0;
}
