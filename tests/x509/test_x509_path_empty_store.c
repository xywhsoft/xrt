/* Ordinary valid empty trust sources and checked ABI boundaries only.
 * No cryptographic mutations, signature tests or network traffic. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

static void balanced(xmemdebugsnapshot before)
{
	xmemdebugsnapshot after;
	xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount &&
		before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount, "empty-source balance");
}

static void rejected(const xx509cert* target, const xx509pathsource* source, size_t capacity)
{
	const xx509cert* path[32] = {NULL};
	xx509anchor marker = {0};
	xx509pathresult result = {73u, &marker};
	xx509pathdecision decision = {0};
	xx509pathconfig config;
	xrtX509PathConfigInit(&config);
	xmemdebugsnapshot before;
	xrtMemDebugSnapshot(&before);
	testRequire(xrtMemDebugFailAfter(0), "arm empty-source zero-allocation check");
	xx509check status = xrtX509PathBuildCheck(target, source, &config,
		path, capacity, &result, &decision);
	bool allocated = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	testRequire(status == X509_CHECK_REJECTED && decision.Reason == X509_ERROR_PATH_BUILD &&
		decision.Depth == SIZE_MAX && decision.Message != NULL && !allocated && !xrtGetError(),
		"empty anchors are a completed nonallocating rejection");
	testRequire(result.Count == 73u && result.Anchor == &marker && path[0] == NULL,
		"empty source publishes no path or successful result");
	balanced(before);
}

int main(void)
{
	xmemdebugsnapshot before;
	xrtMemDebugSnapshot(&before);
	xx509cert leaf, issuer;
	testRequire(xrtX509Parse(X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &leaf) &&
		xrtX509Parse(X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &issuer),
		"parse ordinary fixed certificates");
	xx509store* store = xrtX509StoreCreate();
	testRequire(store != NULL, "empty store");
	xx509pathsource source = {0};
	testRequire(xrtX509StoreSource(store, NULL, 0, &source) &&
		source.IssuerCount == 0 && source.AnchorCount == 0 && !xrtGetError(),
		"empty store source is valid");
	rejected(&leaf, &source, 1u);
	rejected(&leaf, &source, 32u);
	const xx509cert* candidates[] = {&issuer};
	testRequire(xrtX509StoreSource(store, candidates, 1u, &source) &&
		source.Issuers == candidates && source.IssuerCount == 1u && source.AnchorCount == 0,
		"empty anchors with ordinary issuer candidate");
	rejected(&leaf, &source, 32u);
	xx509pathsource direct = {0};
	rejected(&leaf, &direct, 1u);

	xx509pathsource previous = source;
	testRequire(!xrtX509StoreSource(store, NULL, 1u, &source) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
		memcmp(&source, &previous, sizeof(source)) == 0, "invalid source leaves output unchanged");
	xrtClearError();
	xx509pathconfig config;
	xrtX509PathConfigInit(&config);
	const xx509cert* path[1] = {NULL};
	xx509anchor marker = {0};
	xx509pathresult result = {73u, &marker};
	xx509pathdecision decision = {X509_ERROR_SIGNATURE, 7u, "unchanged"};
	xx509pathdecision oldDecision = decision;
	direct.AnchorCount = 1u;
	testRequire(xrtX509PathBuildCheck(&leaf, &direct, &config, path, 1u, &result, &decision) ==
		X509_CHECK_ERROR && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
		memcmp(&decision, &oldDecision, sizeof(decision)) == 0 && result.Count == 73u &&
		result.Anchor == &marker, "nonnull-count pointer contract still checked");
	xrtClearError();
	direct.AnchorCount = 0;
	testRequire(!xrtX509PathBuild(&leaf, &direct, &config, path, 1u, &result) &&
		xrtGetError() != NULL && result.Count == 73u && result.Anchor == &marker,
		"legacy bool still diagnoses a completed rejection");
	xrtClearError();
	xrtX509StoreFree(store);
	balanced(before);
	testRequire(xrtMemDebugReset(), "empty-source memory drain");
	puts("[PASS] x509_path_empty_store: 4 completed rejections; zero allocations; outputs preserved");
	return 0;
}
