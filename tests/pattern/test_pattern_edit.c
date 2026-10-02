#include "../test.h"
#include "../../src/internal/xrt_pattern.h"

/* 白盒只用于有限计数器边界；其余编辑合同均通过公共 API 检查。 */
static void requireKind(xerrkind Kind)
{
	testRequire(xrtErrorKind(xrtGetError()) == Kind, "pattern edit error kind mismatch");
	xrtClearError();
}

static void commitWithoutAllocation(xpatternedit* pEdit)
{
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(xrtMemDebugFailAfter(0), "edit fail injection failed");
	#endif
	testRequire(xrtPatternEditCommit(pEdit), "valid prepared edit did not commit");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(!xrtMemDebugFailTriggered(), "prepared commit allocated");
	xrtMemDebugFailClear();
	#endif
	testRequire(xrtGetError() == NULL, "valid commit left an error");
}

static void testEdits(void)
{
	xpatternbuilder* b=xrtPatternBuilderCreate();
	xpatternspec specs[2]={{XRT_STR_LITERAL("/literal"),NULL,0,0},
		{XRT_STR_LITERAL("/{field}"),(ptr)(uintptr_t)7u,-1,0}};
	xpattern* empty=xrtPatternBuilderCompile(b);
	uint64 version=xrtPatternBuilderVersion(b);
	xpatternedit* edit=xrtPatternBuilderPrepareAdd(b,specs,2);
	testRequire(edit && xrtPatternEditReady(edit) && xrtPatternEditCount(edit)==2, "prepare add failed");
	xpatternid first=xrtPatternEditId(edit,0), second=xrtPatternEditId(edit,1);
	testRequire(first && second && first!=second, "prepared IDs invalid");
	testRequire(!xrtPatternBuilderContains(b,first) && xrtPatternBuilderCount(b)==0 &&
		xrtPatternBuilderVersion(b)==version && !xrtPatternBuilderDirty(b), "prepare mutated logical state");
	xpattern* same=xrtPatternBuilderCompile(b);
	testRequire(same==empty, "prepare invalidated cached program"); xrtPatternRelease(same);
	testRequire(xrtPatternBuilderAdd(b,&specs[0])==XPATTERN_ID_INVALID, "pending add accepted"); requireKind(XERR_STATE);
	testRequire(!xrtPatternBuilderAddMany(b,NULL,0,NULL), "pending batch accepted"); requireKind(XERR_STATE);
	testRequire(!xrtPatternBuilderSet(b,first,&specs[0]), "pending set accepted"); requireKind(XERR_STATE);
	testRequire(!xrtPatternBuilderRemove(b,first), "pending remove accepted"); requireKind(XERR_STATE);
	xrtPatternBuilderClear(b); requireKind(XERR_STATE);
	testRequire(!xrtPatternBuilderReserve(b,1), "pending reserve accepted"); requireKind(XERR_STATE);
	testRequire(xrtPatternBuilderPrepareClear(b)==NULL, "second edit accepted"); requireKind(XERR_STATE);
	testRequire(xrtPatternEditId(edit,2)==XPATTERN_ID_INVALID, "out-of-range edit ID accepted"); requireKind(XERR_RANGE);
	xrtPatternEditFree(edit);
	testRequire(xrtPatternBuilderCount(b)==0 && xrtPatternBuilderVersion(b)==version &&
		!xrtPatternBuilderDirty(b), "abort mutated logical state");
	edit=xrtPatternBuilderPrepareAdd(b,specs,2);
	testRequire(edit && xrtPatternEditId(edit,0)==first && xrtPatternEditId(edit,1)==second, "abort consumed IDs");
	commitWithoutAllocation(edit);
	testRequire(!xrtPatternEditReady(edit) && xrtPatternEditId(edit,0)==first, "committed metadata lost");
	testRequire(!xrtPatternEditCommit(edit), "duplicate commit accepted"); requireKind(XERR_STATE);
	xrtPatternEditFree(edit);
	testRequire(xrtPatternBuilderCount(b)==2 && xrtPatternBuilderContains(b,first) &&
		xrtPatternBuilderVersion(b)==version+1 && xrtPatternBuilderDirty(b), "add commit mismatch");
	testRequire(xrtPatternCount(empty)==0, "old program changed");
	xpattern* before=xrtPatternBuilderCompile(b);
	testRequire(before && xrtPatternId(before,0)==first && xrtPatternId(before,1)==second, "registration order mismatch");
	xpatternspec replacement={XRT_STR_LITERAL("/new"),(ptr)(uintptr_t)8u,20,0};
	edit=xrtPatternBuilderPrepareSet(b,first,&replacement);
	testRequire(edit && xrtPatternEditId(edit,0)==first && !xrtPatternBuilderDirty(b), "set preparation changed state");
	xrtPatternEditFree(edit);
	same=xrtPatternBuilderCompile(b); testRequire(same==before, "abort invalidated cache"); xrtPatternRelease(same);
	edit=xrtPatternBuilderPrepareSet(b,first,&replacement); testRequire(edit!=NULL, "set retry failed");
	commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	xpattern* after=xrtPatternBuilderCompile(b);
	testRequire(after && xrtPatternId(after,0)==first && xrtPatternValue(after,0)==replacement.Value &&
		xrtPatternTest(after,XRT_STR_LITERAL("/new"))==XPATTERN_MATCH &&
		xrtPatternTest(before,XRT_STR_LITERAL("/literal"))==XPATTERN_MATCH, "set/snapshot mismatch");
	edit=xrtPatternBuilderPrepareRemove(b,first); testRequire(edit!=NULL, "remove prepare failed");
	xrtPatternEditFree(edit); testRequire(xrtPatternBuilderContains(b,first), "remove abort changed state");
	edit=xrtPatternBuilderPrepareRemove(b,first); commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	testRequire(!xrtPatternBuilderContains(b,first) && !xrtPatternBuilderRemove(b,first) &&
		xrtGetError()==NULL, "stale original remove contract changed");
	testRequire(xrtPatternBuilderPrepareSet(b,first,&replacement)==NULL, "prepared stale set accepted"); requireKind(XERR_STATE);
	xpatternid reused=xrtPatternBuilderAdd(b,&replacement);
	testRequire(reused && reused!=first, "removed ID resurrected");
	edit=xrtPatternBuilderPrepareClear(b); testRequire(edit!=NULL, "clear prepare failed");
	xrtPatternEditFree(edit); testRequire(xrtPatternBuilderContains(b,reused), "clear abort changed state");
	edit=xrtPatternBuilderPrepareClear(b); commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	testRequire(xrtPatternBuilderCount(b)==0 && !xrtPatternBuilderContains(b,reused), "clear commit mismatch");
	version=xrtPatternBuilderVersion(b);
	edit=xrtPatternBuilderPrepareAdd(b,NULL,0); testRequire(edit && xrtPatternEditCount(edit)==0, "empty add prepare failed");
	commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	edit=xrtPatternBuilderPrepareClear(b); testRequire(edit!=NULL, "empty clear prepare failed");
	commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	testRequire(xrtPatternBuilderVersion(b)==version, "empty edit advanced version");
	xrtPatternRelease(empty); xrtPatternRelease(before); xrtPatternRelease(after); xrtPatternBuilderFree(b);
}

