/* Normal certificate profiles and routine allocation/callback failures only. */
#include "../test.h"
#include "../fixtures/x509_decision_vectors.h"
#include "../../src/internal/xrt_x509.h"

typedef struct CriticalState { int Mode; size_t Calls; const xerror* Error; } CriticalState;
static xx509result critical(const xx509cert* Cert,const xx509ext* Ext,size_t Depth,ptr User)
{
	CriticalState* State=(CriticalState*)User;
	(void)Cert;(void)Ext;
	testRequire(Depth==1,"critical depth");State->Calls++;
	if(State->Mode==1){xrtSetError(State->Error);return X509_ERROR;}
	if(State->Mode==2){xrtClearError();return X509_ERROR;}
	return State->Mode==3?X509_DONE:X509_VALUE;
}
static void parse(const uint8* Data,size_t Size,xx509cert* Cert)
{testRequire(xrtX509Parse(Data,Size,Cert),"normal certificate parse");}
static xx509pathconfig policy(const xx509cert* Cert)
{
	xx509pathconfig Config;memset(&Config,0,sizeof(Config));
	Config.Time=Cert->NotBefore+(Cert->NotAfter-Cert->NotBefore)/2;return Config;
}
static void normalPaths(void)
{
	for(size_t i=0;i<sizeof(decisionVectors)/sizeof(*decisionVectors);i++){
		const DecisionVector* V=&decisionVectors[i];xx509cert Root,Issuer,Target;xx509anchor Anchor;
		parse(V->Root.Data,V->Root.Size,&Root);parse(V->Issuer.Data,V->Issuer.Size,&Issuer);
		parse(V->Target.Data,V->Target.Size,&Target);testRequire(xrtX509Anchor(&Root,&Anchor),"normal anchor");
		const xx509cert* Path[2]={&Target,&Issuer};xx509pathconfig Config=policy(&Target);xx509pathdecision Decision;
		testRequire(xrtMemDebugFailAfter(0),"normal zero allocation setup");
		testRequire(xrtX509PathCheck(Path,2,&Anchor,&Config,&Decision)==X509_CHECK_VALID &&
			Decision.Reason==0 && Decision.Depth==SIZE_MAX && Decision.Message==NULL,"normal typed path");
		testRequire(xrtX509PathValidate(Path,2,&Anchor,&Config),"normal legacy path");
		testRequire(!xrtGetError()&&!xrtMemDebugFailTriggered(),"valid path allocated diagnostics");xrtMemDebugFailClear();
		Config.Time=Target.NotBefore-1;
		testRequire(xrtMemDebugFailAfter(0),"time decision setup");
		testRequire(xrtX509PathCheck(Path,2,&Anchor,&Config,&Decision)==X509_CHECK_REJECTED &&
			Decision.Reason==X509_ERROR_TIME && Decision.Depth==0 && Decision.Message!=NULL,"precise time decision");
		testRequire(!xrtGetError()&&!xrtMemDebugFailTriggered(),"time rejection allocated diagnostics");xrtMemDebugFailClear();
		Config=policy(&Target);Config.KeyUsage=X509_USAGE_CERT_SIGN;
		testRequire(xrtMemDebugFailAfter(0),"usage decision setup");
		testRequire(xrtX509PathCheck(Path,2,&Anchor,&Config,&Decision)==X509_CHECK_REJECTED &&
			Decision.Reason==X509_ERROR_KEY_USAGE && Decision.Depth==0,"precise key usage decision");
		testRequire(!xrtGetError()&&!xrtMemDebugFailTriggered(),"usage rejection allocated diagnostics");xrtMemDebugFailClear();
		testRequire(!xrtX509PathValidate(Path,2,&Anchor,&Config) && xrtErrorCode(xrtGetError())==X509_ERROR_PURPOSE,
			"legacy usage category");xrtClearError();
	}
}
static void buildDecisions(void)
{
	xx509cert Root,Client,Server,Critical,Target,GoodNc,BadNc;xx509anchor Anchor;
#define PARSE(Field,Name) parse(decision_##Name,sizeof(decision_##Name),&Field)
	PARSE(Root,build_root);PARSE(Client,build_client);PARSE(Server,build_server);PARSE(Critical,build_critical);
	PARSE(Target,build_target);PARSE(GoodNc,build_good_nc);PARSE(BadNc,build_bad_nc);
	testRequire(xrtX509Anchor(&Root,&Anchor),"build anchor");
	const xx509cert* Candidates[3]={&Client,&Critical,&Server};const xx509cert* Path[17];
	xx509pathsource Source={Candidates,2,&Anchor,1};xx509pathresult Result;xx509pathdecision Decision;
	static const uint8 Purpose[]={43,6,1,5,5,7,3,1};
	xx509pathconfig Config=policy(&Target);Config.Flags=X509_PATH_REQUIRE_PURPOSE;
	Config.Purpose=(xbytesview){Purpose,sizeof(Purpose)};
	CriticalState State={0};Config.Critical=critical;Config.UserData=&State;
	testRequire(xrtMemDebugFailAfter(0),"alternative policy setup");
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_VALID &&
		Result.Count==2 && Path[1]==&Critical && State.Calls==1 && Decision.Reason==0,"alternative policy search");
	testRequire(xrtX509PathBuild(&Target,&Source,&Config,Path,8,&Result),"legacy alternative policy search");
	testRequire(!xrtGetError()&&!xrtMemDebugFailTriggered(),"ordinary candidate rejection allocated");xrtMemDebugFailClear();

	Source.IssuerCount=1;Result.Count=123;Result.Anchor=&Anchor;
	testRequire(xrtMemDebugFailAfter(0),"purpose rejection setup");
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_REJECTED &&
		Decision.Reason==X509_ERROR_PURPOSE && Decision.Depth==1 && Result.Count==123 && Result.Anchor==&Anchor,
		"completed rejection and result publication");
	testRequire(!xrtGetError()&&!xrtMemDebugFailTriggered(),"completed rejection allocated");xrtMemDebugFailClear();
	testRequire(!xrtX509PathBuild(&Target,&Source,&Config,Path,8,&Result) &&
		xrtErrorKind(xrtGetError())==XERR_NOT_FOUND && xrtErrorCause(xrtGetError()) &&
		xrtErrorCode(xrtErrorCause(xrtGetError()))==X509_ERROR_PURPOSE,"legacy candidate diagnostic");xrtClearError();

	Source.IssuerCount=3;State.Mode=3;State.Calls=0;
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_VALID &&
		Path[1]==&Server && State.Calls==1 && !xrtGetError(),"unhandled extension may backtrack");
	State.Error=xrtErrorCreate(XERR_MEMORY,"path-fixture",85,"owned callback resource failure");
	testRequire(State.Error!=NULL,"owned callback error");
	for(int Legacy=0;Legacy<2;Legacy++)for(int Mode=1;Mode<=2;Mode++){
		State.Mode=Mode;State.Calls=0;Result.Count=123;Result.Anchor=&Anchor;
		Decision.Reason=X509_ERROR_NOT_FOUND;Decision.Depth=999;Decision.Message="unpublished";
		testRequire(xrtMemDebugFailAfter(0),"callback failure setup");
		if(Legacy)testRequire(!xrtX509PathBuild(&Target,&Source,&Config,Path,8,&Result),"legacy callback failure");
		else testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_ERROR &&
			Decision.Reason==X509_ERROR_NOT_FOUND && Decision.Depth==999 &&
			!strcmp(Decision.Message,"unpublished"),"typed callback failure publication");
		testRequire(Result.Count==123 && Result.Anchor==&Anchor && State.Calls==1,"callback failure continued search");
		testRequire((Mode==1?xrtGetError()==State.Error:xrtGetError()==NULL) && !xrtMemDebugFailTriggered(),
			"callback error overwritten or materialized");xrtMemDebugFailClear();xrtClearError();
	}
	State.Mode=0;xrtSetError(State.Error);
	testRequire(xrtMemDebugFailAfter(0),"pending error search setup");
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_VALID &&
		xrtGetError()==State.Error && !xrtMemDebugFailTriggered(),"pending error changed search decision");
	xrtMemDebugFailClear();xrtClearError();xrtErrorFree((xerror*)State.Error);

	Result.Count=123;Result.Anchor=&Anchor;Decision.Reason=X509_ERROR_NOT_FOUND;Decision.Depth=999;
	testRequire(xrtMemDebugFailAfter(0),"frame allocation setup");
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,17,&Result,&Decision)==X509_CHECK_ERROR &&
		xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError())==XERR_MEMORY && Result.Count==123 &&
		Decision.Reason==X509_ERROR_NOT_FOUND && Decision.Depth==999,"frame allocation failure escaped");
	xrtMemDebugFailClear();xrtClearError();

	/* Normal trust-policy alternatives exercise the typed NameConstraints path. */
	xx509anchor Anchors[2];testRequire(xrtX509Anchor(&BadNc,&Anchors[0]) &&
		xrtX509Anchor(&GoodNc,&Anchors[1]),"name constraints anchors");
	Candidates[0]=&Server;Source.IssuerCount=1;Source.Anchors=Anchors;Source.AnchorCount=2;
	testRequire(xrtMemDebugFailAfter(0),"name constraints alternatives setup");
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_VALID &&
		Result.Anchor==&Anchors[1] && !xrtGetError() && !xrtMemDebugFailTriggered(),"name constraints alternatives");xrtMemDebugFailClear();
	Source.AnchorCount=1;
	testRequire(xrtMemDebugFailAfter(0),"name constraints rejection setup");
	testRequire(xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision)==X509_CHECK_REJECTED &&
		Decision.Reason==X509_ERROR_NAME_CONSTRAINTS && Decision.Depth==0 && !xrtGetError() &&
		!xrtMemDebugFailTriggered(),"name constraints rejection");xrtMemDebugFailClear();

	/* Ordinary null arguments are checked failures, not completed verdicts. */
	Decision.Reason=X509_ERROR_NOT_FOUND;Decision.Depth=999;
	testRequire(xrtX509PathCheck(NULL,0,&Anchor,&Config,&Decision)==X509_CHECK_ERROR &&
		Decision.Reason==X509_ERROR_NOT_FOUND && Decision.Depth==999,"invalid ordered input published verdict");xrtClearError();
