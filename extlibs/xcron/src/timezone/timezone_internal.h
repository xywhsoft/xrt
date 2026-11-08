#ifndef XCRON_TIMEZONE_INTERNAL_H
#define XCRON_TIMEZONE_INTERNAL_H
#include <xcron/timezone.h>
#include <xrt/atomic.h>
#include <limits.h>
#if defined(XCRON_FEATURE_XCRON_TIMEZONE)
#define XCRON_ZONE_MAGIC UINT32_C(0x58435a31)
typedef struct xcronzonehistory xcronzonehistory;
struct xcronzone {
    xatomic32 Refs;
    uint32 Magic;
    size_t Bytes;
    int32 Offsets[2];
    uint16 Names[2];
    uint8 Mode,HasDST;
    xcronzonehistory* History;
    /* 800 start/end UTC offsets for 2000..2399, then owned names (POSIX only). */
    int32 Changes[];
};
bool xcronZoneFail(xcronzoneerror Code,size_t Offset,xcronzoneissue* Issue);
bool xcronZoneCivilSeconds(const xdatetime* Date,int64* Seconds);
/* Pure span/owner/alias checks; callers publish their own single diagnostic. */
bool xcronZoneOutputs(const xcronzone* Zone,const void* Input,size_t InputSize,void* const* Outputs,const size_t* Sizes,size_t Count);
xcronzonestate xcronZoneStateValue(const xcronzone* Zone,xtime When);
void xcronZoneDecompose(xtime When,int32 Offset,bool DST,xdatetime* Date);
xcronzonestatus xcronZoneNextValue(const xcronzone* Zone,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue);
/* Product-private, borrowed-owner transition walk. Init does bounded history
 * lookup/constant setup; Step charges at most YearBudget additional rule years.
 * History directory lookup is logarithmic in the capped transition count.
 * PENDING is private: public synchronous zone APIs never return it. */
#define XCRON_ZONE_WALK_PENDING ((xcronzonestatus)2)
typedef struct xcronzonewalk {
    const xcronzone *Rules,*History;
    int64 Year,Day,Second,Best;
    uint32 Years;
    xcronzonestatus Status;
    bool Before,After;
    xcronzonetransition Result;
} xcronzonewalk;
void xcronZoneWalkInit(const xcronzone* Zone,xtime After,xcronzonewalk* Walk);
xcronzonestatus xcronZoneWalkStep(xcronzonewalk* Walk,uint32 YearBudget);
void xcronZoneHistoryWalkInit(const xcronzone* Zone,xtime After,xcronzonewalk* Walk);
void xcronZoneHistoryWalkResult(const xcronzone* Zone,xcronzonewalk* Walk);
/* Earliest >=Begin from which the complete zone repeats every Gregorian cycle.
 * False means no proven periodic suffix inside the xtime domain. */
bool xcronZoneRepeat(const xcronzone* Zone,xtime Begin,xtime* First);
bool xcronZoneHistoryRepeat(const xcronzone* Zone,xtime Begin,xtime* First);
xcronzonestate xcronZoneHistoryAt(const xcronzone* Zone,xtime When);
xcronzonestatus xcronZoneHistoryNext(const xcronzone* Zone,xtime After,xcronzonetransition* Transition,xcronzoneissue* Issue);
xcronzonestatus xcronZoneHistoryResolve(const xcronzone* Zone,const xdatetime* Date,xtime* Candidates,size_t Capacity,size_t* Count,xcronzoneissue* Issue);
bool xcronZoneHistoryOverlap(const xcronzone* Zone,const void* Output,size_t Size);
void xcronZoneHistoryRelease(const xcronzone* Zone);
/* Requested bytes owned by this zone, including an independently owned footer. */
#if defined(XCRON_FEATURE_XCRON_TZDB)
size_t xcronZoneOwnedBytes(const xcronzone* Zone);
#endif
/* Fraction stays nonnegative at the asymmetric signed-int64 millisecond ends. */
static inline bool xcronZoneSecondValue(int64 Seconds,int Milli,xtime* Out)
{
    int64 Minimum=INT64_MIN/1000-1,Maximum=INT64_MAX/1000;
    if(Seconds<Minimum || Seconds>Maximum)return false;
    if(Seconds==Minimum){int Limit=(int)(1000+INT64_MIN%1000);
        if(Milli<Limit)return false;
        *Out=INT64_MIN+(Milli-Limit);return true;
    }
    if(Seconds==Maximum && Milli>INT64_MAX%1000)return false;
    *Out=Seconds*1000+Milli;return true;
}
#endif
#endif
