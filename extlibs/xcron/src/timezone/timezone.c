#include <xcron/timezone.h>
#include "timezone_internal.h"
#include <xrt/atomic.h>
#include <xrt/error.h>
#include <xrt/memory.h>
#include <limits.h>
#include <string.h>
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
typedef struct xcronzonerule {unsigned Kind,A,B,C;
    int32 Time;
} xcronzonerule;
typedef struct xcronzoneparser {xstrview Text;
    size_t At;
    xcronzonegrammar Grammar;
    xcronzoneissue* Issue;
} xcronzoneparser;
bool xcronZoneFail(xcronzoneerror Code,size_t Offset,xcronzoneissue* Issue)
{
    if(Issue)*Issue=(xcronzoneissue){Code,Offset,0};
    xrtSetErrorInfo(Code==XCRON_ZONE_ARGUMENT?XERR_ARGUMENT:Code==XCRON_ZONE_MEMORY?XERR_MEMORY:Code==XCRON_ZONE_LIMIT?XERR_RANGE:Code==XCRON_ZONE_UNKNOWN?XERR_STATE:XERR_VALUE,"xrt.cron.zone",Code,
    Code==XCRON_ZONE_RULES_REQUIRED?"explicit DST rules are required":"invalid cron timezone input or value");
    return false;
}
static int64 xcronZoneFloor(int64 N,int64 D){int64 Q=N/D;
    return Q-(N%D<0);
}
static int64 xcronZoneYearStep(int64 Year,int Step){Year+=Step;
    if(!Year)Year+=Step;
    return Year;
}
static unsigned xcronZoneCycle(int64 Year){int64 N=(Year<0?Year+1:Year)%400;
    return (unsigned)(N<0?N+400:N);
}
static bool xcronZoneName(xcronzoneparser* P,xstrview* Name)
{
    bool Quoted=P->At<P->Text.Size && P->Text.Data[P->At]=='<';
    if(Quoted)++P->At;
    size_t Start=P->At;
    while(P->At<P->Text.Size){unsigned char C=(unsigned char)P->Text.Data[P->At];
        bool Alpha=(C>='A' && C<='Z') || (C>='a' && C<='z');
        if(!Alpha && !(Quoted && ((C>='0' && C<='9') || C=='+' || C=='-')))break;
        ++P->At;
    }
    size_t Size=P->At-Start;
    if(Size<3 || Size>XCRON_ZONE_NAME_MAX || (Quoted && (P->At==P->Text.Size || P->Text.Data[P->At++]!='>')))return xcronZoneFail(Size>XCRON_ZONE_NAME_MAX?XCRON_ZONE_LIMIT:XCRON_ZONE_SYNTAX,Start,P->Issue);
    *Name=(xstrview){P->Text.Data+Start,Size};
    return true;
}
static bool xcronZoneNumber(xcronzoneparser* P,unsigned Limit,unsigned* Out)
{
    size_t Start=P->At;
    unsigned N=0;
    while(P->At<P->Text.Size && P->Text.Data[P->At]>='0' && P->Text.Data[P->At]<='9'){
        unsigned D=(unsigned)(P->Text.Data[P->At]-'0');
        if(N>Limit/10 || (N==Limit/10 && D>Limit%10))return xcronZoneFail(XCRON_ZONE_SYNTAX,P->At,P->Issue);
        N=N*10+D;
        ++P->At;
    }
    if(P->At==Start)return xcronZoneFail(XCRON_ZONE_SYNTAX,Start,P->Issue);
    *Out=N;
    return true;
}
static bool xcronZoneClock(xcronzoneparser* P,bool Transition,int32* Out)
{
    int Sign=1;
    size_t Start=P->At;
    bool Signed=P->At<P->Text.Size && (P->Text.Data[P->At]=='+' || P->Text.Data[P->At]=='-');
    if(Signed){if(Transition && P->Grammar==XCRON_ZONE_POSIX)return xcronZoneFail(XCRON_ZONE_SYNTAX,Start,P->Issue);
        if(P->Text.Data[P->At]=='-')Sign=-1;
        ++P->At;
    }
    unsigned H,M=0,S=0;
    if(!xcronZoneNumber(P,Transition && P->Grammar==XCRON_ZONE_TZIF3?167:24,&H))return false;
    if(P->At<P->Text.Size && P->Text.Data[P->At]==':'){++P->At;
        if(!xcronZoneNumber(P,59,&M))return false;
        if(P->At<P->Text.Size && P->Text.Data[P->At]==':'){++P->At;
            if(!xcronZoneNumber(P,59,&S))return false;
    }}
    *Out=Sign*(int32)(H*3600+M*60+S);
    return true;
}
static bool xcronZoneRule(xcronzoneparser* P,xcronzonerule* R)
{
    *R=(xcronzonerule){0,0,0,0,7200};
    if(P->At<P->Text.Size && P->Text.Data[P->At]=='M'){
        ++P->At;
        R->Kind=2;
        if(!xcronZoneNumber(P,12,&R->A) || !R->A)goto Invalid;
        if(P->At==P->Text.Size || P->Text.Data[P->At++]!='.' || !xcronZoneNumber(P,5,&R->B) || !R->B)goto Invalid;
        if(P->At==P->Text.Size || P->Text.Data[P->At++]!='.' || !xcronZoneNumber(P,6,&R->C))goto Invalid;
    }else {if(P->At<P->Text.Size && P->Text.Data[P->At]=='J'){++P->At;
            R->Kind=1;
        }
        if(!xcronZoneNumber(P,365,&R->A) || (R->Kind && !R->A))goto Invalid;
    }
    if(P->At<P->Text.Size && P->Text.Data[P->At]=='/'){++P->At;
        return xcronZoneClock(P,true,&R->Time);
    }return true;
    Invalid:return xcronZoneFail(XCRON_ZONE_SYNTAX,P->At,P->Issue);
}
static int32 xcronZoneRuleSecond(unsigned Cycle,const xcronzonerule* R,int32 Before)
{
    int64 Year=2000+(int64)Cycle;
    bool Leap=xrtDaysInYear(Year)==366;
    unsigned Day=R->A;
    if(R->Kind==1)Day=R->A-1+(Leap && R->A>=60);
    else if(R->Kind==2){xtime T;
        xdatetime D;
        (void)xrtDate(Year,(int)R->A,1,&T);
        (void)xrtTimeSplit(T,&D);
        unsigned Dom=1+(R->C+7-(unsigned)D.Weekday)%7+7*(R->B-1);
        if(Dom>(unsigned)xrtDaysInMonth(Year,(int)R->A))Dom-=7;
        Day=Dom-1;
        for(unsigned M=1;M<R->A;++M)Day+=(unsigned)xrtDaysInMonth(Year,(int)M);
    }
    return (int32)Day*86400+R->Time-Before;
}
static bool xcronZoneHandle(const xcronzone* Z)
{return Z && xrtMemRangeValid(Z,sizeof(*Z)) && Z->Magic==XCRON_ZONE_MAGIC;
}
bool xcronZoneOutputs(const xcronzone* Z,const void* Input,size_t InputSize,void* const* Outputs,const size_t* Sizes,size_t Count)
{
    if(!xcronZoneHandle(Z) || (Input && !xrtMemRangeValid(Input,InputSize)))return false;
    for(size_t I=0;I<Count;++I){if(!xrtMemRangeValid(Outputs[I],Sizes[I]) || xrtMemRangesOverlap(Outputs[I],Sizes[I],Z,Z->Bytes) || (Z->History && xcronZoneHistoryOverlap(Z,Outputs[I],Sizes[I])) || xrtMemRangesOverlap(Outputs[I],Sizes[I],Input,InputSize))return false;
        for(size_t J=0;J<I;++J)if(xrtMemRangesOverlap(Outputs[I],Sizes[I],Outputs[J],Sizes[J]))return false;
    }return true;
}
XCRON_API bool xcronZoneCreatePosix(xstrview Text,xcronzonegrammar Grammar,xcronzone** Out,xcronzoneissue* Issue)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (!Text.Data && Text.Size) || !xrtMemRangeValid(Text.Data,Text.Size) ||
    (Issue && !xrtMemRangeValid(Issue,sizeof(*Issue))) || xrtMemRangesOverlap(Out,sizeof(*Out),Text.Data,Text.Size) ||
    (Issue && (xrtMemRangesOverlap(Issue,sizeof(*Issue),Text.Data,Text.Size) || xrtMemRangesOverlap(Issue,sizeof(*Issue),Out,sizeof(*Out)))) ||
    (Grammar!=XCRON_ZONE_POSIX && Grammar!=XCRON_ZONE_TZIF3))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    if(Text.Size>XCRON_ZONE_TEXT_MAX)return xcronZoneFail(XCRON_ZONE_LIMIT,XCRON_ZONE_TEXT_MAX,Issue);
    xcronzoneparser P={Text,0,Grammar,Issue};
    xstrview Names[2]={{0}};
    int32 Offsets[2]={0};
    xcronzonerule Rules[2];
    bool DST=false;
    if(!xcronZoneName(&P,&Names[0]) || !xcronZoneClock(&P,false,&Offsets[0]))return false;
    Offsets[0]=-Offsets[0];
    if(P.At<Text.Size){DST=true;
        if(!xcronZoneName(&P,&Names[1]))return false;
        Offsets[1]=Offsets[0]+3600;
        if(P.At<Text.Size && Text.Data[P.At]!=','){if(!xcronZoneClock(&P,false,&Offsets[1]))return false;
            Offsets[1]=-Offsets[1];
        }
        if(P.At==Text.Size)return xcronZoneFail(XCRON_ZONE_RULES_REQUIRED,P.At,Issue);
        if(Text.Data[P.At++]!=',' || !xcronZoneRule(&P,&Rules[0]) || P.At==Text.Size || Text.Data[P.At++]!=',' || !xcronZoneRule(&P,&Rules[1]))return xcronZoneFail(XCRON_ZONE_SYNTAX,P.At,Issue);
    }
    if(P.At!=Text.Size)return xcronZoneFail(XCRON_ZONE_SYNTAX,P.At,Issue);
    size_t Bytes=sizeof(xcronzone)+(DST?800*sizeof(int32):0)+Names[0].Size+1+(DST?Names[1].Size+1:0);
    xcronzone* Z=xrtCalloc(1,Bytes);
    if(!Z)return xcronZoneFail(XCRON_ZONE_MEMORY,P.At,Issue);
    xrtAtomic32Init(&Z->Refs,1);
    Z->Magic=XCRON_ZONE_MAGIC;
    Z->Bytes=Bytes;
    Z->Offsets[0]=Offsets[0];
    Z->Offsets[1]=Offsets[1];
    Z->HasDST=(uint8)DST;
    Z->Mode=DST?2:0;
    Z->Names[0]=(uint16)Names[0].Size;
    Z->Names[1]=(uint16)Names[1].Size;
    char* N=(char*)(Z->Changes+(DST?800:0));
    memcpy(N,Names[0].Data,Names[0].Size);
    if(DST)memcpy(N+Names[0].Size+1,Names[1].Data,Names[1].Size);
    if(DST){bool AlwaysSTD=true,AlwaysDST=true;
        for(unsigned Y=0;Y<400;++Y){Z->Changes[Y*2]=xcronZoneRuleSecond(Y,&Rules[0],Offsets[0]);
            Z->Changes[Y*2+1]=xcronZoneRuleSecond(Y,&Rules[1],Offsets[1]);
            if(Z->Changes[Y*2]!=Z->Changes[Y*2+1])AlwaysSTD=false;
            int64 Next=(int64)xrtDaysInYear(2000+Y)*86400+xcronZoneRuleSecond((Y+1)%400,&Rules[0],Offsets[0]);
            if(Z->Changes[Y*2+1]!=Next)AlwaysDST=false;
        }
        if(AlwaysSTD)Z->Mode=0;
        else if(AlwaysDST)Z->Mode=1;
    }
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,Text.Size,DST?400:0};
    *Out=Z;
    return true;
}
XCRON_API bool xcronZoneRetain(const xcronzone* Zone)
{
    if(!xcronZoneHandle(Zone))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    xcronzone* Z=(xcronzone*)Zone;
    uint32 N=xrtAtomic32Load(&Z->Refs,XMEMORY_RELAXED);
    while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&Z->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    return xcronZoneFail(XCRON_ZONE_LIMIT,0,NULL);
}
XCRON_API void xcronZoneRelease(const xcronzone* Zone)
{xcronzone* Z=(xcronzone*)Zone;
    if(Z && xrtAtomic32FetchSub(&Z->Refs,1,XMEMORY_ACQ_REL)==1){Z->Magic=0;
        if(Z->History)xcronZoneHistoryRelease(Z);
        xrtFree(Z);
}}
static bool xcronZoneLookup(const xcronzone* Z,xtime When)
{
    if(Z->Mode!=2)return Z->Mode==1;
    xdatetime D;
    (void)xrtTimeSplit(When,&D);
    int64 Day=xcronZoneFloor(When,XRT_TIME_DAY)-(D.YearDay-1),Year=D.Year;
    for(unsigned I=0;I<2;++I){Year=xcronZoneYearStep(Year,-1);
        Day-=xrtDaysInYear(Year);
    }int64 Best=INT64_MIN,Second=xcronZoneFloor(When,1000);
    bool DST=false;
    /* A rule can carry less than nine days across its nominal year. The
     * surrounding two years therefore contain the latest applicable event.
     * At ties, a later rule-year start wins; same-year end wins over start. */
    for(unsigned Y=0;Y<5;++Y){unsigned Cycle=xcronZoneCycle(Year);
        for(unsigned K=0;K<2;++K){int64 T=Day*86400+Z->Changes[Cycle*2+K];
            if(T<=Second && T>=Best){Best=T;
                DST=K==0;
        }}
        Day+=xrtDaysInYear(Year);
        Year=xcronZoneYearStep(Year,1);
    }return DST;
}
static xcronzonestate xcronZoneState(const xcronzone* Z,bool DST)
{const char* Names=(const char*)(Z->Changes+(Z->HasDST?800:0));
    return (xcronzonestate){Z->Offsets[DST?1:0],DST,{Names+(DST?Z->Names[0]+1:0),Z->Names[DST?1:0]}};
}
xcronzonestate xcronZoneStateValue(const xcronzone* Z,xtime When)
{return Z->History?xcronZoneHistoryAt(Z,When):xcronZoneState(Z,xcronZoneLookup(Z,When));}
bool xcronZoneRepeat(const xcronzone* Z,xtime Begin,xtime* First)
{if(Z->History)return xcronZoneHistoryRepeat(Z,Begin,First);*First=Begin;return true;}
XCRON_API bool xcronZoneAt(const xcronzone* Z,xtime When,xcronzonestate* State,xcronzoneissue* Issue)
{
    void* Outputs[]={State,Issue};
    size_t Sizes[]={State?sizeof(*State):0,Issue?sizeof(*Issue):0};
    if(!State || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    xcronzonestate Value=xcronZoneStateValue(Z,When);
    if(Value.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZoneFail(XCRON_ZONE_UNKNOWN,0,Issue);
    *State=Value;
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,!Z->History && Z->Mode==2?5:0};
    return true;
}
void xcronZoneDecompose(xtime When,int32 Offset,bool DST,xdatetime* Out)
{
    int64 Seconds=xcronZoneFloor(When,1000),Milli=When%1000;
    if(Milli<0)Milli+=1000;
    /* XRT's epoch is 0001-01-01, not the Unix epoch. Shift whole Gregorian
    * cycles to 2000..2399 before calling the public core decomposition. */
    const int64 Anchor=XRT_TIME_UNIX_EPOCH/XRT_TIME_DAY+10957;
    int64 Local=Seconds+Offset,Day=xcronZoneFloor(Local,86400),TOD=Local-Day*86400,Relative=Day-Anchor,Cycles=xcronZoneFloor(Relative,146097);
    xtime Safe=(Anchor+Relative-Cycles*146097)*XRT_TIME_DAY+TOD*1000+Milli;
    (void)xrtTimeSplit(Safe,Out);
    int64 Year=Out->Year+Cycles*400;
    Out->Year=Year>0?Year:Year-1;
    Out->Offset=Offset;
    Out->IsDST=DST?1:0;
}
XCRON_API bool xcronZoneSplit(const xcronzone* Z,xtime When,xdatetime* Date,xcronzoneissue* Issue)
{
    void* Outputs[]={Date,Issue};
    size_t Sizes[]={Date?sizeof(*Date):0,Issue?sizeof(*Issue):0};
    if(!Date || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2))return xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
    xcronzonestate State=xcronZoneStateValue(Z,When);
    if(State.Offset==XCRON_ZONE_OFFSET_UNKNOWN)return xcronZoneFail(XCRON_ZONE_UNKNOWN,0,Issue);
    xcronZoneDecompose(When,State.Offset,State.IsDST,Date);
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,!Z->History && Z->Mode==2?5:0};
    return true;
}
static bool xcronZoneCivil(const xdatetime* D)
{return D->Year && D->Month>=1 && D->Month<=12 && D->Day>=1 && D->Day<=xrtDaysInMonth(D->Year,D->Month) && D->Hour>=0 && D->Hour<=23 && D->Minute>=0 && D->Minute<=59 && D->Second>=0 && D->Second<=59 && D->Millisecond>=0 && D->Millisecond<=999;
}
bool xcronZoneCivilSeconds(const xdatetime* D,int64* Seconds)
{
    if(D->Year<-400000000 || D->Year>400000000)return false;
    int64 Year=D->Year<0?D->Year+1:D->Year,Cycles=xcronZoneFloor(Year,400),SafeYear=2000+Year-Cycles*400;
    xtime Safe;
    (void)xrtDateTime(SafeYear,D->Month,D->Day,D->Hour,D->Minute,D->Second,0,&Safe);
    *Seconds=Safe/1000+(Cycles-5)*INT64_C(12622780800);
    return true;
}
static bool xcronZoneInstant(const xdatetime* D,int32 Offset,xtime* Out)
{int64 Seconds;return xcronZoneCivilSeconds(D,&Seconds) && xcronZoneSecondValue(Seconds-Offset,D->Millisecond,Out);}
XCRON_API xcronzonestatus xcronZoneResolve(const xcronzone* Z,const xdatetime* Local,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue)
{
    void* Outputs[]={Candidates,Count,Issue};
    size_t Sizes[]={Capacity<=SIZE_MAX/sizeof(xtime)?Capacity*sizeof(xtime):0,Count?sizeof(*Count):0,Issue?sizeof(*Issue):0};
    if(!Local || !Count || Capacity>SIZE_MAX/sizeof(xtime) || (!Candidates && Capacity) || !xcronZoneOutputs(Z,Local,sizeof(*Local),Outputs,Sizes,3)){xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
        return XCRON_ZONE_ERROR;
    }
    if(!xcronZoneCivil(Local)){xcronZoneFail(XCRON_ZONE_DATE,0,Issue);
        return XCRON_ZONE_ERROR;
    }
    if(Z->History)return xcronZoneHistoryResolve(Z,Local,Candidates,Capacity,Count,Issue);
    xtime Values[2];
    size_t N=0;
    for(unsigned K=0;K<(Z->HasDST?2u:1u);++K){xtime T;
        if(xcronZoneInstant(Local,Z->Offsets[K],&T) && Z->Offsets[xcronZoneLookup(Z,T)?1:0]==Z->Offsets[K] && (!N || Values[0]!=T))Values[N++]=T;
    }
    if(N==2 && Values[0]>Values[1]){xtime T=Values[0];
        Values[0]=Values[1];
        Values[1]=T;
    }*Count=N;
    if(Issue)*Issue=(xcronzoneissue){N>Capacity?XCRON_ZONE_LIMIT:XCRON_ZONE_OK,0,Z->Mode==2?10:0};
    if(N>Capacity)return XCRON_ZONE_CAPACITY;
    if(N)memcpy(Candidates,Values,N*sizeof(xtime));
    return N?XCRON_ZONE_FOUND:XCRON_ZONE_NONE;
}
XCRON_API xcronzonestatus xcronZoneNextTransition(const xcronzone* Z,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue)
{
    void* Outputs[]={Transition,Issue};
    size_t Sizes[]={Transition?sizeof(*Transition):0,Issue?sizeof(*Issue):0};
    if(!Transition || !xcronZoneOutputs(Z,NULL,0,Outputs,Sizes,2)){
        xcronZoneFail(XCRON_ZONE_ARGUMENT,0,NULL);
        return XCRON_ZONE_ERROR;
    }
    return xcronZoneNextValue(Z,After,Transition,Issue);
}
xcronzonestatus xcronZoneNextValue(const xcronzone* Z,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue)
{
    if(Issue)*Issue=(xcronzoneissue){XCRON_ZONE_OK,0,0};
    if(Z->History)return xcronZoneHistoryNext(Z,After,Transition,Issue);
    if(Z->Mode!=2)return XCRON_ZONE_NONE;
    xdatetime D;
    (void)xrtTimeSplit(After,&D);
    int64 Year=xcronZoneYearStep(D.Year,-1),Day=xcronZoneFloor(After,XRT_TIME_DAY)-(D.YearDay-1)-xrtDaysInYear(Year),Second=xcronZoneFloor(After,1000);
    int64 Best=INT64_MAX;
    bool Before=false,Next=false;
    /* UTC-year-1 through UTC-year+401 covers every Gregorian pattern and
     * cross-year carry. 700000s exceeds 167:59:59 + the largest offset (93599).
     * Compare actual states across ties before reporting an effective change. */
    for(unsigned Y=0;Y<403;++Y){if(Issue)Issue->YearsExamined=Y+1;
        unsigned C=xcronZoneCycle(Year);
        for(unsigned K=0;K<2;++K){int64 T=Day*86400+Z->Changes[C*2+K];
            if(T<=Second || T>=Best || T>INT64_MAX/1000)continue;
            bool A=xcronZoneLookup(Z,T*1000),B=xcronZoneLookup(Z,T*1000-1);
            if(A!=B){Best=T;
                Before=B;
                Next=A;
        }}
        Day+=xrtDaysInYear(Year);
        Year=xcronZoneYearStep(Year,1);
        if(Best!=INT64_MAX && Day*86400-700000>Best)break;
        if(Day*86400-700000>INT64_MAX/1000)break;
    }
    if(Best==INT64_MAX)return XCRON_ZONE_NONE;
    *Transition=(xcronzonetransition){Best*1000,xcronZoneState(Z,Before),xcronZoneState(Z,Next)};
    return XCRON_ZONE_FOUND;
}
#endif
