/* Shared fixture credentials/trust loading only; implementation uses public XRT APIs. */
#define XREDIS_MODULE_XREDIS_ROLE_CLIENT
#define INTEROP_TLS
#define main roleInitializationReference
#include "redis_init.c"
#undef main
#include <xredis/role_client.h>
static unsigned RoleMode,RoleVersion,RoleWorker;
static int RoleAuth;
static bool roleRealConnect(xnetengine* E,const xnetaddr* A,xredisinitmode Mode,uint32 Flag,xredisclient** C)
{
    xredisclientinitconfig I;interopOptions(&I,RoleAuth);I.Mode=Mode;I.Flags|=Flag;size_t Worker=RoleWorker++%2;
    if(RoleMode==2){xnetaddrlist* L=xrtNetAddrListCreate(A,1);interopRequire(L!=NULL,"role prepared address snapshot");bool OK=RoleVersion?xredisClientTlsDialAddresses(E,L,Worker,NULL,&I,&InteropTLS,C):xredisClientDialAddresses(E,L,Worker,NULL,&I,NULL,C);xrtNetAddrListDestroy(L);return OK;}
    if(RoleMode==1)return RoleVersion?xredisClientTlsDial(E,InteropResolver,XRT_STR_LITERAL("localhost"),A->Port,Worker,NULL,&I,&InteropTLS,C):xredisClientDial(E,InteropResolver,XRT_STR_LITERAL("localhost"),A->Port,Worker,NULL,&I,NULL,C);
    return RoleVersion?xredisClientTlsConnect(E,A,Worker,NULL,&I,&InteropTLS,C):xredisClientConnectInit(E,A,Worker,NULL,&I,C);
}
int main(int Argc,char** Argv)
{
    if(Argc!=7 || strlen(Argv[6])!=40)return 2;
    unsigned long Port=strtoul(Argv[1],NULL,10);RoleAuth=atoi(Argv[2]);RoleMode=(unsigned)atoi(Argv[3]);RoleVersion=(unsigned)atoi(Argv[4]);
    if(!Port || Port>65535 || RoleMode>2 || (RoleVersion!=0 && RoleVersion!=12 && RoleVersion!=13) || RoleAuth<0 || RoleAuth>2)return 2;
    xtlsverifier* Trust=NULL;xtlscontext* Context=NULL;
    if(RoleVersion){Trust=interopTrust(Argv[5]);xtlspolicy Policy;xrtTlsPolicyInit(&Policy);xtlsversion V=RoleVersion==12?XTLS_VERSION_12:XTLS_VERSION_13;xtlscipher Cipher=RoleVersion==12?XTLS_ECDHE_RSA_AES_128_GCM_SHA256:XTLS_AES_128_GCM_SHA256;xtlssignature Signature=XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;Policy.Versions=&V;Policy.VersionCount=1;Policy.Ciphers=&Cipher;Policy.CipherCount=1;Policy.Signatures=&Signature;Policy.SignatureCount=1;xtlscontextconfig K;xrtTlsContextConfigInit(&K);K.Policy=&Policy;Context=xrtTlsContextCreate(&K);interopRequire(Context!=NULL,"role TLS policy");xredisClientTlsConfigInit(&InteropTLS);InteropTLS.Session.Context=Context;InteropTLS.Session.Verifier=Trust;InteropTLS.Session.ServerName=XRT_STR_LITERAL("localhost");InteropTLS.Session.VerifyName=XRT_STR_LITERAL("localhost");InteropTLS.WriteBytes=3;}
    xnetengineconfig K;xrtNetEngineConfigInit(&K);K.Workers=2;xnetengine* E=xrtNetEngineCreate(&K);interopRequire(E && xrtNetEngineStart(E),"role engine");InteropResolver=xrtNetResolverCreate(NULL);interopRequire(InteropResolver!=NULL,"role resolver");xnetaddr A;interopRequire(xrtNetAddrParse(&A,"127.0.0.1",(uint16)Port),"role address");
    xfuture* Retained[6];size_t Count=0;
    const xredisinitmode Modes[]={XREDIS_INIT_RESP2,XREDIS_INIT_RESP3,XREDIS_INIT_AUTO};
    for(unsigned M=0;M<3;++M)for(unsigned R=0;R<3;++R){uint32 Flag=R==0?XREDIS_INIT_REQUIRE_MASTER:R==1?XREDIS_INIT_REQUIRE_REPLICA:XREDIS_INIT_REQUIRE_SENTINEL;xredisclient* C=NULL;interopRequire(roleRealConnect(E,&A,Modes[M],Flag,&C),"real role factory");xfuture* F=xredisClientReady(C);xfuture* Ping=NULL;xbytesview Word=XRT_BYTES_LITERAL("PING");interopRequire(xredisClientCallAsync(C,&Word,1,5000,&Ping)==XREDIS_ADMISSION_ACCEPTED,"real queued role business");const xredisreadyresult* Ready=interopWait(F);const xredisrequestresult* B=interopWait(Ping);
        if(!R){interopRequire(Ready->Status==XREDIS_READY_OK && Ready->Step==XREDIS_INIT_DONE && B->Status==XREDIS_REQUEST_OK && B->Delivery==XREDIS_REPLIED,"real master matches before queued PING");}
        else interopRequire(Ready->Status==XREDIS_READY_ROLE_MISMATCH && Ready->Step==XREDIS_INIT_ROLE && !Ready->Hello && B->Delivery==XREDIS_NOT_SENT,"real wrong role fails closed");
        interopClose(C);xrtFutureDestroy(Ping);if(M<2 && !R)Retained[Count++]=F;else xrtFutureDestroy(F);
    }
    /* Verify the owned run identity before changing the disposable ACL. */
    xredisclient* Admin=NULL;interopRequire(roleRealConnect(E,&A,XREDIS_INIT_RESP3,0,&Admin),"owned ACL client");xfuture* AF=xredisClientReady(Admin);interopRequire(((const xredisreadyresult*)interopWait(AF))->Status==XREDIS_READY_OK,"owned ACL ready");xbytesview Identify[]={XRT_BYTES_LITERAL("INFO"),XRT_BYTES_LITERAL("SERVER")};xfuture* ID=interopCall(Admin,Identify,2);xredisreplyvalue Server=interopValue(ID);bool Match=false;for(size_t N=0;N+47<=Server.Data.Size;++N)if(!memcmp(Server.Data.Data+N,"run_id:",7) && !memcmp(Server.Data.Data+N+7,Argv[6],40)){Match=true;break;}interopRequire(Match,"owned Redis run identity before ACL mutation");xrtFutureDestroy(ID);
    xbytesview User=RoleAuth==2?(xbytesview)XRT_BYTES_LITERAL(INTEROP_USERNAME):(xbytesview)XRT_BYTES_LITERAL("default");xbytesview Deny[]={XRT_BYTES_LITERAL("ACL"),XRT_BYTES_LITERAL("SETUSER"),User,XRT_BYTES_LITERAL("-role")};xfuture* Changed=interopCall(Admin,Deny,4);xrtFutureDestroy(Changed);
    xredisclient* C=NULL;interopRequire(roleRealConnect(E,&A,XREDIS_INIT_RESP3,XREDIS_INIT_REQUIRE_MASTER,&C),"ROLE ACL admission");xfuture* Ready=xredisClientReady(C);const xredisreadyresult* Failure=interopWait(Ready);interopRequire(Failure->Status==XREDIS_READY_SERVER_ERROR && Failure->Step==XREDIS_INIT_ROLE && !Failure->Hello,"actual ROLE NOPERM cannot publish ready");interopClose(C);xrtFutureDestroy(Ready);Deny[3]=(xbytesview)XRT_BYTES_LITERAL("+role");Changed=interopCall(Admin,Deny,4);xrtFutureDestroy(Changed);interopClose(Admin);xrtFutureDestroy(AF);
    interopRequire(xrtNetResolverDestroy(InteropResolver) && xrtNetEngineStop(E) && xrtNetEngineDestroy(E),"role physical owners joined");for(size_t N=0;N<Count;++N){interopRequire(((const xredisreadyresult*)interopWait(Retained[N]))->Status==XREDIS_READY_OK,"role ready survives engine teardown");xrtFutureDestroy(Retained[N]);}if(Trust)xrtTlsVerifierRelease(Trust);if(Context)xrtTlsContextRelease(Context);
    printf("[PASS] real role barrier: RESP2/3/AUTO, master match, replica/Sentinel mismatch, ROLE ACL, queued delivery and retained outcomes; mode=%u TLS=%u auth=%d\n",RoleMode,RoleVersion,RoleAuth);return 0;
}
