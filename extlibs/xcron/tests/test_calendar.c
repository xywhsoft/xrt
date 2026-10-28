#include "test.h"
#include <limits.h>

static xcronexpression calendarCompile(cstr Text, xcrondialect Dialect)
{
	xcronexpression Expression; testRequire(xcronExpressionCompile((xstrview){Text,strlen(Text)},Dialect,&Expression,NULL),"calendar fixture compile"); return Expression;
}

static xtime calendarStamp(int64 Year, int Month, int Day, int Hour, int Minute, int Second, int Millisecond)
{
	xtime Value; testRequire(xrtDateTime(Year,Month,Day,Hour,Minute,Second,Millisecond,&Value),"calendar fixture timestamp"); return Value;
}

static bool calendarSame(const xdatetime* A, const xdatetime* B)
{
	return A->Year==B->Year && A->Month==B->Month && A->Day==B->Day && A->Hour==B->Hour && A->Minute==B->Minute && A->Second==B->Second &&
		A->Millisecond==B->Millisecond && A->Weekday==B->Weekday && A->YearDay==B->YearDay && A->Offset==B->Offset && A->IsDST==B->IsDST;
}

static void calendarExpect(cstr Text, xcrondialect Dialect, xtime Begin, xtime End, uint32 Flags, xtime Expected, xcroncalendarstatus Status)
{
	xcronexpression Expression=calendarCompile(Text,Dialect); xtime Next=777; xcroncalendarissue Issue;
	testRequire(xcronCalendarNext(&Expression,Begin,End,Flags,&Next,&Issue)==Status && Issue.Code==XCRON_CALENDAR_OK &&
		Issue.Field==XCRON_FIELD_NONE && Issue.MonthsExamined<=XCRON_CALENDAR_MAX_MONTHS,"calendar known result/status/work");
	testRequire(Next==(Status==XCRON_CALENDAR_FOUND ? Expected : 777),"calendar earliest timestamp/atomic NONE");
	if (Status==XCRON_CALENDAR_FOUND) {
		bool Match=false; testRequire(xcronCalendarMatch(&Expression,Next,&Match,NULL) && Match,"found UTC calendar candidate does not match");
		xdatetime First, Last, Civil, Canonical; xrtTimeSplit(Begin,&First); xrtTimeSplit(End,&Last); xrtTimeSplit(Expected,&Canonical);
		First.Offset=INT_MIN; First.Weekday=99; First.IsDST=99; First.YearDay=-100; Last.Offset=INT_MAX;
		testRequire(xcronCalendarNextCivil(&Expression,&First,&Last,Flags,&Civil,&Issue)==XCRON_CALENDAR_FOUND && calendarSame(&Civil,&Canonical),"civil and UTC search disagree/metadata not canonical");
	}
}

