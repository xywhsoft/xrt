#include <xcron/calendar.h>
#include "calendar_internal.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XCRON_FEATURE_XCRON_CALENDAR)

static bool xcronCalendarReport(xcroncalendarerror Code, xcronfield Field,
	xcroncalendarissue* Issue, cstr Message)
{
	if (Issue!=NULL) *Issue=(xcroncalendarissue){Code,Field,0};
	xrtSetErrorInfo(Code==XCRON_CALENDAR_STATE ? XERR_STATE : Code==XCRON_CALENDAR_ARGUMENT ? XERR_ARGUMENT : Code==XCRON_CALENDAR_EXPRESSION ? XERR_VALUE : XERR_RANGE,
		"xrt.cron",(int32)Code,Message); return false;
}

static bool xcronCalendarArgs(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
	uint32 Flags, void* Output, size_t Size, xcroncalendarissue* Issue)
{
	if (Expression==NULL || !xrtMemRangeValid(Expression,sizeof(*Expression)) || Output==NULL || !xrtMemRangeValid(Output,Size) ||
		(Issue!=NULL && !xrtMemRangeValid(Issue,sizeof(*Issue))) || (Flags&~XCRON_SEARCH_EXCLUSIVE) ||
		(Begin!=NULL && !xrtMemRangeValid(Begin,sizeof(*Begin))) || (End!=NULL && !xrtMemRangeValid(End,sizeof(*End)))) goto Invalid;
	const void* Inputs[]={Expression,Begin,End}; size_t Sizes[]={sizeof(*Expression),Begin ? sizeof(*Begin) : 0,End ? sizeof(*End) : 0};
	for (size_t Index=0; Index<3; ++Index)
		if (xrtMemRangesOverlap(Output,Size,Inputs[Index],Sizes[Index]) || (Issue!=NULL && xrtMemRangesOverlap(Issue,sizeof(*Issue),Inputs[Index],Sizes[Index]))) goto Invalid;
	if (Issue!=NULL && xrtMemRangesOverlap(Output,Size,Issue,sizeof(*Issue))) goto Invalid;
	return true;
Invalid:
	return xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"invalid cron calendar arguments, spans or output aliases");
}

bool xcronCalendarValidateExpression(const xcronexpression* Expression,xcronfield* BadField)
{
	*BadField=XCRON_FIELD_NONE;
	static const uint64 Allowed[]={UINT64_C(0x0fffffffffffffff),UINT64_C(0x0fffffffffffffff),UINT64_C(0xffffff),
		UINT64_C(0xfffffffe),UINT64_C(0x1ffe),UINT64_C(0x7f)};
	if ((Expression->Dialect!=XCRON_UNIX5 && Expression->Dialect!=XCRON_SECONDS6) || (Expression->Flags&~(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR)))
		return false;
	for (unsigned Field=0; Field<XCRON_FIELD_COUNT; ++Field)
		if (!Expression->Fields[Field] || (Expression->Fields[Field]&~Allowed[Field]) ||
			(Field==XCRON_FIELD_SECOND && Expression->Dialect==XCRON_UNIX5 && Expression->Fields[Field]!=1))
			{ *BadField=(xcronfield)Field; return false; }
	return true;
}
static bool xcronCalendarExpression(const xcronexpression* Expression,xcroncalendarissue* Issue)
{
	xcronfield Field;
	return xcronCalendarValidateExpression(Expression,&Field) || xcronCalendarReport(XCRON_CALENDAR_EXPRESSION,Field,Issue,
		Field==XCRON_FIELD_NONE?"invalid compiled cron dialect/day flags":"invalid compiled cron field bitmap");
}

