#include "../test.h"

typedef struct keyitem { int Key; int* Resource; } keyitem;
typedef struct keystate {
	size_t Hashes, Equals, Copies, Drops, FailHash, FailEqual;
	int FailKey;
	bool DropError;
	xerror* Failure;
} keystate;

static void emitFailure(keystate* state)
{
	testRequire(state->Failure != NULL, "failure was not armed");
	xrtSetErrorTake(state->Failure); state->Failure = NULL;
}
static uint64 keyHash(const void* item, ptr data)
{
	keystate* state = data;
	++state->Hashes;
	if (state->Hashes == state->FailHash || ((const keyitem*)item)->Key == state->FailKey)
		emitFailure(state);
	return 0; /* Legitimate zero hash forces every lookup through collisions. */
}
static bool keyEqual(const void* left, const void* right, ptr data)
{
	keystate* state = data;
	if (++state->Equals == state->FailEqual) { emitFailure(state); return true; }
	return ((const keyitem*)left)->Key == ((const keyitem*)right)->Key;
}
static bool itemCopy(ptr target, const void* source, ptr data)
{
	keystate* state = data; keyitem* copy = target;
	copy->Resource = xrtMalloc(sizeof(int));
	if (!copy->Resource) return false;
	copy->Key = ((const keyitem*)source)->Key; *copy->Resource = copy->Key;
	++state->Copies; return true;
}
static void itemDrop(ptr item, ptr data)
{
	keystate* state = data;
	xrtFree(((keyitem*)item)->Resource); ((keyitem*)item)->Resource = NULL;
	++state->Drops;
	if (state->DropError) xrtSetErrorKind(XERR_ARGUMENT);
}
static void initSet(xset* set, keystate* state)
{
	testRequire(xrtSetInit(set, sizeof(keyitem)), "init");
	testRequire(xrtSetSetKeyPolicy(set,keyHash,keyEqual,state), "keys");
	testRequire(xrtSetSetLifecycle(set,itemCopy,itemDrop,state), "lifecycle");
}
static const xerror* arm(keystate* state)
{
	xrtClearError(); xrtSetErrorKind(XERR_VALUE);
	state->Failure = xrtTakeError(); return state->Failure;
}
static void failed(const xerror* original)
{
	testRequire(xrtGetError() == original, "callback error was lost or replaced");
	xrtClearError();
}
static void unchanged(const xset* set, const xset* before)
{
	testRequire(set->Count == before->Count && set->Version == before->Version &&
		set->Buckets == before->Buckets && set->First == before->First &&
		set->Last == before->Last && set->BucketCount == before->BucketCount,
		"failed callback changed visible storage/version");
}

int main(void)
{
	keystate state = {0}; xset left, right, empty, before;
	keyitem one = {1,NULL}, two = {2,NULL}, three = {3,NULL}, output = {91,NULL};
	const xerror* original; bool added = true;
	initSet(&left,&state); initSet(&right,&state); initSet(&empty,&state);
	testRequire(xrtSetAdd(&left,&one), "zero hash insertion");
	before = left; original = arm(&state); state.FailHash = state.Hashes + 1;
	testRequire(!xrtSetGetOrAdd(&left,&two,&added) && !added, "hash failure committed");
	failed(original); unchanged(&left,&before); state.FailHash = 0;

	/* A true callback payload with an error is NOT a matching key. */
	original = arm(&state); state.FailEqual = state.Equals + 1;
	testRequire(!xrtSetAdd(&left,&two), "equal failure became duplicate success");
	failed(original); unchanged(&left,&before);
	original = arm(&state); state.FailEqual = state.Equals + 1;
	testRequire(!xrtSetTake(&left,&one,&output) && output.Key == 91,
		"failed comparison consumed canonical item or changed output");
	failed(original); unchanged(&left,&before);
	original = arm(&state); state.FailEqual = state.Equals + 1;
	testRequire(!xrtSetRemove(&left,&one), "failed comparison removed item");
	failed(original); unchanged(&left,&before); state.FailEqual = 0;

	/* Successful/absent probes preserve an unrelated pre-existing error. */
	xrtSetErrorKind(XERR_STATE); original = xrtGetError();
	testRequire(xrtSetHas(&left,&one) && !xrtSetHas(&left,&two) &&
		xrtGetError() == original, "ambient error confused callback status");
	xrtClearError();

	/* Post-copy validation rollback must preserve the original error across Drop. */
	before = empty; state.DropError = true;
	original = arm(&state); state.FailHash = state.Hashes + 2;
	testRequire(!xrtSetAdd(&empty,&one), "post-copy hash failure committed");
	failed(original); unchanged(&empty,&before); state.FailHash = 0;
	original = arm(&state); state.FailEqual = state.Equals + 1;
	testRequire(!xrtSetAdd(&empty,&one), "post-copy equality failure committed");
	failed(original); unchanged(&empty,&before); state.FailEqual = 0;
	state.DropError = false;

	testRequire(xrtSetAdd(&right,&two) && xrtSetAdd(&right,&three), "merge source");
	before = left; state.DropError = true;
	original = arm(&state); state.FailKey = 3;
	testRequire(!xrtSetMerge(&left,&right), "late merge failure committed prefix");
	failed(original); unchanged(&left,&before); state.FailKey = 0;
	state.DropError = false;

	/* Query failures must not become ordinary false branches in algebra. */
	for (int operation = 0; operation < 8; ++operation) {
		xset* result = NULL; bool answer = false;
		original = arm(&state); state.FailHash = state.Hashes + 1;
		switch (operation) {
		case 0: result = xrtSetUnion(&left,&right); break;
		case 1: result = xrtSetIntersection(&left,&right); break;
		case 2: result = xrtSetDifference(&left,&right); break;
		case 3: result = xrtSetSymmetricDifference(&left,&right); break;
		case 4: answer = xrtSetIsSubset(&left,&right,false); break;
		case 5: answer = xrtSetIsSuperset(&right,&left,false); break;
		case 6: answer = xrtSetIsDisjoint(&left,&right); break;
		case 7: result = xrtSetClone(&left); break;
		}
		testRequire(!result && !answer, "failed callback produced algebra result");
		failed(original); unchanged(&left,&before); state.FailHash = 0;
	}
	/* Failure after an intersection result already owns an item. */
	testRequire(xrtSetAdd(&left,&two), "intersection source");
	state.DropError = true; original = arm(&state); state.FailKey = 2;
	testRequire(!xrtSetIntersection(&left,&left), "late filter failure returned prefix");
	failed(original); state.FailKey = 0; state.DropError = false;
	testRequire(xrtSetHas(&left,&one) && xrtSetHas(&left,&two), "recovery");
	xrtSetUnit(&empty); xrtSetUnit(&right); xrtSetUnit(&left);
	testRequire(state.Copies == state.Drops && !state.Failure && !xrtGetError(),
		"callback rollback leaked an owned item/error");
	printf("[PASS] set callback error boundaries\n"); return 0;
}
