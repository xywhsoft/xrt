/* Ordinary fixed certificates, deterministic work budgets and resource
 * lifecycle only. No changed DER/signatures, network or security campaigns. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

static unsigned Checks, AllocationFailures;

static void balanced(xmemdebugsnapshot before)
{
	xmemdebugsnapshot after;
	xrtMemDebugSnapshot(&after);
	testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
		after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
		before.InvalidFreeCount == after.InvalidFreeCount &&
		before.DoubleFreeCount == after.DoubleFreeCount &&
		before.UseAfterFreeCount == after.UseAfterFreeCount, "search work memory balance");
}

static void check(const xx509cert* target, const xx509pathsource* source,
	const xx509pathconfig* config, size_t limit, xx509check expected, size_t count)
{
	const xx509cert* path[32] = {NULL};
	xx509anchor marker = {0};
	xx509pathresult result = {73u, &marker};
	xx509pathdecision decision = {X509_ERROR_SIGNATURE, 7u, "unchanged"};
	xx509pathdecision previous = decision;
	xmemdebugsnapshot before;
	xrtMemDebugSnapshot(&before);
	xx509check status = xrtX509PathBuildCheckLimit(target, source, config,
		path, 32u, limit, &result, &decision);
	testRequire(status == expected, "exact search work boundary");
	if (status == X509_CHECK_VALID) {
		testRequire(result.Count == count && result.Anchor == source->Anchors &&
			path[0] == target && !decision.Reason && decision.Depth == SIZE_MAX &&
			!decision.Message && !xrtGetError(), "bounded successful publication");
	} else {
		testRequire(result.Count == 73u && result.Anchor == &marker,
			"unsuccessful search never publishes a result");
		if (status == X509_CHECK_ERROR) {
			testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE &&
				memcmp(&decision, &previous, sizeof(decision)) == 0,
				"budget exhaustion is checked Range, not a completed decision");
			xrtClearError();
		} else {
			testRequire(decision.Reason != 0 && decision.Message && !xrtGetError(),
				"completed rejection has a native decision, not an error");
		}
	}
	balanced(before);Checks++;
}

static void allocations(const xx509cert* target, const xx509pathsource* source,
	const xx509pathconfig* config, size_t steps, xx509check completed)
{
	bool finished = false;
	for (size_t budget = 0; budget < 256u; budget++) {
		const xx509cert* path[32] = {NULL};
		xx509anchor marker = {0};
		xx509pathresult result = {73u, &marker};
		xx509pathdecision decision = {X509_ERROR_SIGNATURE, 7u, "unchanged"};
		xx509pathdecision previous = decision;
		xmemdebugsnapshot before;
		xrtMemDebugSnapshot(&before);
		testRequire(xrtMemDebugFailAfter(budget), "arm search allocation failure");
		xx509check status = xrtX509PathBuildCheckLimit(target, source, config,
			path, 32u, steps, &result, &decision);
		bool hit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if (hit) {
			testRequire(status == X509_CHECK_ERROR && result.Count == 73u &&
				result.Anchor == &marker && memcmp(&decision, &previous, sizeof(decision)) == 0,
				"resource failure stops bounded search without publication");
			AllocationFailures++;
		} else {
			testRequire(status == completed, "allocation sweep reached deterministic completion");
			finished = true;
		}
		xrtClearError();balanced(before);
		if (finished) break;
	}
	testRequire(finished, "all bounded search allocation points enumerated");
}

int main(void)
{
	xmemdebugsnapshot before;
	xrtMemDebugSnapshot(&before);
	xx509cert leaf, mid, root;
	xx509anchor anchor;
	testRequire(xrtX509Parse(X509_PATH_LEAF, sizeof(X509_PATH_LEAF), &leaf) &&
		xrtX509Parse(X509_PATH_INTERMEDIATE, sizeof(X509_PATH_INTERMEDIATE), &mid) &&
		xrtX509Parse(X509_PATH_ROOT, sizeof(X509_PATH_ROOT), &root) &&
		xrtX509Anchor(&root, &anchor), "parse original normal RSA chain");
	xx509pathconfig config = {0};config.Time = leaf.NotBefore;
	const xx509cert* issuers[] = {&mid};
	xx509pathsource source = {issuers, 1u, &anchor, 1u};
	/* Leaf anchor miss + issuer visit + mid anchor match + two certificates. */
	for (size_t steps = 1u; steps <= 7u; steps++)
		check(&leaf, &source, &config, steps,
			steps < 5u ? X509_CHECK_ERROR : X509_CHECK_VALID, 2u);
	check(&leaf, &source, &config, 0, X509_CHECK_VALID, 2u);
	check(&leaf, &source, &config, SIZE_MAX, X509_CHECK_VALID, 2u);

	/* A direct issuer-to-anchor path costs one visit and one validation unit. */
	xx509pathsource direct = {NULL, 0, &anchor, 1u};
	check(&mid, &direct, &config, 1u, X509_CHECK_ERROR, 1u);
	check(&mid, &direct, &config, 2u, X509_CHECK_VALID, 1u);

	/* Duplicate/already-used and nonmatching candidates still consume work. */
	const xx509cert* duplicates[] = {&leaf, &root, &mid};
	xx509pathsource repeated = {duplicates, 3u, &anchor, 1u};
	check(&leaf, &repeated, &config, 6u, X509_CHECK_ERROR, 2u);
	check(&leaf, &repeated, &config, 7u, X509_CHECK_VALID, 2u);
	xx509anchor anchors[2];anchors[0] = anchor;anchors[1] = anchor;
	xx509pathsource repeatedAnchors = {issuers, 1u, anchors, 2u};
	check(&leaf, &repeatedAnchors, &config, 5u, X509_CHECK_ERROR, 2u);
	check(&leaf, &repeatedAnchors, &config, 6u, X509_CHECK_VALID, 2u);

	/* A normal date-policy rejection cannot escape a later exhausted search. */
	xx509pathconfig early = config;early.Time--;
	check(&leaf, &source, &early, 5u, X509_CHECK_ERROR, 0);
	check(&leaf, &source, &early, 6u, X509_CHECK_REJECTED, 0);
	xx509pathsource empty = {NULL, 0, NULL, 0};
	check(&leaf, &empty, &config, 1u, X509_CHECK_REJECTED, 0);

	/* The legacy typed/bool entries still use the same unlimited algorithm. */
	const xx509cert* path[2];xx509pathresult result;xx509pathdecision decision;
	testRequire(xrtX509PathBuildCheck(&leaf, &source, &config, path, 2u, &result, &decision) ==
		X509_CHECK_VALID && result.Count == 2u &&
		xrtX509PathBuild(&leaf, &source, &config, path, 2u, &result), "unlimited wrappers unchanged");
	allocations(&leaf, &source, &config, 1u, X509_CHECK_ERROR);
	allocations(&leaf, &source, &config, 5u, X509_CHECK_VALID);
	testRequire(AllocationFailures > 0, "real allocation failures reached");
	balanced(before);testRequire(xrtMemDebugReset(), "search work memory drain");
	printf("[PASS] x509_path_work: %u exact boundaries; %u allocation failures; balanced\n",
		Checks, AllocationFailures);
	return 0;
}
