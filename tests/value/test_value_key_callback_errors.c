#include "../test.h"

typedef struct valuekeystate {
	size_t Hashes, Equals, FailHash, FailEqual, Drops;
	bool DropError;
	xerror* Failure;
} valuekeystate;
static void failKey(valuekeystate* state)
{
	testRequire(state->Failure != NULL,"unarmed failure");
	xrtSetErrorTake(state->Failure); state->Failure = NULL;
}
static uint64 handleHash(ptr raw,ptr data)
{
	valuekeystate* state=data; (void)raw;
	if (++state->Hashes == state->FailHash) failKey(state);
	return 0;
}
static bool handleEqual(ptr left,ptr right,ptr data)
{
	valuekeystate* state=data;
	if (++state->Equals == state->FailEqual) {failKey(state);return true;}
	return *(int*)left == *(int*)right;
}
static void handleDrop(ptr raw,ptr data)
{
	valuekeystate* state=data; xrtFree(raw); ++state->Drops;
	if(state->DropError)xrtSetErrorKind(XERR_ARGUMENT);
}
static const xvaluehandleops keyOps={NULL,handleDrop,handleHash,handleEqual};
static xvalue* makeKey(int number,valuekeystate* state)
{
	ptr raw=xrtMalloc(sizeof(int)); xvalue* value;
	testRequire(raw != NULL,"key allocation"); *(int*)raw=number;
	value=xrtValueHandleTake(&raw,&keyOps,state);
	testRequire(value != NULL && raw == NULL,"key publication"); return value;
}
static uint64 objectHash(const xvalue* value,ptr data)
{ return handleHash((ptr)value,data); }
static bool objectEqual(const xvalue* left,const xvalue* right,ptr data)
{
	valuekeystate* state=data; (void)left;(void)right;
	if (++state->Equals == state->FailEqual) failKey(state);
	return true;
}
static const xerror* arm(valuekeystate* state)
{
	xrtClearError(); xrtSetErrorKind(XERR_VALUE);
	state->Failure=xrtTakeError(); return state->Failure;
}
static void failed(const xerror* original)
{ testRequire(xrtGetError()==original,"original callback failure was lost");xrtClearError(); }

/* A true payload with an error at the first child must stop the graph, rather
 * than invoking a later child callback which can clear/replace that error. */
static void nestedEqualFailure(xvalue* first,xvalue* same,valuekeystate* state)
{
	for ( int kind = 0; kind < 3; ++kind ) {
		xvalue* left = kind == 0 ? xrtValueArray() :
			(kind == 1 ? xrtValueIntMap() : xrtValueObject());
		xvalue* right = kind == 0 ? xrtValueArray() :
			(kind == 1 ? xrtValueIntMap() : xrtValueObject());
		const xerror* original;
		size_t before;
		testRequire(left && right,"nested equality fixture");
		for ( int index = 0; index < 2; ++index ) {
			bool ready;
			xstrview key = index == 0 ? XRT_STR_LITERAL("first") : XRT_STR_LITERAL("later");
			if ( kind == 0 ) {
				ready = xrtValueArrayAppend(left,first) && xrtValueArrayAppend(right,same);
			} else if ( kind == 1 ) {
				ready = xrtValueIntMapSet(left,index,first) && xrtValueIntMapSet(right,index,same);
			} else {
				ready = xrtValueObjectSet(left,key,first) && xrtValueObjectSet(right,key,same);
			}
			testRequire(ready,"nested equality admission");
		}
		before = state->Equals;
		original = arm(state);
		state->FailEqual = before + 1;
		testRequire(!xrtValueEqual(left,right) && state->Equals == before + 1,
			"graph continued after a failed equality callback");
		failed(original); state->FailEqual = 0;
		testRequire(xrtValueEqual(left,right),"graph equality recovery");
		xrtValueRelease(right); xrtValueRelease(left);
	}
}

/* Complete deterministic callback-failure prefixes across COW and post-copy
 * validation, not just the preflight. Callbacks themselves allocate nothing. */
static void callbackSweep(xvalue* first,xvalue* second,valuekeystate* state,bool hashMode)
{
	for(size_t index=1;index<32;++index){
		xvalue* set=xrtValueSet();xvalue* alias;const xerror* original;bool ready,hit;
		testRequire(set && xrtValueSetAdd(set,first),"sweep setup");
		alias=xrtValueClone(set);testRequire(alias!=NULL,"sweep alias");
		original=arm(state);
		if(hashMode)state->FailHash=state->Hashes+index;
		else state->FailEqual=state->Equals+index;
		ready=xrtValueSetAdd(set,second);hit=state->Failure==NULL;
		state->FailHash=state->FailEqual=0;
		if(hit){
			testRequire(!ready && xrtValueCount(set)==1 && xrtValueCount(alias)==1,
				"late callback failure committed COW insertion");failed(original);
		}else{
			testRequire(ready && !xrtGetError() && xrtValueCount(set)==2 &&
				xrtValueCount(alias)==1,"sweep baseline/recovery");
			xrtErrorFree(state->Failure);state->Failure=NULL;
		}
		xrtValueRelease(alias);xrtValueRelease(set);
		if(!hit){printf("callback %s first no-hit=%lu\n",hashMode?"hash":"equal",(unsigned long)index);return;}
	}
	testRequire(false,"callback sweep did not reach baseline");
}