static void testMixedSlots(void)
{
	xpatternbuilder* b=xrtPatternBuilderCreate();
	xpatternspec spec={XRT_STR_LITERAL("/old"),NULL,0,0};
	xpatternid ids[3];
	for(size_t i=0;i<3;++i) ids[i]=xrtPatternBuilderAdd(b,&spec);
	testRequire(xrtPatternBuilderRemove(b,ids[1]) && xrtPatternBuilderRemove(b,ids[0]), "free-list setup failed");
	xpatternspec batch[4]={{XRT_STR_LITERAL("/a"),NULL,0,0},
		{XRT_STR_LITERAL("/b"),NULL,0,0},{XRT_STR_LITERAL("/c"),NULL,0,0},
		{XRT_STR_LITERAL("/d"),NULL,0,0}};
	xpatternedit* edit=xrtPatternBuilderPrepareAdd(b,batch,4);
	testRequire(edit!=NULL, "mixed-slot prepare failed");
	xpatternid prepared[4];
	for(size_t i=0;i<4;++i) prepared[i]=xrtPatternEditId(edit,i);
	testRequire((uint32)prepared[0]==1u && (uint32)prepared[1]==2u &&
		(uint32)prepared[2]==4u && (uint32)prepared[3]==5u &&
		(prepared[0]>>32u)==2u && (prepared[1]>>32u)==2u &&
		(prepared[2]>>32u)==1u && (prepared[3]>>32u)==1u, "mixed-slot IDs mismatch");
	xrtPatternEditFree(edit);
	edit=xrtPatternBuilderPrepareAdd(b,batch,4); testRequire(edit!=NULL, "mixed-slot retry failed");
	for(size_t i=0;i<4;++i) testRequire(xrtPatternEditId(edit,i)==prepared[i], "abort consumed free-list IDs");
	commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	xpattern* program=xrtPatternBuilderCompile(b); testRequire(program && xrtPatternCount(program)==5, "mixed-slot compile failed");
	testRequire(xrtPatternId(program,0)==ids[2], "remaining registration order lost");
	for(size_t i=0;i<4;++i) testRequire(xrtPatternId(program,i+1)==prepared[i], "batch registration order lost");
	xrtPatternRelease(program); xrtPatternBuilderFree(b);
}

