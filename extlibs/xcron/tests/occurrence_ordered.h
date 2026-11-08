#include "../src/timezone/timezone_internal.h"
static void testOccurrenceOrdered(void)
{
    xcronzone* Z=occurrenceZone("STD0DST,J60/2,J300/2");xcronexpression E=occurrenceExpr("0 30 2 1 MAR *");
    xtime Begin=occurrenceTime(2000,1,1,0,0,0),Gap=occurrenceTime(2000,3,1,2,0,0);xcronoccurrence Out;xcronoccurrenceissue Short,Long;
    testRequire(xcronOccurrenceNext(&E,Z,Begin,Gap,0,(xcrondeliverypolicy){0,1},&Out,&Short)==XCRON_OCCURRENCE_ERROR && Short.Code==XCRON_OCCURRENCE_MISSING,"first yearly gap rejects in a short window");xrtClearError();
    testRequire(xcronOccurrenceNext(&E,Z,Begin,INT64_MAX,0,(xcrondeliverypolicy){0,1},&Out,&Long)==XCRON_OCCURRENCE_ERROR && Long.Code==XCRON_OCCURRENCE_MISSING && Long.When==Gap,"first yearly gap rejects without first proving actual absence for400 years");xrtClearError();
    testRequire(!memcmp(&Short,&Long,sizeof(Short)) && Long.SegmentsExamined<=2 && Long.MonthsExamined<=4 && Long.RuleYearsExamined<=8 && Long.LabelsExamined==1,"earliest gap work is independent of the later query horizon");
    xcronoccurrencecursor C;testRequire(xcronOccurrenceCursorInit(&C,&E,Z,Begin,INT64_MAX,0,(xcrondeliverypolicy){0,1},NULL),"ordered gap cursor init");xcronoccurrencestatus S;unsigned Steps=0;
    do{S=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&Out,&Short);testRequire(++Steps<=32,"earliest gap completes in bounded small-budget calls");}while(S==XCRON_OCCURRENCE_PENDING);
    testRequire(S==XCRON_OCCURRENCE_ERROR && !memcmp(&Short,&Long,sizeof(Short)),"small budgets preserve the ordered gap metadata and work");xrtClearError();xcronZoneRelease(Z);
    /* Every label in the first segment is globally repeated. LAST filters120
     * labels before accepting the transition instant. Reuse the same segment
     * walk/calendar rather than restarting a zoned search for every rejection. */
    tzifblob Blob;const int64 Times[]={0};const uint8 Indices[]={1};const tzifkind Types[]={{120,false,"OLD"},{0,false,"UTC"}};
    tzifBuild(&Blob,'3',Times,Indices,1,Types,2,"UTC0");testRequire(xcronZoneCreateTzif((xbytesview){Blob.Data,Blob.Size},&Z,NULL),"dense repeated-label fixture");E=occurrenceExpr("* * * * * *");Begin=XRT_TIME_UNIX_EPOCH-120000;
    occurrenceExpect(&E,Z,Begin,XRT_TIME_UNIX_EPOCH,0,(xcrondeliverypolicy){XCRON_FOLD_LAST,XCRON_MISSING_REJECT},XCRON_OCCURRENCE_FOUND,0,XRT_TIME_UNIX_EPOCH,2);
    testRequire(xcronOccurrenceNext(&E,Z,Begin,XRT_TIME_UNIX_EPOCH,0,(xcrondeliverypolicy){2,1},&Out,&Long)==XCRON_OCCURRENCE_FOUND && Long.SegmentsExamined==2 && Long.LabelsExamined==121,"dense filtered candidates share two ordered segments and retain full global resolution");xcronZoneRelease(Z);
    /* Synthetic internal fallback only: the factory normally recognizes this
     * coincident season as constant. Exclusive test ownership forces the general
     * rule engine to prove its full403-year fallback; no benchmark uses this. */
    Z=occurrenceZone("STD0DST,J1/0,J1/1");uint8 Mode=Z->Mode;Z->Mode=2;E=occurrenceExpr("0 0 0 31 FEB *");
    testRequire(xcronOccurrenceCursorInit(&C,&E,Z,occurrenceTime(2000,1,1,0,0,0),INT64_MAX,0,(xcrondeliverypolicy){0,0},NULL),"synthetic fallback cursor init");
    for(unsigned J=0;J<403;++J){S=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&Out,&Long);testRequire(S==(J==402?XCRON_OCCURRENCE_NONE:XCRON_OCCURRENCE_PENDING) && Long.RuleYearsExamined==J+1,"occurrence traversal yields each actual fallback year without truncation");}
    Z->Mode=Mode;xcronZoneRelease(Z);
}
