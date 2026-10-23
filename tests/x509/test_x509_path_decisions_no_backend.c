#include "../test.h"
#include "../fixtures/x509_decision_vectors.h"
int main(void)
{
	const DecisionVector* V=&decisionVectors[3];xx509cert Root,Issuer,Target;xx509anchor Anchor;
	testRequire(xrtX509Parse(V->Root.Data,V->Root.Size,&Root) && xrtX509Anchor(&Root,&Anchor) &&
		xrtX509Parse(V->Issuer.Data,V->Issuer.Size,&Issuer) && xrtX509Parse(V->Target.Data,V->Target.Size,&Target),"normal no-backend fixtures");
	xx509pathconfig Config;memset(&Config,0,sizeof(Config));Config.Time=Target.NotBefore;
	const xx509cert* Path[2]={&Target,&Issuer};xx509pathdecision Decision;
	testRequire(xrtMemDebugFailAfter(0),"missing backend setup");
	testRequire(xrtX509PathCheck(Path,2,&Anchor,&Config,&Decision)==X509_CHECK_UNSUPPORTED &&
		Decision.Reason==X509_ERROR_SIGNATURE && Decision.Depth==0 && !xrtGetError() &&
		!xrtMemDebugFailTriggered(),"missing backend is not a checked operation failure");xrtMemDebugFailClear();
	testRequire(!xrtX509PathValidate(Path,2,&Anchor,&Config) && xrtErrorCode(xrtGetError())==X509_ERROR_SIGNATURE &&
		xrtErrorCause(xrtGetError()) && xrtErrorKind(xrtErrorCause(xrtGetError()))==XERR_UNSUPPORTED,"legacy backend cause");xrtClearError();
	testRequire(xrtMemDebugReset(),"no-backend cleanup");
	printf("[PASS] path decisions without crypto backend: explicit unsupported and legacy cause\n");return 0;
}