static bool xcronCalendarCivil(const xdatetime* Date, xdatetime* Value, xcroncalendarissue* Issue)
{
	if (!Date->Year || Date->Month<1 || Date->Month>12 || Date->Day<1 || Date->Day>xrtDaysInMonth(Date->Year,Date->Month) ||
		Date->Hour<0 || Date->Hour>23 || Date->Minute<0 || Date->Minute>59 || Date->Second<0 || Date->Second>59 || Date->Millisecond<0 || Date->Millisecond>999)
		return xcronCalendarReport(XCRON_CALENDAR_DATE,XCRON_FIELD_NONE,Issue,"invalid Gregorian civil date/time");
	/* Astronomical-year cycle, preserving XRT's no-civil-year-zero convention.
	 * This also handles civil labels just beyond an int64 timestamp endpoint. */
	int64 Cycle=(Date->Year<0 ? Date->Year+1 : Date->Year)%400; if (Cycle<0) Cycle+=400;
	xtime Safe; (void)xrtDateTime(2000+Cycle,Date->Month,Date->Day,Date->Hour,Date->Minute,Date->Second,Date->Millisecond,&Safe);
	(void)xrtTimeSplit(Safe,Value); Value->Year=Date->Year;
	return true;
}

bool xcronCalendarPartsMatch(const xcronexpression* Expression, const xdatetime* Date)
{
	if (Date->Millisecond || !(Expression->Fields[XCRON_FIELD_MONTH]&((uint64)1u<<Date->Month)) ||
		!(Expression->Fields[XCRON_FIELD_HOUR]&((uint64)1u<<Date->Hour)) || !(Expression->Fields[XCRON_FIELD_MINUTE]&((uint64)1u<<Date->Minute)) ||
		!(Expression->Fields[XCRON_FIELD_SECOND]&((uint64)1u<<Date->Second))) return false;
	bool MonthDay=(Expression->Fields[XCRON_FIELD_DAY]&((uint64)1u<<Date->Day))!=0;
	bool WeekDay=(Expression->Fields[XCRON_FIELD_WEEKDAY]&((uint64)1u<<Date->Weekday))!=0;
	return Expression->Flags&(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR) ? MonthDay && WeekDay : MonthDay || WeekDay;
}

/* Nonzero bitmap, portable bounded binary scan (no private XRT intrinsics). */
static int xcronCalendarFirst(uint64 Mask)
{
	int Bit=0;
	if (!(Mask&UINT64_C(0xffffffff))) { Mask>>=32; Bit+=32; }
	if (!(Mask&UINT64_C(0xffff))) { Mask>>=16; Bit+=16; }
	if (!(Mask&UINT64_C(0xff))) { Mask>>=8; Bit+=8; }
	if (!(Mask&15u)) { Mask>>=4; Bit+=4; }
	if (!(Mask&3u)) { Mask>>=2; Bit+=2; }
	return Bit+(!(Mask&1u));
}

static int xcronCalendarFind(uint64 Mask, int Begin)
{
	if (Begin>=64) return -1;
	Mask&=UINT64_MAX<<Begin; return Mask ? xcronCalendarFirst(Mask) : -1;
}

static uint64 xcronCalendarDays(const xcronexpression* Expression, int Days, int FirstWeekday)
{
	uint64 Allowed=((uint64)1u<<(Days+1))-2u, Week=Expression->Fields[XCRON_FIELD_WEEKDAY];
	Week=((Week>>FirstWeekday)|(Week<<(7-FirstWeekday)))&127u;
	uint64 WeekDays=(Week|(Week<<7)|(Week<<14)|(Week<<21)|(Week<<28))<<1;
	uint64 MonthDays=Expression->Fields[XCRON_FIELD_DAY];
	return (Expression->Flags&(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR) ? MonthDays&WeekDays : MonthDays|WeekDays)&Allowed;
}

static bool xcronCalendarTime(const xcronexpression* Expression, int Hour, int Minute, int Second, xdatetime* Date)
{
	for (int H=xcronCalendarFind(Expression->Fields[XCRON_FIELD_HOUR],Hour); H>=0; H=xcronCalendarFind(Expression->Fields[XCRON_FIELD_HOUR],H+1))
		for (int M=xcronCalendarFind(Expression->Fields[XCRON_FIELD_MINUTE],H==Hour ? Minute : 0); M>=0; M=xcronCalendarFind(Expression->Fields[XCRON_FIELD_MINUTE],M+1)) {
			int S=xcronCalendarFind(Expression->Fields[XCRON_FIELD_SECOND],H==Hour && M==Minute ? Second : 0);
			if (S>=0) { Date->Hour=H; Date->Minute=M; Date->Second=S; Date->Millisecond=0; return true; }
		}
	return false;
}

