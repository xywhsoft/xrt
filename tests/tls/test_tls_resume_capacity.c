#include "../fixtures/tls_server.h"
#include <xrt/tls_client_auth.h>
static void capacityCheck(xtlssession* S,size_t Expected)
{
    testRequire(xrtTlsClientResumeCount(S)==0&&xrtTlsClientResumeDropped(S)==0,"empty queues do not describe configured capacity");
    xrtSetErrorInfo(XERR_STATE,"capacity.sentinel",1,"preserve");const xerror* Error=xrtGetError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"one-byte actual query allocation budget");
    for(unsigned I=0;I<10000;++I)testRequire(xrtTlsClientResumeCapacity(S)==Expected&&xrtGetError()==Error,"immutable capacity query preserves error and storage");
    testRequire(xrtAllocationBudgetLeave(&Scope),"leave query scope");xallocationbudgetinfo Info={.Size=sizeof(Info)};testRequire(xrtAllocationBudgetInfo(B,&Info)&&!Info.PeakBytes&&!Info.Rejections&&!Info.LiveCount,"capacity query allocates nothing");xrtAllocationBudgetFree(B);xrtClearError();
}
int main(void)
{
    xtlscontext* Context=testTlsServerContext();xtlsverifierconfig VC;xrtTlsVerifierConfigInit(&VC);VC.Verify=testTlsServerAccept;xtlsverifier* Verify=xrtTlsVerifierCreate(&VC);testRequire(Context&&Verify,"native context and verifier");
    for(size_t I=0;I<3;++I){size_t Limit=I==2?4:I;xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=Context;K.Verifier=Verify;K.ServerName=XRT_STR_LITERAL("example.com");K.ResumeLimit=Limit;xtlssession* S=xrtTlsClientCreate(&K,NULL);testRequire(S!=NULL,"native client with explicit capacity");K.ResumeLimit=17;capacityCheck(S,Limit);xrtTlsSessionDestroy(S);}
    xtlsidentity* Identity=testTlsServerIdentity();xtlsclientconfig K;xrtTlsClientConfigInit(&K);K.Context=Context;K.Verifier=Verify;K.ServerName=XRT_STR_LITERAL("example.com");K.ResumeLimit=4;xtlsclientauthconfigv1 A;xrtTlsClientAuthConfigInitV1(&A);A.Identity=Identity;xtlssession* S=xrtTlsClientCreateAuthV1(&K,&A,NULL);testRequire(S!=NULL,"native authenticated-client construction");capacityCheck(S,0);xrtTlsSessionDestroy(S);
    xtlsserverconfig Server;xrtTlsServerConfigInit(&Server);Server.Context=Context;Server.Identity=Identity;S=xrtTlsServerCreate(&Server,NULL);testRequire(S&&xrtTlsClientResumeCapacity(S)==0&&xrtGetError(),"server role rejected checked");xrtClearError();xrtTlsSessionDestroy(S);testRequire(xrtTlsClientResumeCapacity(NULL)==0&&xrtGetError(),"null query rejected checked");xrtClearError();xrtTlsIdentityRelease(Identity);xrtTlsVerifierRelease(Verify);xrtTlsContextRelease(Context);puts("[PASS] native TLS resume capacity0/1/4, configuration snapshots, ClientAuth0, role/null and zero allocation");return 0;
}