static void testBuilderRetirement(void)
{
	for(size_t kind=0;kind<4;++kind) {
		xpatternbuilder* b=xrtPatternBuilderCreate();
		xpatternspec spec={XRT_STR_LITERAL("/{id}"),NULL,0,0};
		xpatternid id=xrtPatternBuilderAdd(b,&spec);
		xpatternedit* edit=kind==0?xrtPatternBuilderPrepareAdd(b,&spec,1):
			kind==1?xrtPatternBuilderPrepareSet(b,id,&spec):
			kind==2?xrtPatternBuilderPrepareRemove(b,id):xrtPatternBuilderPrepareClear(b);
		testRequire(edit!=NULL, "retirement edit prepare failed");
		xrtPatternBuilderFree(b);
		testRequire(!xrtPatternEditReady(edit), "retired builder left a ready edit");
		testRequire(xrtPatternEditCount(edit)==(kind==3?0u:1u), "retired edit metadata invalid");
		testRequire(!xrtPatternEditCommit(edit), "retired edit committed"); requireKind(XERR_STATE);
		xrtPatternEditFree(edit);
	}
}

static void testExhaustion(void)
{
	xpatternbuilder* b=xrtPatternBuilderCreate();
	xpatternspec spec={XRT_STR_LITERAL("/first"),NULL,0,0};
	xpatternid stale=xrtPatternBuilderAdd(b,&spec);
	spec.Pattern=XRT_STR_LITERAL("/second");
	xpatternid second=xrtPatternBuilderAdd(b,&spec);
	const __xrt_pattern_source* secondSource=b->Slots[1].Source;
	b->Slots[0].Generation=UINT32_MAX;
	xpatternid exhausted=((uint64)UINT32_MAX<<32u)|1u;
	b->Slots[0].Source->Id=exhausted;
	uint64 version=b->Version;
	testRequire(!xrtPatternBuilderRemove(b,exhausted), "generation wrapped in remove"); requireKind(XERR_STATE);
	testRequire(xrtPatternBuilderPrepareRemove(b,exhausted)==NULL, "exhausted remove prepared"); requireKind(XERR_STATE);
	xrtPatternBuilderClear(b); requireKind(XERR_STATE);
	testRequire(xrtPatternBuilderPrepareClear(b)==NULL, "exhausted clear prepared"); requireKind(XERR_STATE);
	testRequire(b->Version==version && b->Count==2 && b->Slots[1].Source==secondSource &&
		xrtPatternBuilderContains(b,exhausted) && xrtPatternBuilderContains(b,second) &&
		!xrtPatternBuilderContains(b,stale), "generation preflight partially changed state");
	testRequire(xrtPatternBuilderSet(b,exhausted,&spec), "same-ID set rejected at max generation");
	b->Version=UINT64_MAX;
	testRequire(xrtPatternBuilderAdd(b,&spec)==XPATTERN_ID_INVALID, "version wrapped in add"); requireKind(XERR_STATE);
	testRequire(xrtPatternBuilderPrepareAdd(b,&spec,1)==NULL, "exhausted add prepared"); requireKind(XERR_STATE);
	testRequire(!xrtPatternBuilderSet(b,exhausted,&spec), "version wrapped in set"); requireKind(XERR_STATE);
	testRequire(xrtPatternBuilderPrepareSet(b,exhausted,&spec)==NULL, "exhausted set prepared"); requireKind(XERR_STATE);
	testRequire(xrtPatternBuilderAddMany(b,NULL,0,NULL), "empty original add changed");
	xpatternedit* edit=xrtPatternBuilderPrepareAdd(b,NULL,0);
	testRequire(edit!=NULL, "empty edit rejected at max version"); commitWithoutAllocation(edit); xrtPatternEditFree(edit);
	testRequire(b->Version==UINT64_MAX && b->Count==2, "empty max-version edit mutated state");
	xrtPatternBuilderFree(b);
}

