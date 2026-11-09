#include <xmqtt.h>
#include "../../examples/tls/material.h"
#include <stdlib.h>
/* Independent SSL peer driver. Key/CA contents are runtime-only files. */
static void tlsInteropRequire(bool OK,cstr Message)
{if(!OK){fprintf(stderr,"[FAIL] MQTT TLS interoperability: %s cause=%s\n",Message,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");exit(1);}}
static const void* tlsInteropWait(xfuture* F)
{tlsInteropRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureState(F)==XFUTURE_RESOLVED,"bounded typed outcome");return xrtFutureValue(F);}
int main(int Count,char** Args)
{
    /* port mqtt-version tls-version factory expected-failure CA client-cert|- key|- name */
    if(Count!=10)return 2;
    unsigned long Port=strtoul(Args[1],NULL,10);unsigned Version=(unsigned)strtoul(Args[2],NULL,10),Tls=(unsigned)strtoul(Args[3],NULL,10),Factory=(unsigned)strtoul(Args[4],NULL,10);bool Failure=atoi(Args[5])!=0;
    tlsInteropRequire(Port && Port<=UINT16_MAX && (Version==5 || Version==311) && (Tls==12 || Tls==13) && Factory<3,"driver arguments");
    size_t Bytes=0;bytes CA=xmqttExampleTlsRead(Args[6],65536,&Bytes);xx509store* Store=xrtX509StoreCreate();tlsInteropRequire(CA && Store && xrtX509StoreAddData(Store,(xbytesview){CA,Bytes},NULL),"bounded trust anchors");xrtSecureZero(CA,Bytes);xrtFree(CA);
    xtlsverifierconfig Verify;xrtTlsVerifierConfigInit(&Verify);Verify.Store=Store;xtlsverifylimits Limits;xrtTlsVerifyLimitsInit(&Limits);xtlsverifier* Verifier=xrtTlsVerifierCreateLimited(&Verify,&Limits);xrtX509StoreFree(Store);tlsInteropRequire(Verifier!=NULL,"strict native verifier");
    xtlsversion Selected=Tls==12?XTLS_VERSION_12:XTLS_VERSION_13;xtlscipher Cipher=Tls==12?XTLS_ECDHE_RSA_AES_128_GCM_SHA256:XTLS_AES_128_GCM_SHA256;
    xtlspolicy Policy;xrtTlsPolicyInit(&Policy);Policy.Versions=&Selected;Policy.VersionCount=1;Policy.Ciphers=&Cipher;Policy.CipherCount=1;
    xtlssignature Signature=XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256;Policy.Signatures=&Signature;Policy.SignatureCount=1;
    xtlscontextconfig TC;xrtTlsContextConfigInit(&TC);TC.Policy=&Policy;xtlscontext* Context=xrtTlsContextCreate(&TC);tlsInteropRequire(Context!=NULL,"version-specific context");
    xnetengineconfig EC;xrtNetEngineConfigInit(&EC);EC.Workers=2;xnetengine* Engine=xrtNetEngineCreate(&EC);tlsInteropRequire(Engine && xrtNetEngineStart(Engine),"host engine");xnetresolver* Resolver=xrtNetResolverCreate(NULL);tlsInteropRequire(Resolver!=NULL,"host resolver");
    xmqttclienttlsconfig T;xmqttClientTlsConfigInit(&T);T.Session.Context=Context;T.Session.Verifier=Verifier;T.Session.ServerName=XRT_STR_LITERAL("localhost");T.Session.VerifyName=(xstrview){Args[9],strlen(Args[9])};
    char Alpn[4]={'m','q','t','t'};xstrview Protocol={Alpn,4};T.Session.Protocols=&Protocol;T.Session.ProtocolCount=1;
    xtlsidentity* Identity=NULL;xtlsclientauthconfigv1 Auth;
    if(strcmp(Args[7],"-")){Identity=xmqttExampleTlsIdentity(Args[7],Args[8]);tlsInteropRequire(Identity!=NULL,"runtime client identity");xrtTlsClientAuthConfigInitV1(&Auth);Auth.Identity=Identity;T.ClientAuth=&Auth;}
    xmqttclientconfig K;xmqttClientConfigInit(&K);K.Version=Version==5?XMQTT_VERSION_5:XMQTT_VERSION_311;K.ReadBytes=7;K.WriteBytes=13;K.WriteChunkBytes=5;K.WorkPackets=3;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.CleanStart=true;Q.Body.Connect.ClientId=XRT_STR_LITERAL("tls-interop");xmqttclient* C=NULL;xnetaddr Address;xrtNetAddrLoopback(&Address,XNET_FAMILY_IPV4,(uint16)Port);bool OK;
    if(!Factory)OK=xmqttClientTlsConnect(Engine,&Address,1,&K,&T,&Q,&C);
    else if(Factory==1){char Host[9];memcpy(Host,"localhost",9);OK=xmqttClientTlsDial(Engine,Resolver,(xstrview){Host,9},(uint16)Port,1,&K,&T,&Q,&C);memset(Host,'x',sizeof(Host));}
    else {xnetaddrlist* L=xrtNetAddrListCreate(&Address,1);tlsInteropRequire(L!=NULL,"prepared endpoint");OK=xmqttClientTlsDialAddresses(Engine,L,1,&K,&T,&Q,&C);xrtNetAddrListDestroy(L);}
    tlsInteropRequire(OK,"TLS factory admission");xrtTlsVerifierRelease(Verifier);xrtTlsIdentityRelease(Identity);xrtTlsContextRelease(Context);memset(&T,0xa5,sizeof(T));memset(&Auth,0xa5,sizeof(Auth));memset(Alpn,'x',sizeof(Alpn));Protocol=(xstrview){0};
    xfuture* Ready=xmqttClientReady(C);const xmqttreadyresult* R=tlsInteropWait(Ready);
    if(Failure){tlsInteropRequire(R->Status==XMQTT_STATUS_TRANSPORT && R->Cause,"verification/mutual-auth refusal before MQTT readiness");}
    else {
        if(R->Status!=XMQTT_STATUS_OK)fprintf(stderr,"TLS readiness status=%d cause=%s\n",(int)R->Status,R->Cause?xrtErrorMessage(R->Cause):"none");
        tlsInteropRequire(R->Status==XMQTT_STATUS_OK,"strict TLS plus CONNACK readiness");
        for(unsigned Qos=0;Qos<3;++Qos){xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Q);Q.Flags=(uint8)(Qos<<1);Q.Body.Publish.Topic=XRT_STR_LITERAL("tls/topic");Q.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");xfuture* F=NULL;
            tlsInteropRequire(xmqttClientPublishAsync(C,&Q,5000,&F)==XMQTT_ADMISSION_ACCEPTED,"publish admission");const xmqttwriteresult* W=tlsInteropWait(F);tlsInteropRequire(W->Status==XMQTT_STATUS_OK && W->AcceptedBytes,"QoS0/1/2 result");xrtFutureDestroy(F);}
        xfuture* Bye=NULL;tlsInteropRequire(xmqttClientDisconnectAsync(C,NULL,5000,&Bye)==XMQTT_ADMISSION_ACCEPTED,"graceful admission");tlsInteropRequire(((const xmqttwriteresult*)tlsInteropWait(Bye))->Status==XMQTT_STATUS_OK,"authenticated close");xrtFutureDestroy(Bye);
    }
    tlsInteropRequire(xmqttClientClose(C) && xmqttClientWaitFor(C,10000)==XWAIT_OK && xmqttClientDestroy(C),"actual Client/session/native retirement");xrtFutureDestroy(Ready);
    tlsInteropRequire(xrtNetResolverDestroy(Resolver) && xrtNetEngineDestroy(Engine),"actual native join before unload");puts("[PASS] independent SSL MQTT TLS case");return 0;
}