static int xcronCalendarCompare(const xdatetime* Left, const xdatetime* Right)
{
	if (Left->Year!=Right->Year) return Left->Year<Right->Year ? -1 : 1;
	const int L[]={Left->Month,Left->Day,Left->Hour,Left->Minute,Left->Second,Left->Millisecond};
	const int R[]={Right->Month,Right->Day,Right->Hour,Right->Minute,Right->Second,Right->Millisecond};
	for (unsigned Index=0; Index<6; ++Index) if (L[Index]!=R[Index]) return L[Index]<R[Index] ? -1 : 1;
	return 0;
}

static bool xcronCalendarTick(xdatetime* Date)
{
	Date->Millisecond=0;
	if (++Date->Second<=59) return true;
	Date->Second=0; if (++Date->Minute<=59) return true;
	Date->Minute=0; if (++Date->Hour<=23) return true;
	Date->Hour=0; Date->Weekday=(Date->Weekday+1)%7;
	if (++Date->Day<=xrtDaysInMonth(Date->Year,Date->Month)) return true;
	Date->Day=1; if (++Date->Month<=12) return true;
	Date->Month=1; if (Date->Year==INT64_MAX) return false;
	if (++Date->Year==0) Date->Year=1;
	return true;
}

/* Keep the shared loop/start in the synchronous fast path on optimizing targets. */
#if defined(_MSC_VER)
#define XCRON_CALENDAR_INLINE static __forceinline
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
#define XCRON_CALENDAR_INLINE static inline __attribute__((always_inline))
#else
#define XCRON_CALENDAR_INLINE static inline
#endif
static bool xcronCalendarMayExist(const xcronexpression* E)
{
    uint64 Months=E->Fields[XCRON_FIELD_MONTH];
    if((Months&UINT64_C(0x15aa)) || !(E->Flags&(XCRON_DAY_MONTH_STAR|XCRON_DAY_WEEK_STAR)) || (E->Fields[XCRON_FIELD_DAY]&2u))return true;
    uint64 Allowed=(Months&UINT64_C(0x0a50))?UINT64_C(0x7ffffffe):UINT64_C(0x3ffffffe);
    return (E->Fields[XCRON_FIELD_DAY]&Allowed)!=0;
}
XCRON_CALENDAR_INLINE xcroncalendarstatus xcronCalendarSearchStart(const xcronexpression* E,const xdatetime* Begin,const xdatetime* End,
    uint32 Flags,xdatetime* First,xcroncalendarissue* Issue)
{
    if(xcronCalendarCompare(Begin,End)>0){xcronCalendarReport(XCRON_CALENDAR_WINDOW,XCRON_FIELD_NONE,Issue,"cron search window is reversed");return XCRON_CALENDAR_ERROR;}
    if(Issue)*Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
    *First=*Begin;
    if((First->Millisecond || (Flags&XCRON_SEARCH_EXCLUSIVE)) && !xcronCalendarTick(First))return XCRON_CALENDAR_NONE;
    if(xcronCalendarCompare(First,End)>0)return XCRON_CALENDAR_NONE;
    if(xcronCalendarPartsMatch(E,First)){if(Issue)Issue->MonthsExamined=1;return XCRON_CALENDAR_FOUND;}
    if(!xcronCalendarMayExist(E))return XCRON_CALENDAR_NONE;
    return XCRON_CALENDAR_PENDING;
}
void xcronCalendarValueInit(xcroncalendarvalue* V,const xdatetime* First,const xdatetime* End,xcroncalendarstatus Status)
{
    V->First=V->Date=*First;V->End=*End;V->Status=Status;V->Months=Status==XCRON_CALENDAR_FOUND?1:0;
    V->FirstWeekday=Status==XCRON_CALENDAR_PENDING?(First->Weekday+7-((First->Day-1)%7))%7:0;
}
XCRON_CALENDAR_INLINE xcroncalendarstatus xcronCalendarMonths(const xcronexpression* E,const xdatetime* First,
    const xdatetime* End,xdatetime* InOutDate,uint32* InOutMonths,int* InOutWeekday,uint32 Budget)
{
    xdatetime D=*InOutDate;uint32 Months=*InOutMonths;int Weekday=*InOutWeekday;
    const uint64 MonthMask=E->Fields[XCRON_FIELD_MONTH];xcroncalendarstatus Status=XCRON_CALENDAR_PENDING;
    uint32 Limit=XCRON_CALENDAR_MAX_MONTHS;if(Budget<Limit-Months)Limit=Months+Budget;
    for(;Months<Limit;++Months){
        if(D.Year>End->Year || (D.Year==End->Year && D.Month>End->Month)){Status=XCRON_CALENDAR_NONE;break;}
        int Days=xrtDaysInMonth(D.Year,D.Month);
        if(MonthMask&((uint64)1u<<D.Month)){
            uint64 Mask=xcronCalendarDays(E,Days,Weekday);
            for(int Day=xcronCalendarFind(Mask,Months==0?First->Day:1);Day>=0;Day=xcronCalendarFind(Mask,Day+1)){
                D.Day=Day;bool Same=Months==0 && Day==First->Day;
                if(!xcronCalendarTime(E,Same?First->Hour:0,Same?First->Minute:0,Same?First->Second:0,&D))continue;
                ++Months;Status=xcronCalendarCompare(&D,End)>0?XCRON_CALENDAR_NONE:XCRON_CALENDAR_FOUND;goto Done;
            }
        }
        Weekday=(Weekday+Days)%7;
        if(++D.Month>12){D.Month=1;if(D.Year==INT64_MAX){++Months;Status=XCRON_CALENDAR_NONE;goto Done;}if(++D.Year==0)D.Year=1;}
    }
    if(Months==XCRON_CALENDAR_MAX_MONTHS || D.Year>End->Year || (D.Year==End->Year && D.Month>End->Month))Status=XCRON_CALENDAR_NONE;
Done:
    *InOutDate=D;*InOutMonths=Months;*InOutWeekday=Weekday;return Status;
}
xcroncalendarstatus xcronCalendarValueStep(const xcronexpression* E,xcroncalendarvalue* V,uint32 Budget)
{
    if(V->Status!=XCRON_CALENDAR_PENDING || !Budget)return V->Status;
    V->Status=xcronCalendarMonths(E,&V->First,&V->End,&V->Date,&V->Months,&V->FirstWeekday,Budget);return V->Status;
}
void xcronCalendarValueBegin(const xcronexpression* E,const xdatetime* First,const xdatetime* End,xcroncalendarvalue* V)
{xcronCalendarValueInit(V,First,End,xcronCalendarMayExist(E)?XCRON_CALENDAR_PENDING:XCRON_CALENDAR_NONE);}
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
void xcronCalendarValueAfter(const xcronexpression* E,const xdatetime* First,const xdatetime* End,xcroncalendarvalue* V)
{
    xdatetime Next=*First;
    if(!xcronCalendarTick(&Next)){xcronCalendarValueInit(V,&Next,End,XCRON_CALENDAR_NONE);return;}
    xcronCalendarValueBegin(E,&Next,End,V);
}
#endif
/* Keep the legacy synchronous control loop specialized: measured cursor-state
 * setup/copy overhead is material for a single-month search. Date/bitmap rules
 * are shared; independent oracles cover both control paths. */
