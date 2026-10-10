#include "../test.h"
#include "typed_value_fixture.h"

typedef struct key_state {
    bool FailHash, FailEqual, Probe, Blocked;
    xtypedset* Target;
} key_state;

static void probe(key_state* state)
{
    if (state->Probe) {
        (void)xrtTypedSetCount(state->Target);
        state->Blocked = xrtErrorKind(xrtGetError()) == XERR_STATE;
        xrtClearError();
    }
}
static uint64 keyHash(const void* item, ptr opaque)
{
    (void)item; key_state* state = opaque; probe(state);
    if (state->FailHash) xrtSetErrorKind(XERR_RANGE);
    return 0; /* Legitimate zero hash, deliberate collision coverage. */
}
static bool keyEqual(const void* a, const void* b, ptr opaque)
{
    key_state* state = opaque; probe(state);
    if (state->FailEqual) xrtSetErrorKind(XERR_VALUE);
    return ((const testtypedvalue*)a)->Value == ((const testtypedvalue*)b)->Value;
}
static uint64 otherHash(const void* item, ptr opaque) { return keyHash(item, opaque); }
static xrttype keyType(xrttypeops* ops)
{
    xrttype type = testTypedValueType(); *ops = *type.Ops;
    ops->Compare = NULL; ops->Hash = NULL; type.Ops = ops;
    return type;
}
static void noError(void) { testRequire(!xrtGetError(), "unexpected key policy error"); }
static void errorKind(xerrkind kind)
{
    testRequire(xrtGetError() && xrtErrorKind(xrtGetError()) == kind, "key policy error kind mismatch");
    xrtClearError();
}
static void contracts(void)
{
    xrttypeops ops; xrttype type = keyType(&ops);
    key_state state = {0}, other = {0};
    const xtypedsetkeypolicy policy = {keyHash, keyEqual, &state};
    const xtypedsetkeypolicy same = {keyHash, keyEqual, &state};
    const xtypedsetkeypolicy different = {keyHash, keyEqual, &other};
    const xtypedsetkeypolicy differentHash = {otherHash, keyEqual, &state};
    const xtypedsetkeypolicy incomplete = {keyHash, NULL, &state};
    xtypedset left, right, invalid, mismatch; int owners = 3;
    testtypedvalue one = {1,&owners}, two = {2,&owners}, three = {3,&owners}, output = {0};
    testRequire(!xrtTypeIsComparable(&type) && !xrtTypeIsHashable(&type), "fixture invented ordering/hash");
    testRequire(!xrtTypedSetInit(&invalid,&type), "legacy init accepted absent typed keys"); errorKind(XERR_UNSUPPORTED);
    testRequire(!xrtTypedSetInitWithKeyPolicy(&invalid,&type,NULL), "null policy accepted"); errorKind(XERR_ARGUMENT);
    testRequire(!xrtTypedSetInitWithKeyPolicy(&invalid,&type,&incomplete), "partial policy accepted"); errorKind(XERR_ARGUMENT);
    union { xtypedset Set; xtypedsetkeypolicy Policy; } alias = {.Policy = policy};
    testRequire(!xrtTypedSetInitWithKeyPolicy(&alias.Set,&type,&alias.Policy), "aliased policy accepted"); errorKind(XERR_ARGUMENT);
    testRequire(alias.Policy.Hash == keyHash, "alias refusal destroyed policy");
    testRequire(xrtTypedSetInitWithKeyPolicy(&left,&type,&policy) &&
        xrtTypedSetInitWithKeyPolicy(&right,&type,&same), "explicit key policy init failed");
    state.Target = &left;
    testRequire(xrtTypedSetAdd(&left,&one) && xrtTypedSetAdd(&left,&two) &&
        xrtTypedSetAdd(&left,&one) && owners==5 && xrtTypedSetCount(&left)==2, "canonical owned uniqueness failed");
    const testtypedvalue* canonical = xrtTypedSetGet(&left,&one);
    testRequire(canonical && xrtTypedSetAdd(&left,canonical) && owners==5, "canonical source was recopied");
    state.Probe=true;
    testRequire(xrtTypedSetHas(&left,&one) && state.Blocked, "explicit key callback allowed reentry");
    state.Probe=false;
    xerror* prior=xrtErrorCreate(XERR_ARGUMENT,"test.prior",7,"prior"); testRequire(prior!=NULL,"prior failed");
    xrtSetError(prior); testRequire(xrtTypedSetHas(&left,&one) && xrtGetError()==prior,"successful zero hash lost prior error");
    xrtClearError(); xrtErrorFree(prior);
    state.FailHash=true;
    testRequire(!xrtTypedSetAdd(&left,&three) && xrtTypedSetCount(&left)==2 && owners==5,"hash failure committed value");
    errorKind(XERR_RANGE); state.FailHash=false;
    state.FailEqual=true;
    testRequire(!xrtTypedSetAdd(&left,&one) && xrtTypedSetCount(&left)==2 && owners==5,"equal failure treated as duplicate success");
    errorKind(XERR_VALUE); state.FailEqual=false;
    testTypedValueCopyFail(true);
    testRequire(!xrtTypedSetAdd(&left,&three) && owners==5 && xrtTypedSetCount(&left)==2,"copy failure committed value");
    testRequire(xrtGetError()!=NULL,"copy failure lost diagnostic"); xrtClearError(); testTypedValueCopyFail(false);
    testRequire(xrtTypedSetAdd(&right,&two) && xrtTypedSetAdd(&right,&three),"right source failed");
    xtypedset* copy=xrtTypedSetClone(&left);
    testRequire(copy && copy->KeyPolicy==&policy && xrtTypedSetEquals(copy,&left),"clone lost key capability");
    xtypedset* results[4]={xrtTypedSetUnion(&left,&right),xrtTypedSetIntersection(&left,&right),
        xrtTypedSetDifference(&left,&right),xrtTypedSetSymmetricDifference(&left,&right)};
    const size_t counts[4]={3,1,1,2};
    for (size_t i=0;i<4;i++) {
        testRequire(results[i] && results[i]->KeyPolicy==&policy && xrtTypedSetCount(results[i])==counts[i],"algebra lost policy/results");
        xrtTypedSetDestroy(results[i]);
    }
    testRequire(xrtTypedSetIsSubset(copy,&left,false) && xrtTypedSetIsSuperset(&left,copy,false) &&
        !xrtTypedSetIsDisjoint(&left,&right) && xrtTypedSetMerge(&left,&right) && xrtTypedSetCount(&left)==3,"same-key relations/merge failed");
    testRequire(xrtTypedSetTake(&left,&one,&output) && output.Value==1 && output.Owners==&owners,
        "owned take failed");
    xtypedsetiter iterator={0}; size_t visited=0;
    testRequire(xrtTypedSetIterBegin(&left,&iterator),"iteration failed");
    while (xrtTypedSetIterNext(&iterator)) ++visited;
    xrtTypedSetIterEnd(&iterator); testRequire(visited==2,"iteration count mismatch");
    for (size_t lane=0;lane<2;lane++) {
        testRequire(xrtTypedSetInitWithKeyPolicy(&mismatch,&type,lane ? &differentHash : &different),"mismatch source init failed");
        testRequire(!xrtTypedSetMerge(&left,&mismatch),"mismatched key policy merged"); errorKind(XERR_TYPE);
        testRequire(!xrtTypedSetUnion(&left,&mismatch),"mismatched key policy algebra accepted"); errorKind(XERR_TYPE);
        testRequire(!xrtTypedSetEquals(&left,&mismatch),"mismatched key policy equality accepted"); errorKind(XERR_TYPE);
        xrtTypedSetUnit(&mismatch);
    }
    xsethash saved=left.Storage.Hash; left.Storage.Hash=otherHash;
    testRequire(xrtTypedSetCount(&left)==0,"raw policy tampering accepted"); errorKind(XERR_STATE); left.Storage.Hash=saved;
    testRequire(!xrtTypeIsComparable(&type) && !xrtTypeIsHashable(&type),"set changed type capabilities");
    xrtTypedSetDestroy(copy); xrtTypedSetUnit(&right); xrtTypedSetUnit(&left);
    testRequire(owners==4,"container retirement leaked copies");
    xrtTypeDropValue(&type,&output); xrtTypeDropValue(&type,&one);
    xrtTypeDropValue(&type,&two); xrtTypeDropValue(&type,&three);
    testRequire(owners==0,"owned key fixture leaked"); noError();
}

