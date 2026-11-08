#ifndef XCRON_OCCURRENCE_INTERNAL_H
#define XCRON_OCCURRENCE_INTERNAL_H
#include <xcron/occurrence.h>
#include "../zoned/zoned_internal.h"
#if defined(XCRON_FEATURE_XCRON_OCCURRENCE)
/* Product-private typed engine; immutable validated inputs, no diagnostics or
 * retention. Reuse Search's dormant calendar/walk state for gap validation. */
typedef struct xcronoccurrencevalue {
    xcronzonedvalue Search;
    xcronzoneresolution Resolve;
    xcronoccurrence Candidate;
    xcronoccurrenceissue Work;
    xtime First,Limit,Bound,After;
    xcrondeliverypolicy Policy;
    uint32 Phase;
    xcronoccurrencestatus Outcome,Status;
    xcronoccurrenceerror OutcomeCode;
    xcronzoneerror OutcomeZone;
} xcronoccurrencevalue;
void xcronOccurrenceValueInit(const xcronexpression* Expression,const xcronzone* Zone,xtime Begin,xtime End,uint32 Flags,xcrondeliverypolicy Policy,xcronoccurrencevalue* Value);
xcronoccurrencestatus xcronOccurrenceValueStep(xcronoccurrencevalue* Value,const xcronzone* Zone,xcronoccurrencebudget Budget);
#endif
#endif