xcroncalendarstatus xcronCalendarSearch(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
	uint32 Flags, xdatetime* Next, xcroncalendarissue* Issue)
{
	if (xcronCalendarCompare(Begin,End)>0) { xcronCalendarReport(XCRON_CALENDAR_WINDOW,XCRON_FIELD_NONE,Issue,"cron search window is reversed"); return XCRON_CALENDAR_ERROR; }
	if (Issue!=NULL) *Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
	xdatetime First=*Begin;
	if ((First.Millisecond || (Flags&XCRON_SEARCH_EXCLUSIVE)) && !xcronCalendarTick(&First)) return XCRON_CALENDAR_NONE;
	if (xcronCalendarCompare(&First,End)>0) return XCRON_CALENDAR_NONE;
	if (xcronCalendarPartsMatch(Expression,&First)) {
		*Next=First; if (Issue!=NULL) Issue->MonthsExamined=1; return XCRON_CALENDAR_FOUND;
	}
	if (!xcronCalendarMayExist(Expression)) return XCRON_CALENDAR_NONE;
	xdatetime Date=First;
	int FirstWeekday=(First.Weekday+7-((First.Day-1)%7))%7;
	for (uint32 Month=0; Month<XCRON_CALENDAR_MAX_MONTHS; ++Month) {
		if (Date.Year>End->Year || (Date.Year==End->Year && Date.Month>End->Month)) return XCRON_CALENDAR_NONE;
		if (Issue!=NULL) Issue->MonthsExamined=Month+1u;
		int Days=xrtDaysInMonth(Date.Year,Date.Month);
		if (Expression->Fields[XCRON_FIELD_MONTH]&((uint64)1u<<Date.Month)) {
			uint64 Mask=xcronCalendarDays(Expression,Days,FirstWeekday);
			for (int Day=xcronCalendarFind(Mask,Month==0 ? First.Day : 1); Day>=0; Day=xcronCalendarFind(Mask,Day+1)) {
				Date.Day=Day; bool Same=Month==0 && Day==First.Day;
				if (!xcronCalendarTime(Expression,Same ? First.Hour : 0,Same ? First.Minute : 0,Same ? First.Second : 0,&Date)) continue;
				if (xcronCalendarCompare(&Date,End)>0) return XCRON_CALENDAR_NONE;
				*Next=Date;
				return XCRON_CALENDAR_FOUND;
			}
		}
		FirstWeekday=(FirstWeekday+Days)%7;
		if (++Date.Month>12) { Date.Month=1; if (Date.Year==INT64_MAX) return XCRON_CALENDAR_NONE; if (++Date.Year==0) Date.Year=1; }
	}
	return XCRON_CALENDAR_NONE;
}