static void testCalendarKnown(void)
{
	xtime Jan=calendarStamp(2024,1,1,0,0,0,0), End=calendarStamp(2401,1,1,0,0,0,0);
	calendarExpect("* * * * * *",XCRON_SECONDS6,Jan,End,0,Jan,XCRON_CALENDAR_FOUND);
	calendarExpect("* * * * * *",XCRON_SECONDS6,Jan,End,XCRON_SEARCH_EXCLUSIVE,Jan+1000,XCRON_CALENDAR_FOUND);
	calendarExpect("* * * * * *",XCRON_SECONDS6,Jan+1,End,0,Jan+1000,XCRON_CALENDAR_FOUND);
	calendarExpect("* * * * *",XCRON_UNIX5,Jan+1,End,0,Jan+60000,XCRON_CALENDAR_FOUND);
	calendarExpect("0 0 29 FEB *",XCRON_UNIX5,calendarStamp(2096,2,29,0,0,0,1),End,0,calendarStamp(2104,2,29,0,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0 0 29 FEB *",XCRON_UNIX5,calendarStamp(2399,3,1,0,0,0,0),End,0,calendarStamp(2400,2,29,0,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0 9 * * MON-FRI",XCRON_UNIX5,calendarStamp(2024,3,1,9,0,0,123),End,0,calendarStamp(2024,3,4,9,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0 0 */2 * MON",XCRON_UNIX5,Jan,End,XCRON_SEARCH_EXCLUSIVE,calendarStamp(2024,1,15,0,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0 0 1-31/2 * MON",XCRON_UNIX5,Jan,End,XCRON_SEARCH_EXCLUSIVE,calendarStamp(2024,1,3,0,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0 0 30 FEB MON",XCRON_UNIX5,calendarStamp(2024,2,1,0,0,0,0),End,0,calendarStamp(2024,2,5,0,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0 0 31 APR *",XCRON_UNIX5,Jan,End,0,0,XCRON_CALENDAR_NONE);
	calendarExpect("0 0 0 1 JAN *",XCRON_SECONDS6,calendarStamp(-1,12,31,23,59,59,999),0,0,0,XCRON_CALENDAR_FOUND);
	calendarExpect("@yearly",XCRON_UNIX5,calendarStamp(12000,1,1,0,0,0,0),calendarStamp(12001,1,1,0,0,0,0),XCRON_SEARCH_EXCLUSIVE,
		calendarStamp(12001,1,1,0,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("0,30 9-17 * * MON-FRI",XCRON_UNIX5,calendarStamp(2024,1,1,17,30,0,1),End,0,calendarStamp(2024,1,2,9,0,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("*/13 7-59/11 6-22/3 * * *",XCRON_SECONDS6,calendarStamp(2024,1,1,6,7,53,0),End,0,calendarStamp(2024,1,1,6,18,0,0),XCRON_CALENDAR_FOUND);
	calendarExpect("* * * * * *",XCRON_SECONDS6,Jan,Jan,0,Jan,XCRON_CALENDAR_FOUND);
	calendarExpect("* * * * * *",XCRON_SECONDS6,Jan,Jan,XCRON_SEARCH_EXCLUSIVE,0,XCRON_CALENDAR_NONE);
	calendarExpect("* * * * * *",XCRON_SECONDS6,Jan+1,Jan+999,0,0,XCRON_CALENDAR_NONE);
	xcronexpression Impossible=calendarCompile("0 0 30 FEB *",XCRON_UNIX5); xtime Next=777; xcroncalendarissue Issue;
	testRequire(xcronCalendarNext(&Impossible,INT64_MIN,INT64_MAX,0,&Next,&Issue)==XCRON_CALENDAR_NONE && Next==777 &&
		Issue.Code==XCRON_CALENDAR_OK && Issue.MonthsExamined==XCRON_CALENDAR_MAX_MONTHS,"400-year complete-cycle no-match bound");
}

/* Independent Gregorian oracle: literal month table and one-second/day stepping,
 * without the production month/day bitmask construction or hierarchical search. */
static int calendarOracleDays(int64 Year, int Month)
{
	static const int Days[]={31,28,31,30,31,30,31,31,30,31,30,31}; int64 Astronomical=Year<0 ? Year+1 : Year;
	return Days[Month-1]+(Month==2 && Astronomical%4==0 && (Astronomical%100!=0 || Astronomical%400==0));
}

static void calendarOracleDay(xdatetime* Date)
{
	Date->Weekday=(Date->Weekday+1)%7;
	if (++Date->Day>calendarOracleDays(Date->Year,Date->Month)) { Date->Day=1; if (++Date->Month>12) { Date->Month=1; if (++Date->Year==0) Date->Year=1; } }
}

static void calendarOracleSecond(xdatetime* Date)
{
	if (++Date->Second==60) { Date->Second=0; if (++Date->Minute==60) { Date->Minute=0; if (++Date->Hour==24) { Date->Hour=0; calendarOracleDay(Date); } } }
}

static bool calendarOracleMatch(const xcronexpression* Expression, const xdatetime* Date)
{
	const int Values[]={Date->Second,Date->Minute,Date->Hour,Date->Day,Date->Month,Date->Weekday}; bool Selected[6];
	for (unsigned Field=0; Field<6; ++Field) Selected[Field]=((Expression->Fields[Field]>>Values[Field])&1u)!=0;
	bool Day=(Expression->Flags&3u) ? Selected[3] && Selected[5] : Selected[3] || Selected[5];
	return !Date->Millisecond && Selected[0] && Selected[1] && Selected[2] && Selected[4] && Day;
}

static void testCalendarGregorianCycle(void)
{
	const cstr Texts[]={"0 0 * * *","0 0 * * MON","0 0 1 * *","0 0 1 * MON","0 0 1-31 * MON",
		"0 0 */2 * MON","0 0 1 * */2","0 0 * FEB *","0 0 30 FEB *","0 0 30 FEB MON"};
	xcronexpression Expressions[10]; for (unsigned Index=0; Index<10; ++Index) Expressions[Index]=calendarCompile(Texts[Index],XCRON_UNIX5);
	xdatetime Date={0}; Date.Year=2000; Date.Month=1; Date.Day=1; Date.Weekday=6; /* 2000-01-01 was Saturday. */
	xtime Time=calendarStamp(2000,1,1,0,0,0,0);
	for (unsigned Day=0; Day<146097; ++Day) {
		for (unsigned Index=0; Index<10; ++Index) {
			bool Actual=false, Expected=calendarOracleMatch(&Expressions[Index],&Date);
			testRequire(xcronCalendarMatch(&Expressions[Index],Time,&Actual,NULL) && Actual==Expected,"400-year DOM/DOW/month/day weekday oracle");
			if (Day%997u==0) { xdatetime Civil=Date; Civil.Offset=INT_MAX; Civil.YearDay=-123; Civil.Weekday=-123; Civil.IsDST=123;
				testRequire(xcronCalendarMatchCivil(&Expressions[Index],&Civil,&Actual,NULL) && Actual==Expected,"civil metadata ignored/recomputed"); }
		}
		calendarOracleDay(&Date); Time+=XRT_TIME_DAY;
	}
	testRequire(Date.Year==2400 && Date.Month==1 && Date.Day==1 && Date.Weekday==6,"independent Gregorian cycle length/weekday");
}

static uint32 calendarRandom(uint32* State) { *State^=*State<<13u; *State^=*State>>17u; *State^=*State<<5u; return *State; }

static void testCalendarSearchOracle(void)
{
	uint32 State=0x75219483u;
	for (unsigned Trial=0; Trial<500; ++Trial) {
		xcronexpression Expression=calendarCompile("* * * * * *",XCRON_SECONDS6);
		Expression.Fields[0]=((uint64)calendarRandom(&State)<<28 | calendarRandom(&State))&UINT64_C(0x0fffffffffffffff); Expression.Fields[0]|=1;
		Expression.Fields[1]=((uint64)calendarRandom(&State)<<28 | calendarRandom(&State))&UINT64_C(0x0fffffffffffffff); Expression.Fields[1]|=1;
		Expression.Fields[2]=(calendarRandom(&State)&0xffffffu)|1u;
		Expression.Fields[3]=((uint64)calendarRandom(&State)&UINT64_C(0xfffffffe))|2u;
		Expression.Fields[4]=(calendarRandom(&State)&0x1ffeu)|2u; Expression.Fields[5]=(calendarRandom(&State)&127u)|1u; Expression.Flags=calendarRandom(&State)&3u;
		int64 Year=1900+calendarRandom(&State)%600u; int Month=1+(int)(calendarRandom(&State)%12u), Day=1+(int)(calendarRandom(&State)%28u);
		xtime Begin=calendarStamp(Year,Month,Day,(int)(calendarRandom(&State)%24u),(int)(calendarRandom(&State)%60u),(int)(calendarRandom(&State)%60u),(int)(calendarRandom(&State)%1000u));
		uint32 Flags=Trial%2u ? XCRON_SEARCH_EXCLUSIVE : 0; xtime End=Begin+(xtime)(calendarRandom(&State)%86400u)*1000;
		xdatetime Reference; xrtTimeSplit(Begin,&Reference); xtime Candidate=Begin-Reference.Millisecond;
		if (Reference.Millisecond || Flags) { Candidate+=1000; calendarOracleSecond(&Reference); } Reference.Millisecond=0;
		bool Found=false;
		for (; Candidate<=End; Candidate+=1000) {
			if (calendarOracleMatch(&Expression,&Reference)) { Found=true; break; }
			calendarOracleSecond(&Reference);
		}
		xtime Actual=777; xcroncalendarissue Issue;
		testRequire(xcronCalendarNext(&Expression,Begin,End,Flags,&Actual,&Issue)==(Found ? XCRON_CALENDAR_FOUND : XCRON_CALENDAR_NONE) &&
			Actual==(Found ? Candidate : 777) && Issue.Code==0 && Issue.MonthsExamined<=2,"500 earliest-match brute-second interval oracles");
	}
	/* Whole-day enumeration independently checks sparse century/leap searches. */
	for (unsigned Trial=0; Trial<1000; ++Trial) {
		xcronexpression Expression=calendarCompile("0 0 0 * * *",XCRON_SECONDS6);
		Expression.Fields[3]=(uint64)1u<<(1u+calendarRandom(&State)%31u); Expression.Fields[4]=(uint64)1u<<(1u+calendarRandom(&State)%12u);
		Expression.Fields[5]=(uint64)1u<<(calendarRandom(&State)%7u); Expression.Flags=calendarRandom(&State)&3u;
		xtime Begin=calendarStamp(1800+calendarRandom(&State)%600u,1+(int)(calendarRandom(&State)%12u),1,0,0,0,0), End=Begin+INT64_C(3660)*XRT_TIME_DAY;
		xdatetime Reference; xrtTimeSplit(Begin,&Reference); xtime Candidate=Begin; bool Found=false;
		for (; Candidate<=End; Candidate+=XRT_TIME_DAY) { if (calendarOracleMatch(&Expression,&Reference)) { Found=true; break; } calendarOracleDay(&Reference); }
		xtime Actual=777; testRequire(xcronCalendarNext(&Expression,Begin,End,0,&Actual,NULL)==(Found ? XCRON_CALENDAR_FOUND : XCRON_CALENDAR_NONE) &&
			Actual==(Found ? Candidate : 777),"1000 sparse Gregorian brute-day search oracles");
	}
}

static void testCalendarEdgesAndCivil(void)
{
	xcronexpression Every=calendarCompile("* * * * * *",XCRON_SECONDS6); xtime Next=777; xcroncalendarissue Issue; bool Match=false;
	const xtime Begins[]={INT64_MIN,INT64_MIN+807,INT64_MIN+808,-1001,-1000,-999,-1,0,1,INT64_MAX-807,INT64_MAX};
	for (size_t Index=0; Index<sizeof(Begins)/sizeof(Begins[0]); ++Index) for (uint32 Flags=0; Flags<=1; ++Flags) {
		xtime Begin=Begins[Index], Fraction=Begin%1000; if (Fraction<0) Fraction+=1000;
		xtime Advance=Fraction ? 1000-Fraction : Flags ? 1000 : 0; bool Fits=Begin<=INT64_MAX-Advance;
		Next=777; testRequire(xcronCalendarNext(&Every,Begin,INT64_MAX,Flags,&Next,&Issue)==(Fits ? XCRON_CALENDAR_FOUND : XCRON_CALENDAR_NONE) &&
			Next==(Fits ? Begin+Advance : 777),"int64 endpoint/fractional/negative rounding");
		testRequire(xcronCalendarMatch(&Every,Begin,&Match,NULL) && Match==(Fraction==0),"exact instant millisecond predicate");
	}
	xdatetime Start={0}, End={0}, Result; Start.Year=INT64_MAX; Start.Month=12; Start.Day=31; Start.Hour=23; Start.Minute=59; Start.Second=59; End=Start;
	testRequire(xcronCalendarNextCivil(&Every,&Start,&End,0,&Result,NULL)==XCRON_CALENDAR_FOUND && Result.Year==INT64_MAX && Result.Second==59 &&
		Result.Offset==0 && Result.IsDST==-1,"full int64 civil positive-year boundary");
	xdatetime Before=Result; testRequire(xcronCalendarNextCivil(&Every,&Start,&End,XCRON_SEARCH_EXCLUSIVE,&Result,NULL)==XCRON_CALENDAR_NONE &&
		!memcmp(&Result,&Before,sizeof(Result)),"exclusive civil year overflow changed output");
	Start.Millisecond=999; End.Millisecond=999;
	testRequire(xcronCalendarNextCivil(&Every,&Start,&End,0,&Result,NULL)==XCRON_CALENDAR_NONE,"fractional final civil second");
	Start=(xdatetime){0}; Start.Year=INT64_MIN; Start.Month=1; Start.Day=1; Start.Millisecond=999; End=Start; End.Second=1;
	testRequire(xcronCalendarNextCivil(&Every,&Start,&End,0,&Result,NULL)==XCRON_CALENDAR_FOUND && Result.Year==INT64_MIN && Result.Second==1 &&
		Result.Millisecond==0 && Result.YearDay==1,"full int64 civil negative-year metadata");
	Start=(xdatetime){0}; Start.Year=-1; Start.Month=12; Start.Day=31; Start.Hour=23; Start.Minute=59; Start.Second=59; Start.Millisecond=999;
	End=(xdatetime){0}; End.Year=1; End.Month=1; End.Day=1;
	testRequire(xcronCalendarNextCivil(&Every,&Start,&End,0,&Result,NULL)==XCRON_CALENDAR_FOUND && Result.Year==1 && Result.Month==1 && Result.Day==1 &&
		Result.Weekday==1 && Result.YearDay==1,"civil BCE/CE transition skipped/introduced year zero");
	xcronexpression Impossible=calendarCompile("0 0 30 FEB *",XCRON_UNIX5);
	Start.Year=INT64_MIN; Start.Month=1; Start.Day=1; Start.Hour=Start.Minute=Start.Second=Start.Millisecond=0; End.Year=INT64_MAX; End.Month=12; End.Day=31;
	testRequire(xcronCalendarNextCivil(&Impossible,&Start,&End,0,&Result,&Issue)==XCRON_CALENDAR_NONE && Issue.MonthsExamined==4801,"civil complete-cycle bound at extreme years");
}

static void testCalendarFailures(void)
{
	xcronexpression Expression=calendarCompile("* * * * * *",XCRON_SECONDS6), Good=Expression; xtime Next=777; xcroncalendarissue Issue; bool Match=true;
	for (unsigned Field=0; Field<6; ++Field) for (unsigned Kind=0; Kind<2; ++Kind) {
		Expression=Good; Expression.Fields[Field]=Kind ? UINT64_MAX : 0;
		testRequire(!xcronCalendarMatch(&Expression,0,&Match,&Issue) && Match && Issue.Code==XCRON_CALENDAR_EXPRESSION && Issue.Field==(xcronfield)Field,"invalid expression field match publication"); xrtClearError();
		testRequire(xcronCalendarNext(&Expression,0,1000,0,&Next,&Issue)==XCRON_CALENDAR_ERROR && Next==777 && Issue.Field==(xcronfield)Field,"invalid expression field search publication"); xrtClearError();
	}
	for (unsigned Kind=0; Kind<3; ++Kind) {
		Expression=Good; if (!Kind) Expression.Dialect=(xcrondialect)0; else if (Kind==1) Expression.Flags=4; else Expression.Dialect=XCRON_UNIX5;
		testRequire(xcronCalendarNext(&Expression,0,1000,0,&Next,&Issue)==XCRON_CALENDAR_ERROR && Issue.Code==XCRON_CALENDAR_EXPRESSION && Next==777,"invalid dialect/flags/UNIX5 seconds"); xrtClearError();
	}
	Expression=Good; testRequire(xcronCalendarNext(&Expression,1000,0,0,&Next,&Issue)==XCRON_CALENDAR_ERROR && Next==777 && Issue.Code==XCRON_CALENDAR_WINDOW,"reverse UTC window"); xrtClearError();
	xdatetime Date={0}; Date.Year=2024; Date.Month=1; Date.Day=1; xdatetime End=Date, Result, Before; memset(&Result,0xa5,sizeof(Result)); memcpy(&Before,&Result,sizeof(Result));
	for (unsigned Kind=0; Kind<8; ++Kind) {
		xdatetime Bad=Date; if (!Kind) Bad.Year=0; else if (Kind==1) Bad.Month=13; else if (Kind==2) Bad.Day=32; else if (Kind==3) { Bad.Month=2; Bad.Day=30; }
		else if (Kind==4) Bad.Hour=24; else if (Kind==5) Bad.Minute=-1; else if (Kind==6) Bad.Second=60; else Bad.Millisecond=1000;
		testRequire(!xcronCalendarMatchCivil(&Expression,&Bad,&Match,&Issue) && Match && Issue.Code==XCRON_CALENDAR_DATE,"invalid civil date predicate"); xrtClearError();
		testRequire(xcronCalendarNextCivil(&Expression,&Date,&Bad,0,&Result,&Issue)==XCRON_CALENDAR_ERROR && Issue.Code==XCRON_CALENDAR_DATE &&
			!memcmp(&Result,&Before,sizeof(Result)),"invalid civil end changed output"); xrtClearError();
	}
	xcroncalendarissue Saved={XCRON_CALENDAR_OK,XCRON_FIELD_NONE,777}; Issue=Saved;
	testRequire(xcronCalendarNext(&Expression,0,1000,2,&Next,&Issue)==XCRON_CALENDAR_ERROR && Next==777 && !memcmp(&Issue,&Saved,sizeof(Issue)),"unknown flags changed outputs"); xrtClearError();
	testRequire(!xcronCalendarMatch(NULL,0,&Match,&Issue) && !memcmp(&Issue,&Saved,sizeof(Issue)),"NULL expression changed issue"); xrtClearError();
	testRequire(!xcronCalendarMatch(&Expression,0,(bool*)&Expression,&Issue) && !memcmp(&Expression,&Good,sizeof(Expression)),"predicate output aliases expression"); xrtClearError();
	testRequire(xcronCalendarNext(&Expression,0,1000,0,(xtime*)&Issue,&Issue)==XCRON_CALENDAR_ERROR && !memcmp(&Issue,&Saved,sizeof(Issue)),"search outputs alias"); xrtClearError();
	testRequire(xcronCalendarNext(&Expression,0,1000,0,(xtime*)&Expression,&Issue)==XCRON_CALENDAR_ERROR && !memcmp(&Expression,&Good,sizeof(Expression)),"search output aliases expression"); xrtClearError();
	testRequire(xcronCalendarNextCivil(&Expression,&Date,&End,0,&Date,&Issue)==XCRON_CALENDAR_ERROR && Date.Year==2024 && Date.Second==0,"civil in-place output aliases input"); xrtClearError();
	testRequire(!xcronCalendarMatchCivil(&Expression,(const xdatetime*)(uintptr_t)(UINTPTR_MAX-1u),&Match,&Issue),"wrapped civil span"); xrtClearError();
	testRequire(xcronCalendarNext((const xcronexpression*)(uintptr_t)(UINTPTR_MAX-1u),0,1000,0,&Next,&Issue)==XCRON_CALENDAR_ERROR,"wrapped expression span"); xrtClearError();
	testRequire(xcronCalendarNextCivil(&Expression,NULL,&End,0,&Result,&Issue)==XCRON_CALENDAR_ERROR && !memcmp(&Issue,&Saved,sizeof(Issue)),"NULL civil begin publication"); xrtClearError();
}

static void testCalendarAllocation(void)
{
	xcronexpression Expression=calendarCompile("0,30 9-17 * * MON-FRI",XCRON_UNIX5), Impossible=calendarCompile("0 0 30 FEB *",XCRON_UNIX5);
	xtime Begin=calendarStamp(2024,3,1,17,30,0,1), End=calendarStamp(2025,1,1,0,0,0,0), Expected=calendarStamp(2024,3,4,9,0,0,0), Next; bool Match;
	xdatetime Civil, Last, Result; xrtTimeSplit(Begin,&Civil); xrtTimeSplit(End,&Last); xcroncalendarissue Issue;
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient"); const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (unsigned Index=0; Index<10000; ++Index) testRequire(xcronCalendarNext(&Expression,Begin,End,0,&Next,&Issue)==XCRON_CALENDAR_FOUND && Next==Expected &&
		xcronCalendarMatch(&Expression,Next,&Match,&Issue) && Match && xcronCalendarNextCivil(&Expression,&Civil,&Last,0,&Result,&Issue)==XCRON_CALENDAR_FOUND &&
		xcronCalendarMatchCivil(&Expression,&Result,&Match,&Issue) && Match && xrtGetError()==Ambient,"warm calendar cycle/error identity");
	testRequire(xcronCalendarNext(&Impossible,INT64_MIN,INT64_MAX,0,&Next,&Issue)==XCRON_CALENDAR_NONE && xrtGetError()==Ambient,"normal NONE replaced ambient error");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false); testRequire(After.BlockAllocCalls==Before.BlockAllocCalls && After.MallocCalls==Before.MallocCalls &&
		After.CallocCalls==Before.CallocCalls && After.ReallocCalls==Before.ReallocCalls,"successful/none calendar operations allocated");
	#endif
	xrtClearError(); xallocationbudget* Budget=NULL; xallocationscope Scope={0}; testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"calendar quota enter");
	testRequire(xcronCalendarNext(&Expression,Begin,End,0,&Next,NULL)==XCRON_CALENDAR_FOUND && Next==Expected &&
		xcronCalendarNext(&Impossible,INT64_MIN,INT64_MAX,0,&Next,NULL)==XCRON_CALENDAR_NONE,"calendar normal path depends on heap");
	Expression.Fields[0]=0; Next=777;
	testRequire(xcronCalendarNext(&Expression,Begin,End,0,&Next,&Issue)==XCRON_CALENDAR_ERROR && Next==777 && Issue.Code==XCRON_CALENDAR_EXPRESSION &&
		Issue.Field==XCRON_FIELD_SECOND && xrtErrorKind(xrtGetError())==XERR_MEMORY,"calendar expression issue lost under diagnostic OOM");
	Expression=Impossible; Civil.Year=0; Match=true;
	testRequire(!xcronCalendarMatchCivil(&Expression,&Civil,&Match,&Issue) && Match && Issue.Code==XCRON_CALENDAR_DATE && xrtErrorKind(xrtGetError())==XERR_MEMORY,"calendar date issue lost under OOM");
	testRequire(xcronCalendarNext(&Expression,End,Begin,0,&Next,&Issue)==XCRON_CALENDAR_ERROR && Next==777 && Issue.Code==XCRON_CALENDAR_WINDOW &&
		xrtErrorKind(xrtGetError())==XERR_MEMORY,"calendar window issue lost under OOM");
	testRequire(xrtAllocationBudgetLeave(&Scope),"calendar quota leave"); xallocationbudgetinfo Info={0}; Info.Size=sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && Info.LiveBytes==0 && Info.Rejections>0,"calendar quota leak"); xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testCalendarKnown(); testCalendarGregorianCycle(); testCalendarSearchOracle(); testCalendarEdgesAndCivil(); testCalendarFailures(); testCalendarAllocation();
	printf("[PASS] xcron calendar: 400-year/146097 days/DOM-DOW/1500 brute oracles/UTC-civil/full-int64/bounds/atomicity/zero-heap/OOM\n"); return 0;
}