#undef PARSE
}
static void nameAllocationFailures(void)
{
	xx509cert Root,Issuer,Target;xx509anchor Anchor;
	parse(decision_long_root,sizeof(decision_long_root),&Root);
	parse(decision_long_issuer,sizeof(decision_long_issuer),&Issuer);
	parse(decision_long_target,sizeof(decision_long_target),&Target);
	testRequire(xrtX509Anchor(&Root,&Anchor),"long-name anchor");
	const xx509cert* Candidates[]={&Issuer};const xx509cert* Path[8];
	xx509pathsource Source={Candidates,1,&Anchor,1};xx509pathconfig Config=policy(&Target);
	xx509pathresult Result;xx509pathdecision Decision;unsigned Failures=0;bool Completed=false;
	for(uint64 Budget=0;Budget<64;Budget++){
		Result.Count=123;Result.Anchor=&Anchor;Decision.Reason=X509_ERROR_NOT_FOUND;Decision.Depth=999;
		testRequire(xrtMemDebugFailAfter(Budget),"name allocation budget");
		xx509check Check=xrtX509PathBuildCheck(&Target,&Source,&Config,Path,8,&Result,&Decision);
		bool Triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
		if(Triggered){
			testRequire(Check==X509_CHECK_ERROR && xrtGetError()!=NULL && Result.Count==123 &&
				Decision.Reason==X509_ERROR_NOT_FOUND && Decision.Depth==999,"name allocation failure swallowed");Failures++;
		}else{testRequire(Check==X509_CHECK_VALID && Result.Count==2 && !xrtGetError(),"long-name baseline");Completed=true;}
		xrtClearError();if(Completed)break;
	}
	testRequire(Completed && Failures>0,"name allocations were not exercised");
	printf("[PASS] name allocation budgets: failures=%u, first_success=%u\n",Failures,Failures);
}
static void legacyCauseDescriptor(void)
{
	/* Routine structured-error metadata, not a malformed key/signature test. */
	xrt_x509_decision Decision=__xrtX509Decision(X509_CHECK_REJECTED,X509_ERROR_SIGNATURE,"normal descriptor test");
	Decision.CryptoCode=XCRYPTO_ERROR_KEY;Decision.CryptoKind=XERR_VALUE;
	testRequire(__xrtX509DecisionRaise(&Decision,"x509-path") && xrtErrorCause(xrtGetError()) &&
		xrtErrorKind(xrtErrorCause(xrtGetError()))==XERR_VALUE &&
		xrtErrorCode(xrtErrorCause(xrtGetError()))==XCRYPTO_ERROR_KEY,"legacy key cause category");
	xrtClearError();
	Decision.CryptoCode=XCRYPTO_ERROR_SIGNATURE;Decision.CryptoKind=XERR_PROTOCOL;
	testRequire(__xrtX509DecisionRaise(&Decision,"x509-path") && xrtErrorCause(xrtGetError()) &&
		xrtErrorKind(xrtErrorCause(xrtGetError()))==XERR_PROTOCOL &&
		xrtErrorCode(xrtErrorCause(xrtGetError()))==XCRYPTO_ERROR_SIGNATURE,"legacy signature cause category");
	xrtClearError();
}
int main(void)
{
	xmemdebugsnapshot Before,After;xrtMemDebugSnapshot(&Before);
	normalPaths();buildDecisions();nameAllocationFailures();legacyCauseDescriptor();
	testRequire(xrtMemDebugReset(),"decision cleanup");xrtMemDebugSnapshot(&After);
	testRequire(After.LiveCount==Before.LiveCount,"decision resource balance");
	printf("[PASS] path decisions: normal profiles, callback errors, precise reasons, failure publication\n");return 0;
}