XCRON_API bool xcronCalendarMatch(const xcronexpression* Expression, xtime When, bool* Matches, xcroncalendarissue* Issue)
{
	if (!xcronCalendarArgs(Expression,NULL,NULL,0,Matches,sizeof(*Matches),Issue) || !xcronCalendarExpression(Expression,Issue)) return false;
	xdatetime Date; (void)xrtTimeSplit(When,&Date); *Matches=xcronCalendarPartsMatch(Expression,&Date);
	if (Issue!=NULL) *Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
	return true;
}

XCRON_API xcroncalendarstatus xcronCalendarNext(const xcronexpression* Expression, xtime Begin, xtime End, uint32 Flags, xtime* Next, xcroncalendarissue* Issue)
{
	if (!xcronCalendarArgs(Expression,NULL,NULL,Flags,Next,sizeof(*Next),Issue) || !xcronCalendarExpression(Expression,Issue)) return XCRON_CALENDAR_ERROR;
	xdatetime First, Last, Date; (void)xrtTimeSplit(Begin,&First); (void)xrtTimeSplit(End,&Last);
	xcroncalendarstatus Status=xcronCalendarSearch(Expression,&First,&Last,Flags,&Date,Issue);
	if (Status==XCRON_CALENDAR_FOUND) {
		/* Ordered bounds guarantee a representable candidate, including partial
		 * endpoint days; never attempt an unrepresentable midnight. */
		xtime Value;
		if (!xrtDateTime(Date.Year,Date.Month,Date.Day,Date.Hour,Date.Minute,Date.Second,0,&Value)) {
			xcronCalendarReport(XCRON_CALENDAR_RANGE,XCRON_FIELD_NONE,Issue,"cron candidate exceeds xtime range"); return XCRON_CALENDAR_ERROR;
		}
		*Next=Value;
	}
	return Status;
}

