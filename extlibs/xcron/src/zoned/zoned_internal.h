#ifndef XCRON_ZONED_INTERNAL_H
#define XCRON_ZONED_INTERNAL_H
#include <xcron/zoned.h>
#if defined(XCRON_FEATURE_XCRON_ZONED)
/* Valid immutable expression/live zone, ordered window and disjoint valid
 * outputs are preconditions. Pure typed result; no ambient error or allocation. */
xcronzonedstatus xcronZonedNextValue(const xcronexpression* E,const xcronzone* Z,xtime Begin,xtime End,uint32 Flags,xtime* Next,xcronzonedissue* Issue);
#endif
#endif