#if defined(XRT_FEATURE_MEMORY_DEBUG)
static void testOom(void)
{
	size_t failures=0, completions=0;
	for(size_t kind=0;kind<4;++kind) {
		bool terminal=false;
		for(size_t point=0;point<128;++point) {
			xmemdebugsnapshot before,after;
			xrtMemDebugSnapshot(&before);
			xpatternbuilder* b=xrtPatternBuilderCreate();
			xpatternspec specs[8];
			for(size_t i=0;i<8;++i) specs[i]=(xpatternspec){XRT_STR_LITERAL("/{id}"),NULL,(int32)i,0};
			xpatternid id=xrtPatternBuilderAdd(b,&specs[0]);
			xpattern* cached=xrtPatternBuilderCompile(b);
			uint64 version=xrtPatternBuilderVersion(b);
			testRequire(b && id && cached, "OOM baseline failed");
			testRequire(xrtMemDebugFailAfter(point), "OOM edit injection failed");
			xpatternedit* edit=kind==0?xrtPatternBuilderPrepareAdd(b,specs,8):
				kind==1?xrtPatternBuilderPrepareSet(b,id,&specs[0]):
				kind==2?xrtPatternBuilderPrepareRemove(b,id):xrtPatternBuilderPrepareClear(b);
			bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
			if(hit) { testRequire(edit==NULL, "failed prepare returned edit"); requireKind(XERR_MEMORY); ++failures; }
			else { testRequire(edit!=NULL, "first no-hit prepare failed"); ++completions; terminal=true; }
			xrtPatternEditFree(edit);
			testRequire(xrtPatternBuilderCount(b)==1 && xrtPatternBuilderVersion(b)==version &&
				!xrtPatternBuilderDirty(b) && xrtPatternBuilderContains(b,id), "OOM/abort changed baseline");
			xpattern* same=xrtPatternBuilderCompile(b); testRequire(same==cached, "OOM/abort lost cache");
			xrtPatternRelease(same); xrtPatternRelease(cached); xrtPatternBuilderFree(b); xrtClearError();
			xrtMemDebugSnapshot(&after);
			testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
				after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
				before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
				before.UseAfterFreeCount==after.UseAfterFreeCount, "OOM/abort allocation ledger unbalanced");
			if(terminal)break;
		}
		testRequire(terminal, "OOM prefix did not reach no-hit completion");
	}
	printf("[PASS] pattern edit OOM: %zu actual failures, %zu first no-hit completions\n",failures,completions);
}
#endif

int main(void)
{
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(xrtMemDebugEnable(true), "memory debugging failed");
	#endif
	testEdits(); testMixedSlots(); testBuilderRetirement(); testExhaustion();
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testOom();
	#endif
	testRequire(!xrtPatternEditReady(NULL), "NULL edit ready"); xrtPatternEditFree(NULL);
	testRequire(xrtPatternEditId(NULL,0)==0, "NULL edit ID accepted"); requireKind(XERR_ARGUMENT);
	testRequire(xrtGetError()==NULL, "edit tests left error");
	printf("[PASS] pattern prepared edits, cache/ID rollback and exhaustion\n");
	return 0;
}
