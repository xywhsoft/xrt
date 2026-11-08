#ifndef XCRON_SCHEDULER_INTERNAL_H
#define XCRON_SCHEDULER_INTERNAL_H
#include <xcron/scheduler.h>
#if defined(XCRON_FEATURE_XCRON_SCHEDULER_SELECTED)
#include <xcron/scheduler_selected.h>
#endif
#if defined(XCRON_FEATURE_XCRON_SCHEDULER)
/* Owner-only provisional admission for the live service. ID is internal until
 * Finish accepts. Pending/failed/cancelled admissions cannot dispatch or Reap.
 * Begin copies/retains into a bounded kernel slot, then queues only calculation.
 * Finish either commits it or releases the provisional slot/zone; business Data
 * remains the service command's duty. PollBudgeted performs all traversal. */
typedef enum xcronadmissionstatus {XCRON_ADMISSION_REJECTED=-1,XCRON_ADMISSION_PENDING=0,XCRON_ADMISSION_ACCEPTED=1} xcronadmissionstatus;
bool xcronSchedulerBeginAdmission(xcronscheduler* S,const xcronjobconfig* C,xcronjobid* Id,xcronschedulerissue* Issue);
xcronadmissionstatus xcronSchedulerFinishAdmission(xcronscheduler* S,xcronjobid Id,bool Cancel,xcronschedulerissue* Issue);
#endif
#endif
