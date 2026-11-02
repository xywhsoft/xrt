#include <xrpc/http_tls.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TLS echo at POST /rpc. Runtime CA and certificate identity are explicit;
 * neither an insecure verifier nor a private key is compiled into this demo. */
int main(int argc,char** argv)
{
    if(argc==1){puts("Usage: xrpc-http-tls DNS PORT CA.pem IDENTITY [AUTHORIZATION_FILE]");return 0;}if(argc!=5 && argc!=6)return 2;
    char* End=NULL;unsigned long Port=strtoul(argv[2],&End,10);if(*End || !Port || Port>65535)return 2;
    unsigned char Authorization[16384]={0};size_t AuthBytes=0;int Code=1;xnetengine* E=NULL;xnetresolver* D=NULL;xx509store* Trust=NULL;xtlsverifier* V=NULL;xrpchttpclient* C=NULL;xfuture *F=NULL,*Closed=NULL;
    FILE* File=fopen(argv[3],"rb");if(!File)return 1;unsigned char Bytes[65536];size_t Size=fread(Bytes,1,sizeof(Bytes),File);
    bool ReadOK=Size && Size<sizeof(Bytes) && !ferror(File);if(fclose(File) || !ReadOK)goto Done;
    Trust=xrtX509StoreCreate();if(!Trust || !xrtX509StoreAddData(Trust,(xbytesview){Bytes,Size},NULL))goto Done;
    xtlsverifierconfig K;xrtTlsVerifierConfigInit(&K);K.Store=Trust;xtlsverifylimits L;xrtTlsVerifyLimitsInit(&L);V=xrtTlsVerifierCreateLimited(&K,&L);if(!V)goto Done;
    xnetengineconfig N;xrtNetEngineConfigInit(&N);N.Workers=2;E=xrtNetEngineCreate(&N);if(!E || !xrtNetEngineStart(E))goto Done;
    D=xrtNetResolverCreate(NULL);if(!D)goto Done;xrpchttptlsconfig T;xrpcHttpTlsConfigInit(&T);T.Session.Verifier=V;T.Session.VerifyName=(xstrview){argv[4],strlen(argv[4])};T.Dial.Family=XNET_FAMILY_IPV4;
    xrpchttpconfig H;xrpcHttpConfigInit(&H);xhttpfield Auth={XRT_STR_LITERAL("Authorization"),{0}};
    if(argc==6){File=fopen(argv[5],"rb");if(!File)goto Done;AuthBytes=fread(Authorization,1,sizeof(Authorization),File);
        ReadOK=AuthBytes && AuthBytes<sizeof(Authorization) && !ferror(File);if(fclose(File) || !ReadOK)goto Done;
        if(AuthBytes && Authorization[AuthBytes-1]=='\n'){--AuthBytes;if(AuthBytes && Authorization[AuthBytes-1]=='\r')--AuthBytes;}
        if(!AuthBytes)goto Done;
        Auth.Value=(xstrview){(cstr)Authorization,AuthBytes};H.RequestHeaders=&Auth;H.RequestHeaderCount=1;}
    if(!xrpcHttpTlsDial(E,D,(xstrview){argv[1],strlen(argv[1])},(uint16)Port,1,(xstrview){argv[1],strlen(argv[1])},XRT_STR_LITERAL("/rpc"),&H,&T,&C))goto Done;
    xrtSecureZero(Authorization,sizeof(Authorization));
    if(xrpcHttpCallAsync(C,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[7]"),10000,&F)!=XRPC_ADMISSION_ACCEPTED || xrtFutureWaitFor(F,15000)!=XWAIT_OK)goto Done;
    const xrpccallresult* R=xrtFutureValue(F);xrpcresponseinfo I={0};I.Size=sizeof(I);
    if(R && R->Status==XRPC_CALL_TRANSPORT && R->Delivery==XRPC_NOT_SENT && R->Cause)fputs("TLS_CALL_TRANSPORT_NOT_SENT\n",stderr);
    if(!R || R->Status!=XRPC_CALL_OK || !xrpcResponsesInfo(R->Responses,&I) || I.Messages[0].Result.Raw.Size!=3 || memcmp(I.Messages[0].Result.Raw.Data,"[7]",3))goto Done;
    puts("[PASS] trusted TLS identity and JSON-RPC echo [7]");Code=0;
Done:
    xrtSecureZero(Authorization,sizeof(Authorization));
    if(C){Closed=xrpcHttpClosed(C);if(!Closed || !xrpcHttpClose(C) || xrtFutureWaitFor(Closed,15000)!=XWAIT_OK)Code=1;xrpcHttpRelease(C);}
    if(D && !xrtNetResolverDestroy(D))Code=1;
    if(E && (!xrtNetEngineStop(E) || !xrtNetEngineDestroy(E)))Code=1;
    xrtFutureDestroy(Closed);xrtFutureDestroy(F);xrtTlsVerifierRelease(V);xrtX509StoreFree(Trust);
    if(Code)fputs("TLS RPC failed: verify explicit CA/identity and typed call status.\n",stderr);
    return Code;
}
