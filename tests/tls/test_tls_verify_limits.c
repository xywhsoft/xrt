/* Ordinary, unchanged certificate chain and resource/lifecycle contracts only.
 * No network traffic, altered cryptography or original security test suites. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

static unsigned Checks, Failures;
typedef struct limits_state { xtime Time; unsigned Calls, Releases; } limits_state;

static xtime clockValue(ptr context) { return ((limits_state*)context)->Time; }
static void released(ptr context) { ((limits_state*)context)->Releases++; }
static xtlsverifydecision accepted(const xtlspeer* peer, ptr context)
{
	testRequire(peer->CertificateCount == 2u, "complete peer passed to callback");
	((limits_state*)context)->Calls++;
	return XTLS_VERIFY_ACCEPT;
}
static bool policy(const xtlsverifiedpeer* peer, ptr context)
{
	testRequire(peer->PathCount == 2u && peer->Anchor &&
		peer->Peer->CertificateCount == 2u, "complete bounded path publication");
	((limits_state*)context)->Calls++;
	return true;
}
static void balanced(xmemdebugsnapshot before)
{
	xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount &&
		before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount, "TLS limits exact memory balance");
}
static void verify(const xtlspeer* peer, const xx509store* store,
	const xtlsverifylimits* limits, bool expected, xerrkind kind)
{
	xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
	limits_state state = {0};
	bool result = xrtTlsPeerVerifyLimited(peer, store, limits, policy, &state);
	testRequire(result == expected && state.Calls == (expected ? 1u : 0u),
		"exact limit; no partial path reaches policy");
	if (!expected) {
		if (xrtErrorKind(xrtGetError()) != kind)
			fprintf(stderr, "boundary %u: kind %d, expected %d\n", Checks,
				(int)xrtErrorKind(xrtGetError()), (int)kind);
		testRequire(xrtErrorKind(xrtGetError()) == kind, "checked limit error kind");
	}
	else testRequire(!xrtGetError(), "successful bounded path has no error");
	xrtClearError(); balanced(before); Checks++;
}
int main(void)
{
	xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
	xx509cert chain[2]; xx509store* store = xrtX509StoreCreate();
	testRequire(store && xrtX509Parse(X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &chain[0]) &&
		xrtX509Parse(X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &chain[1]) &&
		xrtX509StoreAdd(store, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)) == X509_VALUE,
		"normal fixed TLS chain and independent trust anchor");
	xtlspeer peer = {XTLS_SERVER, XRT_STR_LITERAL("sha1-root.test.xrt"), chain[0].NotBefore, chain, 2u};
	xtlsverifylimits defaults; xrtTlsVerifyLimitsInit(&defaults);
	testRequire(defaults.MaxPeerCertificates == 64u && defaults.MaxPeerBytes == 16777216u &&
		defaults.MaxPathCertificates == 64u && defaults.MaxSearchSteps == 100000u &&
		defaults.MaxTrustCertificates == 4096u && defaults.MaxTrustBytes == 67108864u,
		"documented finite defaults");
	verify(&peer, store, &defaults, true, 0);
	xtlsverifylimits exact = {2u, sizeof(X509_PATH_LEAF) + sizeof(X509_PATH_INTERMEDIATE),
		2u, 5u, 1u, sizeof(X509_PATH_ROOT)};
	verify(&peer, store, &exact, true, 0);
	xtlsverifylimits limits = exact; limits.MaxPeerCertificates--;
	verify(&peer, store, &limits, false, XERR_RANGE);
	limits = exact; limits.MaxPeerBytes--;
	verify(&peer, store, &limits, false, XERR_RANGE);
	limits = exact; limits.MaxPathCertificates--;
	verify(&peer, store, &limits, false, XERR_RANGE);
	limits = exact; limits.MaxSearchSteps--;
	verify(&peer, store, &limits, false, XERR_RANGE);
	limits = exact; limits.MaxTrustBytes--;
	verify(&peer, store, &limits, false, XERR_RANGE);
	memset(&limits, 0, sizeof(limits));
	verify(&peer, store, &limits, true, 0);
	testRequire(xrtTlsPeerVerify(&peer, store, NULL, NULL), "legacy unlimited entry unchanged");
	/* A normal clock-policy rejection must not inherit a preceding error. */
	xtlspeer early = peer; early.Time--;
	(void)xrtTlsVerifierCreateLimited(NULL, NULL);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
		!xrtTlsPeerVerifyLimited(&early, store, &defaults, NULL, NULL) &&
		xrtErrorKind(xrtGetError()) == XERR_PROTOCOL && !xrtErrorCause(xrtGetError()),
		"completed policy rejection excludes unrelated previous error");
	xrtClearError(); Checks++;

	/* Resource failure never reaches the policy or publishes a partial path. */
	bool verified = false;
	for (size_t budget = 0; budget < 256u; budget++) {
		xmemdebugsnapshot allocationBefore; xrtMemDebugSnapshot(&allocationBefore);
		limits_state policyState = {0};
		testRequire(xrtMemDebugFailAfter(budget), "arm bounded verification failure");
		bool result = xrtTlsPeerVerifyLimited(&peer, store, &defaults, policy, &policyState);
		bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (hit) { testRequire(!result && !policyState.Calls, "failed path cannot call policy"); Failures++; }
		else { testRequire(result && policyState.Calls == 1u, "bounded path allocation sweep complete"); verified = true; }
		xrtClearError(); balanced(allocationBefore);
		if (verified) break;
	}
	testRequire(verified && Failures, "verification allocation rollback enumerated");

	/* Successful creation owns an independent store and a copy of the budgets. */
	limits_state state = {peer.Time, 0, 0};
	xtlsverifierconfig config; xrtTlsVerifierConfigInit(&config);
	config.Store = store; config.Time = clockValue; config.Context = &state; config.Release = released;
	limits = exact; limits.MaxTrustBytes--;
	testRequire(!xrtTlsVerifierCreateLimited(&config, &limits) && !state.Releases &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE, "trust bytes before cloning and context adoption");
	xrtClearError(); Checks++;
	limits = exact;
	xtlsverifier* verifier = xrtTlsVerifierCreateLimited(&config, &limits);
	testRequire(verifier && xrtTlsVerifierRetain(verifier) == verifier, "bounded shared verifier");
	limits.MaxPeerBytes = 1u;
	xrtX509StoreFree(store); store = NULL;
	testRequire(xrtTlsVerifierVerify(verifier, peer.Role, peer.Name, chain, 2u),
		"source store released; limits copied by value");
	xrtTlsVerifierRelease(verifier); testRequire(state.Releases == 0u, "retained context remains live");
	xrtTlsVerifierRelease(verifier); testRequire(state.Releases == 1u, "context released exactly once");

	/* The quota is enforced even when a custom callback accepts the peer. */
	state.Calls = state.Releases = 0; config.Store = NULL; config.Verify = accepted;
	limits = exact; limits.MaxPeerBytes--;
	verifier = xrtTlsVerifierCreateLimited(&config, &limits);
	testRequire(verifier && !xrtTlsVerifierVerify(verifier, peer.Role, peer.Name, chain, 2u) &&
		state.Calls == 0u && xrtErrorKind(xrtGetError()) == XERR_RANGE, "budget precedes callback");
	xrtClearError(); xrtTlsVerifierRelease(verifier);
	testRequire(state.Releases == 1u, "bounded custom context release");

	store = xrtX509StoreCreate();
	testRequire(store && xrtX509StoreAdd(store, X509_PATH_ROOT, sizeof(X509_PATH_ROOT)) == X509_VALUE &&
		xrtX509StoreAdd(store, X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE)) == X509_VALUE,
		"two ordinary trust certificates");
	state.Releases = 0; config.Store = store; config.Verify = NULL;
	testRequire(!xrtTlsVerifierCreateLimited(&config, &exact) && !state.Releases &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE, "trust count before cloning and context adoption");
	xrtClearError();
	limits = defaults;
	bool completed = false;
	for (size_t budget = 0; budget < 256u; budget++) {
		xmemdebugsnapshot allocationBefore; xrtMemDebugSnapshot(&allocationBefore);
		testRequire(xrtMemDebugFailAfter(budget), "arm verifier construction failure");
		verifier = xrtTlsVerifierCreateLimited(&config, &limits);
		bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if (hit) { testRequire(!verifier && !state.Releases, "failed clone does not adopt context"); Failures++; }
		else { testRequire(verifier != NULL, "all verifier allocations enumerated"); completed = true; }
		xrtTlsVerifierRelease(verifier); xrtClearError(); balanced(allocationBefore);
		if (completed) break;
	}
	testRequire(completed && Failures && state.Releases == 1u, "construction rollback and final ownership");
	state.Releases = 0; config.Store = NULL; config.Verify = accepted;
	testRequire(!xrtTlsVerifierCreateLimited(&config, NULL) && !state.Releases &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "limited create requires explicit limits");
	xrtClearError(); verify(&peer, store, NULL, false, XERR_ARGUMENT);
	xrtX509StoreFree(store); xrtClearError(); balanced(before);
	testRequire(xrtMemDebugReset(), "TLS limit memory drain");
	printf("[PASS] tls_verify_limits: %u exact boundaries; %u allocation failures; balanced\n", Checks, Failures);
	return 0;
}