XCRON_API bool xcronCalendarMatchCivil(const xcronexpression* Expression, const xdatetime* When, bool* Matches, xcroncalendarissue* Issue)
{
	if (When==NULL) return xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"NULL cron civil input");
	if (!xcronCalendarArgs(Expression,When,NULL,0,Matches,sizeof(*Matches),Issue) || !xcronCalendarExpression(Expression,Issue)) return false;
	xdatetime Date; if (!xcronCalendarCivil(When,&Date,Issue)) return false;
	*Matches=xcronCalendarPartsMatch(Expression,&Date);
	if (Issue!=NULL) *Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,0};
	return true;
}

XCRON_API xcroncalendarstatus xcronCalendarNextCivil(const xcronexpression* Expression, const xdatetime* Begin, const xdatetime* End,
	uint32 Flags, xdatetime* Next, xcroncalendarissue* Issue)
{
	if (Begin==NULL || End==NULL) { xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"NULL cron civil window"); return XCRON_CALENDAR_ERROR; }
	if (!xcronCalendarArgs(Expression,Begin,End,Flags,Next,sizeof(*Next),Issue) || !xcronCalendarExpression(Expression,Issue)) return XCRON_CALENDAR_ERROR;
	xdatetime First, Last, Value;
	if (!xcronCalendarCivil(Begin,&First,Issue) || !xcronCalendarCivil(End,&Last,Issue)) return XCRON_CALENDAR_ERROR;
	xcroncalendarstatus Status=xcronCalendarSearch(Expression,&First,&Last,Flags,&Value,Issue);
	if (Status==XCRON_CALENDAR_FOUND) (void)xcronCalendarCivil(&Value,Next,NULL);
	return Status;
}

