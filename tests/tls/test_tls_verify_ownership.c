/* Ordinary immutable valid chain + native graph/lifecycle contracts only.
 * No socket, handshake, malformed crypto, trust bypass or security campaign. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

typedef struct owned_observer {
	unsigned Clocks, Verifies, Policies, Drops;
	bool ReleaseOriginal, FailDrop;
	xatomic32 Park, Entered, Continue;
} owned_observer;
typedef struct owned_context {
	volatile int32 Refs;
	owned_observer* Observer; /* borrowed; observer outlives this context */
	xtlsverifier* Verifier; /* borrowed for call observation only */
	xtlsverifier* Back; /* an actual retained strong slot */
	xtime Time;
} owned_context;
static const xtlsverifiercontextownershipv1 Ownership;

static bool contextCount(const void* data, size_t* count)
{
	const owned_context* self = data;
	if (!self || !count || self->Refs <= 0) return false;
	*count = (size_t)self->Refs; return true;
}
static bool contextTrace(const void* data, xrtownershipvisitor visit, ptr context)
{
	const owned_context* self = data;
	return self && visit && (!self->Back || visit(xrtTlsVerifierOwnership(self->Back), context));
}
static const xrtownershipops ContextOps = {contextCount, contextTrace};
static void contextDrop(ptr data)
{
	owned_context* self = data; owned_observer* observer = self->Observer;
	xrtownershipscope mutation = {0};
	testRequire(xrtOwnershipMutationBegin(&mutation), "context drop mutation");
	testRequire(self->Refs == 1, "exact transferred context reference");
	self->Refs = 0; xtlsverifier* back = self->Back; self->Back = NULL;
	testRequire(xrtOwnershipScopeEnd(&mutation), "context drop scope end");
	if (observer->ReleaseOriginal) {
		xrtownershipscope freeze = {0};
		testRequire(xrtOwnershipFreezeTryBegin(&freeze), "last-owner cleanup observation");
		testRequire(xrtTlsVerifierOwnershipQuiescenceV1Get()->Busy(self->Verifier),
			"last-owner context cleanup remains busy until final free");
		testRequire(xrtOwnershipScopeEnd(&freeze), "last-owner cleanup observation end");
	}
	observer->Drops++;
	xrtTlsVerifierRelease(back); xrtFree(self);
	if (observer->FailDrop) xrtSetErrorKind(XERR_IO);
}
static owned_context* contextNew(owned_observer* observer, xtime at)
{
	owned_context* self = xrtCalloc(1, sizeof(*self));
	testRequire(self != NULL, "context allocation");
	self->Refs = 1; self->Observer = observer; self->Time = at;
	return self;
}
static void active(owned_context* self)
{
	xrtownershipscope freeze = {0}; size_t count = 731;
	testRequire(xrtOwnershipFreezeTryBegin(&freeze), "callback temporary read freeze");
	xrtownershipref ref = xrtTlsVerifierOwnership(self->Verifier);
	const xrtownershippreparationv1* output = (const xrtownershippreparationv1*)self;
	const xtlsverifiercontextownershipv1* policies[] = {&Ownership};
	testRequire(xrtTlsVerifierOwnershipQuiescenceV1Get()->Busy(ref.Data) &&
		!ref.Ops->Count(ref.Data, &count) && count == 731 &&
		!xrtTlsVerifierOwnershipAdapterV1(ref, policies, 1, &output) &&
		output == (const xrtownershippreparationv1*)self, "active callback cannot be admitted");
	testRequire(xrtOwnershipScopeEnd(&freeze), "callback freeze end");
}
static xtime clockValue(ptr data)
{
	owned_context* self = data; owned_observer* observer = self->Observer;
	active(self); observer->Clocks++;
	if (xrtAtomic32Load(&observer->Park, XMEMORY_ACQUIRE)) {
		xrtAtomic32Store(&observer->Entered, 1, XMEMORY_RELEASE);
		while (!xrtAtomic32Load(&observer->Continue, XMEMORY_ACQUIRE)) xrtThreadYield();
	}
	return self->Time;
}
static xtlsverifydecision verifyDefault(const xtlspeer* peer, ptr data)
{
	owned_context* self = data; active(self); self->Observer->Verifies++;
	testRequire(peer && peer->CertificateCount == 2, "complete peer before default verification");
	return XTLS_VERIFY_DEFAULT;
}
static bool verified(const xtlsverifiedpeer* peer, ptr data)
{
	owned_context* self = data; active(self); self->Observer->Policies++;
	testRequire(peer && peer->Peer && peer->PathCount == 2 && peer->Anchor,
		"real complete path/anchor after default verification");
	if (self->Observer->ReleaseOriginal) {
		xrtTlsVerifierRelease(self->Verifier);
		testRequire(self->Observer->Drops == 0, "active call owns actual verifier reference");
	}
	return true;
}
static const xtlsverifiercontextownershipv1 Ownership = {
	sizeof(Ownership), verifyDefault, verified, clockValue, contextDrop, &ContextOps
};
static xtlsverifierconfig config(const xx509store* store, owned_context* context)
{
	xtlsverifierconfig result; xrtTlsVerifierConfigInit(&result);
	result.Store = store; result.Context = context; result.Verify = verifyDefault;
	result.Policy = verified; result.Time = clockValue; result.Release = contextDrop;
	return result;
}
static bool runVerify(xtlsverifier* verifier, const xx509cert* chain)
{
	bool ok = xrtTlsVerifierVerify(verifier, XTLS_SERVER, XRT_STR_LITERAL("sha1-root.test.xrt"), chain, 2);
	if (!ok && xrtGetError()) fprintf(stderr, "verification diagnostic: %s: %s\n",
		xrtErrorOperation(xrtGetError()), xrtErrorMessage(xrtGetError()));
	return ok;
}
typedef struct verify_job { xtlsverifier* Verifier; const xx509cert* Chain; } verify_job;
static int32 verifyThread(ptr data)
{
	const verify_job* job = data;
	return runVerify(job->Verifier, job->Chain) && !xrtGetError() ? 0 : 1;
}
static bool edge(xrtownershipref ref, ptr data)
{
	const owned_context* self = data;
	return ref.Data == self && ref.Ops == &ContextOps;
}
static void balanced(xmemdebugsnapshot before)
{
	xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount, "exact native ownership memory balance");
}
int main(void)
{
	xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline);
	xx509cert chain[2]; xx509store* store = xrtX509StoreCreate();
	testRequire(store && xrtX509Parse(X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &chain[0]) &&
		xrtX509Parse(X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &chain[1]) &&
		xrtX509StoreAdd(store, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)) == X509_VALUE,
		"normal fixed certificate chain and independent anchor");
	xtlsverifylimits limits; xrtTlsVerifyLimitsInit(&limits);
	const xtlsverifiercontextownershipv1* policies[] = {&Ownership};
	/* A legacy callback context is not implicitly admitted as an owned node. */
	owned_observer legacyObserver = {0};
	owned_context* legacyContext = contextNew(&legacyObserver, chain[0].NotBefore);
	xtlsverifierconfig legacyConfig = config(store, legacyContext);
	for (unsigned invalid = 0; invalid < 5; ++invalid) {
		xtlsverifierconfig settings = legacyConfig;
		xtlsverifiercontextownershipv1 policy = Ownership;
		if (invalid == 0) settings.Release = NULL;
		if (invalid == 1) settings.Context = NULL;
		if (invalid == 2) policy.Ops = NULL;
		if (invalid == 3) policy.size--;
		testRequire(!xrtTlsVerifierCreateOwnedV1(&settings, invalid == 4 ? NULL : &limits, &policy) &&
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT && !legacyObserver.Drops && legacyContext->Refs == 1,
			"invalid native descriptor does not adopt or call context");
		xrtClearError();
	}
	xtlsverifier* legacy = xrtTlsVerifierCreateLimited(&legacyConfig, &limits);
	testRequire(legacy != NULL, "unchanged legacy creation");
	xrtownershipscope legacyFreeze = {0}; size_t legacyCount = 911;
	const xrtownershippreparationv1* legacyPreparation = NULL;
	testRequire(xrtOwnershipFreezeTryBegin(&legacyFreeze), "legacy inspection freeze");
	xrtownershipref legacyRef = xrtTlsVerifierOwnership(legacy);
	testRequire(!legacyRef.Ops->Count(legacyRef.Data, &legacyCount) && legacyCount == 911 &&
		!xrtTlsVerifierOwnershipAdapterV1(legacyRef, policies, 1, &legacyPreparation) && !legacyPreparation,
		"opaque legacy context is not inferred from matching callbacks");
	testRequire(xrtOwnershipScopeEnd(&legacyFreeze), "legacy inspection end");
	xrtTlsVerifierRelease(legacy); testRequire(legacyObserver.Drops == 1, "legacy adoption unchanged");
	unsigned allocationFailures = 0; bool complete = false;
	for (size_t budget = 0; budget < 256 && !complete; ++budget) {
		xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
		owned_observer observer = {0}; owned_context* context = contextNew(&observer, chain[0].NotBefore);
		xtlsverifierconfig settings = config(store, context);
		testRequire(xrtMemDebugFailAfter(budget), "arm creation allocation prefix");
		xtlsverifier* verifier = xrtTlsVerifierCreateOwnedV1(&settings, &limits, &Ownership);
		bool failed = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (failed) {
			testRequire(!verifier && !observer.Drops && context->Refs == 1, "failure does not adopt context");
			allocationFailures++; xrtClearError(); contextDrop(context);
		} else {
			testRequire(verifier != NULL && !observer.Drops, "complete owned creation");
			context->Verifier = verifier;
			testRequire(runVerify(verifier, chain), "unchanged default path and identity verification");
			xrtTlsVerifierRelease(verifier); complete = true;
		}
		testRequire(observer.Drops == 1, "one actual context drop"); xrtClearError(); balanced(before);
	}
	testRequire(complete && allocationFailures, "creation rollback reaches first complete allocation prefix");
	for (unsigned mode = 0; mode < 3; ++mode) {
		owned_observer observer = {0}; observer.FailDrop = mode == 2;
		owned_context* context = contextNew(&observer, chain[0].NotBefore);
		xtlsverifierconfig settings = config(store, context);
		xtlsverifier* verifier = xrtTlsVerifierCreateOwnedV1(&settings, &limits, &Ownership);
		testRequire(verifier != NULL, "owned verifier for retirement"); context->Verifier = verifier;
		xrtownershipscope mutation = {0};
		testRequire(xrtOwnershipMutationBegin(&mutation), "cycle publication mutation");
		context->Back = xrtTlsVerifierRetain(verifier);
		testRequire(context->Back && xrtOwnershipScopeEnd(&mutation), "actual context back reference");
		xrtownershipref ref = xrtTlsVerifierOwnership(verifier);
		const xrtownershippreparationv1* preparation = NULL;
		const xrtownershipadapterv1* adapter;
		xrtownershipscope freeze = {0}; size_t count = 0;
		testRequire(xrtOwnershipFreezeTryBegin(&freeze), "retirement graph freeze");
		testRequire(ref.Ops->Count(ref.Data, &count) && count == 2 && ref.Ops->Trace(ref.Data, edge, context), "exact physical graph");
		xtlsverifiercontextownershipv1 copy = Ownership;
		const xtlsverifiercontextownershipv1* unknown[] = {&copy};
		testRequire(!xrtTlsVerifierOwnershipAdapterV1(ref, unknown, 1, &preparation) && !preparation,
			"equivalent descriptor is not policy identity");
		adapter = xrtTlsVerifierOwnershipAdapterV1(ref, policies, 1, &preparation);
		testRequire(adapter && preparation && preparation->Adapter == adapter && preparation->Ready(verifier), "exact admitted policy");
		testRequire(adapter->Hold(verifier) && adapter->Claim(verifier, &observer), "physical collector pin and claim");
		testRequire(!adapter->Claim(verifier, context), "different token cannot steal claim");
		testRequire(xrtOwnershipScopeEnd(&freeze), "claimed graph freeze end");
		testRequire(!runVerify(verifier, chain) && xrtErrorKind(xrtGetError()) == XERR_STATE && !observer.Clocks,
			"claim quarantines new calls before clock"); xrtClearError();
		testRequire(preparation->Prepare(verifier, &observer) == XRT_OWNERSHIP_PREPARE_READY, "idle native preparation");
		if (mode == 0) {
			testRequire(xrtOwnershipFreezeTryBegin(&freeze), "restore freeze");
			adapter->Restore(verifier, &observer);
			testRequire(xrtOwnershipScopeEnd(&freeze), "restore freeze end");
			testRequire(runVerify(verifier, chain), "restored ordinary verification");
			testRequire(xrtOwnershipFreezeTryBegin(&freeze) && adapter->Claim(verifier, &observer), "reclaim after restore");
			testRequire(xrtOwnershipScopeEnd(&freeze), "reclaim freeze end");
		}
		xrtTlsVerifierRelease(verifier); /* only the cycle and actual collector pin remain */
		testRequire(xrtOwnershipFreezeTryBegin(&freeze), "clear graph freeze");
		xrtownershipresult graph = {0};
		testRequire(xrtOwnershipInspect(&ref, 1, NULL, 0, &graph) && graph.NodeCount == 2 &&
			graph.EdgeCount == 2 && graph.ExternalRootCount == 1 && graph.ReachableAnchorCount == 1,
			"both physical cycle edges; the real collector pin is the only root");
		adapter->Clear(verifier, &observer);
		testRequire(observer.Drops == 0 && xrtOwnershipScopeEnd(&freeze), "Clear runs no destructor");
		for (unsigned retry = 0; retry < 2; ++retry) {
			testRequire(adapter->Finish(verifier, &observer) == !observer.FailDrop && observer.Drops == 1,
				"checked at-most-once finish including error retry");
			if (observer.FailDrop) testRequire(xrtErrorKind(xrtGetError()) == XERR_IO, "cleanup cause retained");
			xrtClearError();
		}
		adapter->Drop(verifier);
	}
	/* Native execution, not a fabricated active flag: park the real clock. */
	owned_observer observer = {0}; xrtAtomic32Init(&observer.Park, 1);
	xrtAtomic32Init(&observer.Entered, 0); xrtAtomic32Init(&observer.Continue, 0);
	owned_context* context = contextNew(&observer, chain[0].NotBefore);
	xtlsverifierconfig settings = config(store, context);
	xtlsverifier* verifier = xrtTlsVerifierCreateOwnedV1(&settings, &limits, &Ownership);
	testRequire(verifier != NULL, "parked native verifier"); context->Verifier = verifier;
	verify_job job = {verifier, chain}; xthread* thread = xrtThreadCreate(verifyThread, &job, 0);
	testRequire(thread != NULL, "actual verifying native thread");
	while (!xrtAtomic32Load(&observer.Entered, XMEMORY_ACQUIRE)) xrtThreadYield();
	xrtownershipscope freeze = {0}; const xrtownershippreparationv1* preparation = NULL;
	testRequire(xrtOwnershipFreezeTryBegin(&freeze), "parked call graph read");
	testRequire(xrtTlsVerifierOwnershipQuiescenceV1Get()->Busy(verifier) &&
		!xrtTlsVerifierOwnershipAdapterV1(xrtTlsVerifierOwnership(verifier), policies, 1, &preparation) && !preparation,
		"actual active clock refuses retirement");
	testRequire(xrtOwnershipScopeEnd(&freeze), "parked read end");
	xrtAtomic32Store(&observer.Continue, 1, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitFor(thread, 10000) == XWAIT_OK && xrtThreadExitCode(thread) == 0, "native verification completes");
	xrtThreadDestroy(thread); xrtTlsVerifierRelease(verifier);
	testRequire(observer.Drops == 1, "parked native context released once");
	/* Dropping the original public owner INSIDE Policy cannot free active data. */
	memset(&observer, 0, sizeof(observer)); observer.ReleaseOriginal = true;
	context = contextNew(&observer, chain[0].NotBefore); settings = config(store, context);
	verifier = xrtTlsVerifierCreateOwnedV1(&settings, &limits, &Ownership);
	testRequire(verifier != NULL, "self-release verifier"); context->Verifier = verifier;
	testRequire(runVerify(verifier, chain) && observer.Drops == 1 && observer.Clocks == 1 &&
		observer.Verifies == 1 && observer.Policies == 1, "actual call pin spans all callbacks");
	xrtX509StoreFree(store); xrtClearError(); balanced(baseline);
	printf("TLS verifier ownership: %u allocation failures; exact context/cycle edges, active clock, original-owner release, claim/restore and checked cleanup retry; memory balanced\n", allocationFailures);
	return 0;
}