int main(void)
{
	valuekeystate state={0}; uint64 hash=UINT64_C(0x123456789ABCDEF0);
	xvalue* first=makeKey(1,&state); xvalue* same=makeKey(1,&state);
	xvalue* second=makeKey(2,&state); xvalue* set=xrtValueSet(); xvalue* copy;
	xvalue* item; const xerror* original;
	testRequire(set != NULL,"set allocation");
	original=arm(&state); state.FailHash=state.Hashes+1;
	testRequire(!xrtValueHash(first,&hash) && hash==UINT64_C(0x123456789ABCDEF0),
		"failed hash committed output");failed(original);state.FailHash=0;
	testRequire(xrtValueHash(first,&hash),"legitimate zero callback hash");
	xrtSetErrorKind(XERR_STATE);original=xrtGetError();
	testRequire(xrtValueHash(first,&hash) && xrtGetError()==original,"hash lost prior error");
	xrtClearError();
	for(int scalar=0;scalar<2;++scalar){
		original=arm(&state);state.FailEqual=state.Equals+1;
		testRequire(!(scalar?xrtValueScalarEqual(first,same):xrtValueEqual(first,same)),
			"true equality payload overrode callback failure");
		failed(original);state.FailEqual=0;
	}
	xrtSetErrorKind(XERR_STATE);original=xrtGetError();
	testRequire(xrtValueEqual(first,same) && !xrtValueEqual(first,second) &&
		xrtValueScalarEqual(first,same) && !xrtValueScalarEqual(first,second) &&
		xrtGetError()==original,"successful true/false equality lost prior error");
	xrtClearError();callbackSweep(first,second,&state,true);callbackSweep(first,second,&state,false);
	nestedEqualFailure(first,same,&state);
	testRequire(xrtValueSetAdd(set,first),"initial key");
	copy=xrtValueClone(set);testRequire(copy!=NULL,"COW shell copy");
	/* Preflight hash/equality failures must not continue to COW/insert. */
	original=arm(&state);state.FailHash=state.Hashes+1;item=second;
	testRequire(!xrtValueSetAddTake(set,&item) && item==second,"failure consumed source");
	failed(original);state.FailHash=0;
	original=arm(&state);state.FailEqual=state.Equals+1;
	testRequire(!xrtValueSetAdd(set,second),"failed preflight equality inserted key");
	failed(original);state.FailEqual=0;
	testRequire(xrtValueCount(set)==1 && xrtValueCount(copy)==1 &&
		xrtValueSetHas(set,first) && !xrtValueSetHas(set,second),"COW visible state changed");
	original=arm(&state);state.FailEqual=state.Equals+1;
	testRequire(!xrtValueSetRemove(set,same),"failed equality removed key");
	failed(original);state.FailEqual=0;
	original=arm(&state);state.FailEqual=state.Equals+1;
	testRequire(!xrtValueSetTake(set,same),"failed equality consumed canonical key");
	failed(original);state.FailEqual=0;
	item=makeKey(3,&state);original=arm(&state);state.FailHash=state.Hashes+1;
	state.DropError=true;
	testRequire(!xrtValueSetAddNew(set,item) && state.Drops==1,"temporary was not retired");
	failed(original);state.FailHash=0;state.DropError=false;
	testRequire(xrtValueSetAdd(set,second) && xrtValueCount(set)==2 &&
		xrtValueCount(copy)==1,"recovery/COW isolation");
	xrtValueRelease(copy);xrtValueRelease(set);xrtValueRelease(second);
	xrtValueRelease(same);xrtValueRelease(first);
	testRequire(state.Drops==4,"handle ownership imbalance");
	/* The same consuming completion rule applies to every container New API. */
	set=xrtValueArray();copy=xrtValueObject();second=xrtValueInt(0);
	testRequire(set && copy && second,"consuming error fixture");
	state.DropError=true;
	for(int operation=0;operation<6;++operation){
		bool ready=false;xerrkind expected=XERR_TYPE;item=makeKey(99,&state);
		switch(operation){
		case 0:ready=xrtValueArrayAppendNew(second,item);break;
		case 1:ready=xrtValueArrayInsertNew(set,1,item);expected=XERR_RANGE;break;
		case 2:ready=xrtValueArraySetNew(set,0,item);expected=XERR_RANGE;break;
		case 3:ready=xrtValueIntMapSetNew(second,0,item);break;
		case 4:ready=xrtValueObjectSetNew(copy,(xstrview){NULL,1},item);expected=XERR_ARGUMENT;break;
		case 5:ready=xrtValueSetAddNew(second,item);break;
		}
		testRequire(!ready && xrtErrorKind(xrtGetError())==expected &&
			state.Drops==(size_t)operation+5,"consuming cleanup replaced failure/leaked owner");
		xrtClearError();
	}
	state.DropError=false;xrtValueRelease(second);xrtValueRelease(copy);xrtValueRelease(set);
	/* Nominal class callbacks have the same checked public outcome boundary. */
	first=xrtValueObject();same=xrtValueObject();
	testRequire(first && same && xrtValueTypeIdBind(first,UINT64_C(19)) &&
		xrtValueTypeIdBind(same,UINT64_C(19)) &&
		xrtValueIdentityBind(first,objectHash,objectEqual,&state) &&
		xrtValueIdentityBind(same,objectHash,objectEqual,&state),"nominal identity");
	original=arm(&state);state.FailHash=state.Hashes+1;hash=19;
	testRequire(!xrtValueHash(first,&hash) && hash==19,"nominal hash failure");
	failed(original);state.FailHash=0;
	original=arm(&state);state.FailEqual=state.Equals+1;
	testRequire(!xrtValueEqual(first,same),"nominal equality failure");
	failed(original);state.FailEqual=0;
	nestedEqualFailure(first,same,&state);
	xrtValueRelease(same);xrtValueRelease(first);
	testRequire(!state.Failure && !xrtGetError(),"pending callback error");
	printf("[PASS] value key callback boundaries\n");return 0;
}