#define XCRON_CALENDAR_CURSOR_MAGIC UINT32_C(0x58434331)
typedef struct xcroncalendarimpl {
    uint32 Magic,Civil;
    uintptr_t Address;
    xcronexpression Expression;
    xcroncalendarvalue Search;
} xcroncalendarimpl;
typedef char xcron_calendar_cursor_size_check[(sizeof(xcroncalendarimpl)<=sizeof(xcroncalendarcursor))?1:-1];
/* Encode/decode through memcpy: no private-struct lvalue aliases opaque bytes. */
static bool xcronCalendarCursorInitValue(xcroncalendarcursor* C,const xcronexpression* E,
    const xdatetime* First,const xdatetime* Last,uint32 Flags,uint32 Civil,xcroncalendarissue* Issue)
{
    xcroncalendarimpl I;memset(&I,0,sizeof(I));I.Magic=XCRON_CALENDAR_CURSOR_MAGIC;I.Address=(uintptr_t)C;I.Civil=Civil;I.Expression=*E;
    xdatetime Rounded;xcroncalendarissue Work;xcroncalendarstatus Status=xcronCalendarSearchStart(&I.Expression,First,Last,Flags,&Rounded,&Work);
    if(Status==XCRON_CALENDAR_ERROR){if(Issue)*Issue=Work;return false;}
    xcronCalendarValueInit(&I.Search,&Rounded,Last,Status);memset(C,0,sizeof(*C));memcpy(C,&I,sizeof(I));if(Issue)*Issue=Work;return true;
}
XCRON_API bool xcronCalendarCursorInit(xcroncalendarcursor* C,const xcronexpression* E,xtime Begin,xtime End,uint32 Flags,xcroncalendarissue* Issue)
{
    if(!xcronCalendarArgs(E,NULL,NULL,Flags,C,sizeof(*C),Issue) || !xcronCalendarExpression(E,Issue))return false;
    xdatetime First,Last;(void)xrtTimeSplit(Begin,&First);(void)xrtTimeSplit(End,&Last);
    return xcronCalendarCursorInitValue(C,E,&First,&Last,Flags,0,Issue);
}
XCRON_API bool xcronCalendarCursorInitCivil(xcroncalendarcursor* C,const xcronexpression* E,const xdatetime* Begin,const xdatetime* End,uint32 Flags,xcroncalendarissue* Issue)
{
    if(!Begin || !End)return xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"NULL cursor civil window");
    if(!xcronCalendarArgs(E,Begin,End,Flags,C,sizeof(*C),Issue) || !xcronCalendarExpression(E,Issue))return false;
    xdatetime First,Last;if(!xcronCalendarCivil(Begin,&First,Issue) || !xcronCalendarCivil(End,&Last,Issue))return false;
    return xcronCalendarCursorInitValue(C,E,&First,&Last,Flags,1,Issue);
}
static xcroncalendarstatus xcronCalendarCursorStepValue(xcroncalendarcursor* C,uint32 Budget,void* Next,uint32 Civil,xcroncalendarissue* Issue)
{
    size_t Bytes=Civil?sizeof(xdatetime):sizeof(xtime);
    if(!C || !Next || !xrtMemRangeValid(C,sizeof(*C)) || !xrtMemRangeValid(Next,Bytes) ||
        (Issue && !xrtMemRangeValid(Issue,sizeof(*Issue))) || xrtMemRangesOverlap(C,sizeof(*C),Next,Bytes) ||
        (Issue && (xrtMemRangesOverlap(Issue,sizeof(*Issue),C,sizeof(*C)) || xrtMemRangesOverlap(Issue,sizeof(*Issue),Next,Bytes)))){
        xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,NULL,"invalid cursor spans/output aliases");return XCRON_CALENDAR_ERROR;
    }
    if(!Budget){xcronCalendarReport(XCRON_CALENDAR_ARGUMENT,XCRON_FIELD_NONE,Issue,"cursor month budget must be positive");return XCRON_CALENDAR_ERROR;}
    xcroncalendarimpl I;memcpy(&I,C,sizeof(I));
    if(I.Magic!=XCRON_CALENDAR_CURSOR_MAGIC || I.Address!=(uintptr_t)C || I.Civil!=Civil || I.Search.Months>XCRON_CALENDAR_MAX_MONTHS ||
        (I.Search.Status!=XCRON_CALENDAR_PENDING && I.Search.Status!=XCRON_CALENDAR_FOUND && I.Search.Status!=XCRON_CALENDAR_NONE)){
        xcronCalendarReport(XCRON_CALENDAR_STATE,XCRON_FIELD_NONE,Issue,"uninitialized/copied cursor or wrong step mode");return XCRON_CALENDAR_ERROR;
    }
    xcroncalendarstatus Status=xcronCalendarValueStep(&I.Expression,&I.Search,Budget);xdatetime Canonical;xtime Time=0;
    if(Status==XCRON_CALENDAR_FOUND){
        if(Civil){if(!xcronCalendarCivil(&I.Search.Date,&Canonical,Issue))return XCRON_CALENDAR_ERROR;}
        else if(!xrtDateTime(I.Search.Date.Year,I.Search.Date.Month,I.Search.Date.Day,I.Search.Date.Hour,I.Search.Date.Minute,I.Search.Date.Second,0,&Time)){
            xcronCalendarReport(XCRON_CALENDAR_RANGE,XCRON_FIELD_NONE,Issue,"cursor candidate exceeds timestamp range");return XCRON_CALENDAR_ERROR;
        }
    }
    memcpy(C,&I,sizeof(I));if(Status==XCRON_CALENDAR_FOUND){if(Civil)memcpy(Next,&Canonical,sizeof(Canonical));else memcpy(Next,&Time,sizeof(Time));}
    if(Issue)*Issue=(xcroncalendarissue){XCRON_CALENDAR_OK,XCRON_FIELD_NONE,I.Search.Months};
    return Status;
}
XCRON_API xcroncalendarstatus xcronCalendarCursorStep(xcroncalendarcursor* C,uint32 Budget,xtime* Next,xcroncalendarissue* Issue)
{return xcronCalendarCursorStepValue(C,Budget,Next,0,Issue);}
XCRON_API xcroncalendarstatus xcronCalendarCursorStepCivil(xcroncalendarcursor* C,uint32 Budget,xdatetime* Next,xcroncalendarissue* Issue)
{return xcronCalendarCursorStepValue(C,Budget,Next,1,Issue);}
#endif
