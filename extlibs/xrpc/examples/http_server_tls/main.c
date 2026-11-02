#include <xrpc/http_server_tls.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <xrt/crypto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Runtime DER leaf certificate and native-supported DER/PKCS#8 private key; no credentials are
 * compiled in. Example: xrpc-https CERT.der KEY.der 8443 60000 rsa */
static bool readFile(cstr Name,unsigned char* Data,size_t Capacity,size_t* Size)
{FILE* F=fopen(Name,"rb");if(!F)return false;size_t N=fread(Data,1,Capacity,F);bool OK=N && N<Capacity && !ferror(F);if(fclose(F))OK=false;if(OK)*Size=N;return OK;}
static bool echo(const xrpcmessage* R,xrpcreply* Reply,const xrpcservercontext* Context,ptr User)
{(void)User;if(xrtCancelRequested(Context->Cancel) || xrtTimer()>=Context->Deadline)return false;if(R->Method.Raw.Size==10 && !memcmp(R->Method.Raw.Data,"\"identity\"",10) && Context->Data.Size)return xrpcReplyResult(Reply,(xstrview){(cstr)Context->Data.Data,Context->Data.Size});return xrpcReplyResult(Reply,R->Params.Raw.Size?R->Params.Raw:XRT_STR_LITERAL("null"));}
static xrpchttpauthstatus authorize(const xrpchttpauthrequest* R,bytes Out,size_t Cap,size_t* Used,ptr User)
{
    const xbytesview* Expected=User;const xhttpfield* F=NULL;
    if(xrtCancelRequested(R->Cancel) || xrtTimer()>=R->Deadline)return XRPC_HTTP_AUTH_ERROR;
    if(xrtHttpFieldGetUnique(R->Head->Fields,R->Head->FieldCount,XRT_STR_LITERAL("Authorization"),&F)!=XHTTP_NEXT_ITEM || F->Value.Size!=Expected->Size || !xrtConstTimeEqual(F->Value.Data,Expected->Data,Expected->Size))return XRPC_HTTP_AUTH_UNAUTHORIZED;
    if(Cap<4)return XRPC_HTTP_AUTH_ERROR;
    memcpy(Out,"[42]",4);*Used=4;return XRPC_HTTP_AUTH_ALLOW;
}
int main(int argc,char** argv)
{
    if(argc==1){puts("Usage: xrpc-https CERT.der KEY.der [PORT [DURATION_MS [rsa|p256|p384|ed25519 [AUTHORIZATION_FILE]]]]");return 0;}if(argc<3 || argc>7)return 2;
    char* End=NULL;unsigned long Port=argc>3?strtoul(argv[3],&End,10):0;if((argc>3 && (!*argv[3] || *End)) || Port>65535)return 2;
    unsigned long Duration=argc>4?strtoul(argv[4],&End,10):10000;if((argc>4 && (!*argv[4] || *End)) || !Duration || Duration>3600000)return 2;
    unsigned char Cert[65536],Key[8192],Token[16384]={0};size_t CertBytes=0,KeyBytes=0;xbytesview Expected={0};int Code=1;xtlsidentity* Identity=NULL;xnetengine* E=NULL;xrpchttpserver* H=NULL;
    if(!readFile(argv[1],Cert,sizeof(Cert),&CertBytes) || !readFile(argv[2],Key,sizeof(Key),&KeyBytes))goto Done;
    xbytesview Certificate={Cert,CertBytes},Private={Key,KeyBytes};cstr Kind=argc>5?argv[5]:"rsa";
    if(!strcmp(Kind,"rsa"))Identity=xrtTlsIdentityRsa(&Certificate,1,Private);
    else if(!strcmp(Kind,"p256"))Identity=xrtTlsIdentityP256(&Certificate,1,Private);
    else if(!strcmp(Kind,"p384"))Identity=xrtTlsIdentityP384(&Certificate,1,Private);
    else if(!strcmp(Kind,"ed25519"))Identity=xrtTlsIdentityEd25519(&Certificate,1,Private);
    xrtSecureZero(Key,sizeof(Key));if(!Identity)goto Done;
    xnetengineconfig N;xrtNetEngineConfigInit(&N);N.Workers=2;E=xrtNetEngineCreate(&N);if(!E || !xrtNetEngineStart(E))goto Done;
    xrpchttpserverconfig C;xrpcHttpServerConfigInit(&C);C.Execution.Call=echo;if(!xrtNetAddrParse(&C.Listen.Address,"127.0.0.1",(uint16)Port))goto Done;
    if(argc==7){size_t N=0;if(!readFile(argv[6],Token,sizeof(Token),&N))goto Done;if(N && Token[N-1]=='\n'){--N;if(N && Token[N-1]=='\r')--N;}if(!N || !xrtHttpFieldValueValid((xstrview){(cstr)Token,N}))goto Done;
        Expected=(xbytesview){Token,N};C.Authorize=authorize;C.AuthorizationUser=&Expected;
        if(N>=6 && !memcmp(Token,"Basic ",6))C.UnauthorizedChallenge=XRT_STR_LITERAL("Basic realm=\"rpc\"");}
    xrpchttpservertlsconfig K;xrpcHttpServerTlsConfigInit(&K);K.Session.Identity=Identity;
    if(!xrpcHttpServerTlsCreate(E,&C,&K,&H))goto Done;
    xrtTlsIdentityRelease(Identity);Identity=NULL;
    if(!xrpcHttpServerStart(H))goto Done;
    xnetaddr Address;char Endpoint[80];
    if(!xrpcHttpServerLocal(H,&Address) || xrtNetAddrEndpointText(&Address,Endpoint,sizeof(Endpoint))>=sizeof(Endpoint))goto Done;
    printf("https://%s/rpc\n",Endpoint);fflush(stdout);xrtSleep((int64)Duration);Code=0;
Done:
    xrtSecureZero(Key,sizeof(Key));
    if(H){
        if(!xrpcHttpServerClose(H) || xrpcHttpServerWaitFor(H,15000)!=XWAIT_OK)Code=1;
        else{xrpchttpserverinfo I={0};xrpchttpservertlsinfo T={0};I.Size=sizeof(I);T.Size=sizeof(T);
            if(!xrpcHttpServerInfo(H,&I) || !xrpcHttpServerTlsInfo(H,&T))Code=1;
            else printf("stats {\"requests\":%llu,\"dispatched\":%llu,\"connections\":%zu,\"queued\":%zu,\"active_handshakes\":%zu,\"admitted\":%llu,\"completed\":%llu,\"failed\":%llu}\n",(unsigned long long)I.Requests,(unsigned long long)I.Dispatched,I.Connections,I.QueuedBytes,T.ActiveHandshakes,(unsigned long long)T.Admitted,(unsigned long long)T.Completed,(unsigned long long)T.Failed);
        }
        if(!xrpcHttpServerDestroy(H))Code=1;
    }
    if(E && (!xrtNetEngineStop(E) || !xrtNetEngineDestroy(E)))Code=1;
    xrtTlsIdentityRelease(Identity);
    xrtSecureZero(Token,sizeof(Token));
    if(Code)fputs("TLS server failed; check runtime identity and resource policies.\n",stderr);
    return Code;
}
