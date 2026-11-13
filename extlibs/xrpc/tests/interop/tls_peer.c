#include <xrpc/tls.h>
#include <xrt/thread.h>
#include <xrt/memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static xatomic32 Ready;static xrpctls* Peer;static xrpctlsconfig Config;static xtlsserverconfig NativeServer;
static xbytesview Pin;static const xstrview Protocol={"xrpc-local-test",15};
static bool tlsInteropRead(cstr Name,unsigned char* Bytes,size_t Capacity,size_t* Count)
{FILE* F=fopen(Name,"rb");if(!F)return false;size_t N=fread(Bytes,1,Capacity,F);bool OK=N && N<Capacity && !ferror(F);if(fclose(F))OK=false;if(OK)*Count=N;return OK;}
static bool tlsInteropEcho(const xrpcmessage* M,xrpcreply* R,const xrpcservercontext* C,ptr U)
{(void)C;(void)U;return xrpcReplyResult(R,M->Params.Raw.Size?M->Params.Raw:XRT_STR_LITERAL("null"));}
static xtlsverifydecision tlsInteropPin(const xtlspeer* P,ptr User)
{(void)User;return P && P->CertificateCount && P->Name.Size==8 && !memcmp(P->Name.Data,"rpc.test",8) && P->Certificates[0].Raw.Size==Pin.Size && !memcmp(P->Certificates[0].Raw.Data,Pin.Data,Pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;}
static bool tlsInteropAccept(xnetlistener* L,xnetstream* S,ptr U)
{(void)L;(void)U;if(!xrpcTlsAccept(S,&NativeServer,&Config,&Peer))return false;xrtAtomic32Store(&Ready,1,XMEMORY_RELEASE);return true;}
int main(int Argc,char** Argv)
{
    if(Argc!=7)return 1;
    unsigned Format=(unsigned)strtoul(Argv[1],NULL,10),Port=(unsigned)strtoul(Argv[6],NULL,10);bool Client=!strcmp(Argv[5],"connect");if(Format<1 || Format>2 || Port>65535 || (Client && !Port))return 2;
    unsigned char Certificate[65536],PrivateKey[8192];size_t CertBytes=0,KeyBytes=0;if(!tlsInteropRead(Argv[2],Certificate,sizeof(Certificate),&CertBytes))return 3;Pin=(xbytesview){Certificate,CertBytes};
    xtlsidentity* Identity=NULL;
    if(!Client){if(!tlsInteropRead(Argv[3],PrivateKey,sizeof(PrivateKey),&KeyBytes))return 4;xbytesview Key={PrivateKey,KeyBytes};
        if(!strcmp(Argv[4],"rsa"))Identity=xrtTlsIdentityRsa(&Pin,1,Key);else if(!strcmp(Argv[4],"p256"))Identity=xrtTlsIdentityP256(&Pin,1,Key);else if(!strcmp(Argv[4],"p384"))Identity=xrtTlsIdentityP384(&Pin,1,Key);else if(!strcmp(Argv[4],"ed25519"))Identity=xrtTlsIdentityEd25519(&Pin,1,Key);xrtSecureZero(PrivateKey,sizeof(PrivateKey));if(!Identity)return 5;}
    xrtAtomic32Init(&Ready,0);xnetengineconfig C;xrtNetEngineConfigInit(&C);C.Workers=2;xnetengine* E=xrtNetEngineCreate(&C);if(!E || !xrtNetEngineStart(E))return 6;
    xrpcserverconfig S;xrpcServerConfigInit(&S);S.Call=tlsInteropEcho;xrpcserver* Server=NULL;if(!xrpcServerCreate(&S,&Server))return 7;
    xrpcTlsConfigInit(&Config);Config.Duplex.Server=Server;Config.Frame.Format=(xrpcframeformat)Format;Config.ReadChunkBytes=7;Config.ReadBytes=37;Config.WriteChunkBytes=11;Config.WriteBytes=43;Config.WorkFrames=3;
    xnetlistener* Listener=NULL;xtlsverifier* Verifier=NULL;
    if(Client){xtlsverifierconfig V;xrtTlsVerifierConfigInit(&V);V.Verify=tlsInteropPin;Verifier=xrtTlsVerifierCreate(&V);if(!Verifier)return 8;xtlsclientconfig TLS;xrtTlsClientConfigInit(&TLS);TLS.Verifier=Verifier;TLS.ServerName=XRT_STR_LITERAL("rpc.test");TLS.Protocols=&Protocol;TLS.ProtocolCount=1;xnetaddr Address;if(!xrtNetAddrParse(&Address,"127.0.0.1",(uint16)Port) || !xrpcTlsConnect(E,&Address,1,&TLS,&Config,&Peer))return 9;xrtAtomic32Store(&Ready,1,XMEMORY_RELEASE);puts("READY");fflush(stdout);}
    else{xrtTlsServerConfigInit(&NativeServer);NativeServer.Identity=Identity;NativeServer.Protocols=&Protocol;NativeServer.ProtocolCount=1;xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrParse(&L.Address,"127.0.0.1",0);xnetlistenerevents Events={tlsInteropAccept,NULL,NULL};Listener=xrtNetListen(E,&L,&Events,NULL,NULL);xnetaddr Address;if(!Listener || !xrtNetListenerLocal(Listener,&Address))return 10;printf("PORT %u\n",(unsigned)Address.Port);fflush(stdout);}
    double End=xrtTimer()+15;while(!xrtAtomic32Load(&Ready,XMEMORY_ACQUIRE)){if(xrtTimer()>=End)return 11;xrtSleep(1);}
    xfuture* F=NULL;if(xrpcTlsCall(Peer,XRT_STR_LITERAL("python_echo"),XRT_STR_LITERAL("[42]"),10000,&F)!=XRPC_DUPLEX_ACCEPTED || xrtFutureWaitFor(F,15000)!=XWAIT_OK)return 12;
    const xrpccallresult* R=xrtFutureValue(F);if(!R || R->Status!=XRPC_CALL_OK || R->Delivery!=XRPC_REPLIED)return 13;
    xrpctlsinfo Info={0};Info.Size=sizeof(Info);if(!xrpcTlsInfo(Peer,&Info) || !Info.Version || !Info.Cipher || Info.ProtocolBytes!=Protocol.Size || memcmp(Info.Protocol,Protocol.Data,Protocol.Size))return 14;
    if(xrpcTlsWaitFor(Peer,15000)!=XWAIT_OK || !xrpcTlsDestroy(Peer))return 15;
    xrtFutureDestroy(F);if(Listener){xrtNetListenerClose(Listener);xrtNetListenerDestroy(Listener);}if(!xrpcServerDestroy(Server))return 16;
    End=xrtTimer()+10;xnetretireresult Q;while((Q=xrtNetEngineTryDestroy(E))==XNET_RETIRE_BUSY){if(xrtTimer()>=End)return 17;xrtSleep(1);}if(Q!=XNET_RETIRE_READY)return 18;xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);
    printf("PASS native TLS role=%s version=%x cipher=%x\n",Client?"client":"server",(unsigned)Info.Version,(unsigned)Info.Cipher);return 0;
}