static void prefixes(void)
{
    xmemdebugsnapshot persistent={0}; xrtMemDebugSnapshot(&persistent);
    xrttypeops ops; xrttype type=keyType(&ops); key_state state={0};
    const xtypedsetkeypolicy policy={keyHash,keyEqual,&state}; size_t attempts=0;
    for (unsigned lane=0;lane<7;lane++) {
        bool complete=false;
        for (size_t prefix=0;prefix<1024;prefix++) {
            int owners=3; testtypedvalue one={1,&owners},two={2,&owners},three={3,&owners}; xtypedset left,right;
            testRequire(xrtTypedSetInitWithKeyPolicy(&left,&type,&policy) &&
                xrtTypedSetInitWithKeyPolicy(&right,&type,&policy) && xrtTypedSetAdd(&left,&one) &&
                xrtTypedSetAdd(&left,&two) && xrtTypedSetAdd(&right,&three),"prefix source fixture failed");
            xmemdebugsnapshot baseline={0}; xrtMemDebugSnapshot(&baseline);
            int ownerBaseline=owners; xtypedset* result=NULL; bool success=false;
            testRequire(xrtMemDebugFailAfter(prefix),"failure point could not be armed");
            switch (lane) {
            case 0: result=xrtTypedSetCreateWithKeyPolicy(&type,&policy); break;
            case 1: result=xrtTypedSetClone(&left); break;
            case 2: result=xrtTypedSetUnion(&left,&right); break;
            case 3: result=xrtTypedSetIntersection(&left,&right); break;
            case 4: result=xrtTypedSetDifference(&left,&right); break;
            case 5: result=xrtTypedSetSymmetricDifference(&left,&right); break;
            default: success=xrtTypedSetMerge(&left,&right); break;
            }
            bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); ++attempts;
            if (lane!=6) success=result!=NULL;
            xrtClearError();
            xmemdebugsnapshot after={0}; xrtMemDebugSnapshot(&after);
            if (hit) testRequire(!success && owners==ownerBaseline && after.LiveCount==baseline.LiveCount &&
                after.LiveBytes==baseline.LiveBytes && xrtTypedSetCount(&left)==2 &&
                xrtTypedSetCount(&right)==1,"failed allocation published or leaked");
            else testRequire(success,"first no-hit attempt failed");
            if (result) { testRequire(result->KeyPolicy==&policy,"prefix result lost policy"); xrtTypedSetDestroy(result); }
            xrtTypedSetUnit(&right); xrtTypedSetUnit(&left);
            testRequire(owners==3,"prefix copied owner imbalance");
            xrtTypeDropValue(&type,&one); xrtTypeDropValue(&type,&two); xrtTypeDropValue(&type,&three); xrtClearError();
            xrtMemDebugSnapshot(&after);
            testRequire(!owners && after.LiveCount==persistent.LiveCount && after.LiveBytes==persistent.LiveBytes &&
                after.AllocCount-persistent.AllocCount==after.FreeCount-persistent.FreeCount &&
                after.DoubleFreeCount==persistent.DoubleFreeCount && after.InvalidFreeCount==persistent.InvalidFreeCount &&
                after.UseAfterFreeCount==persistent.UseAfterFreeCount,"prefix allocation ledger not closed");
            if (!hit) { complete=true; printf("key-policy lane=%u prefixes=%zu first-no-hit=%zu balanced\n",lane,prefix+1,prefix); break; }
        }
        testRequire(complete,"key policy allocation prefix not closed");
    }
    printf("key-policy attempts=%zu persistent-baseline=%zu\n",attempts,persistent.LiveCount);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"memory debug enable failed");
    contracts(); prefixes(); printf("[PASS] typed set explicit key policy\n"); return 0;
}
