#ifndef XCRON_DELIVERY_H
#define XCRON_DELIVERY_H
#include <xcron/features.h>
#include <xcron/export.h>
#include <xcron/timezone.h>
#if defined(XCRON_FEATURE_XCRON_DELIVERY) && !defined(XCRON_FEATURE_XCRON_TIMEZONE)
#error "cron delivery requires timezone"
#endif
#if defined(XCRON_FEATURE_XCRON_DELIVERY)
typedef enum xcronfoldpolicy {XCRON_FOLD_ALL=0,XCRON_FOLD_FIRST,XCRON_FOLD_LAST,XCRON_FOLD_REJECT} xcronfoldpolicy;
typedef enum xcronmissingpolicy {XCRON_MISSING_SKIP=0,XCRON_MISSING_REJECT} xcronmissingpolicy;
typedef struct xcrondeliverypolicy {xcronfoldpolicy Fold;xcronmissingpolicy Missing;} xcrondeliverypolicy;
typedef enum xcrondeliverystatus {XCRON_DELIVERY_CAPACITY=-2,XCRON_DELIVERY_ERROR=-1,XCRON_DELIVERY_NONE=0,XCRON_DELIVERY_FOUND=1} xcrondeliverystatus;
typedef enum xcrondeliveryerror {XCRON_DELIVERY_OK=0,XCRON_DELIVERY_ARGUMENT=8001,XCRON_DELIVERY_ZONE,XCRON_DELIVERY_LIMIT,XCRON_DELIVERY_AMBIGUOUS,XCRON_DELIVERY_MISSING} xcrondeliveryerror;
/* Candidates is the complete actual multiplicity, before selection, when known.
 * Zone applies to ZONE errors. Count below is the SELECTED required capacity. */
typedef struct xcrondeliveryissue {xcrondeliveryerror Code;xcronzoneerror Zone;size_t Candidates;} xcrondeliveryissue;
XRT_EXTERN_C_BEGIN
/* Resolve ONE floating nominal civil label, then apply explicit policies to its
 * COMPLETE ascending candidate set. ALL keeps every candidate (including more
 * than two); FIRST/LAST keep the global earliest/latest, not the first one within
 * a later query window. REJECT fails AMBIGUOUS when multiplicity exceeds one.
 * Offset/IsDST/derived civil fields are ignored as in ZoneResolve. Fractions and
 * the full signed-int64 timestamp domain are supported. Missing SKIP returns
 * NONE/Count0; REJECT fails MISSING. Missing includes an otherwise valid civil
 * label outside the representable timestamp domain. UNKNOWN coverage remains a
 * typed ZONE/UNKNOWN error even if a subset of known candidates might suffice.
 * No silent UTC fallback, gap shifting, expression matching, recurrence search,
 * clock/misfire/overlap/executor/storage or existing-job reconfiguration occurs.
 * Those scheduling/delivery stages remain separate and developing.
 *
 * Caller holds the exact immutable Zone and stable Local for the synchronous
 * call. No retention, mutation, heap/IO/lock on normal FOUND/NONE/CAPACITY.
 * Capacity0/CandidatesNULL is a sizing query. FOUND writes Count and selected
 * candidates; CAPACITY writes required selected Count/Issue and leaves the
 * candidate array intact. ERROR preserves Count/array. Invalid structural args,
 * policies/spans/aliases preserve EVERY output, including Issue. Other errors
 * publish typed Issue independently of diagnostic allocation. All output spans
 * must avoid Local, the whole zone/footer ownership graph and one another.
 * Normal results preserve the ambient error; diagnostics may allocate.
 *
 * ALL+SKIP delegates one original complete Resolve directly. Other policies
 * use one258-timestamp stack scratch, in addition to Resolve's own temporary,
 * and copy only selected results after checking capacity. Complete resolution
 * still enumerates known offset types and checks unknown coverage; this is a
 * synchronous calculation primitive, not a bounded owner-turn search budget.
 * Existing actual-occurrence APIs and scheduler semantics are unchanged.
 * Error domain: xrt.cron.delivery. */
XCRON_API xcrondeliverystatus xcronDeliveryResolve(const xcronzone* Zone,const xdatetime* Local,xcrondeliverypolicy Policy,
    xtime* Candidates,size_t Capacity,size_t* Count,xcrondeliveryissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
