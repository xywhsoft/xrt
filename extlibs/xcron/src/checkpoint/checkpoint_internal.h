#ifndef XCRON_CHECKPOINT_INTERNAL_H
#define XCRON_CHECKPOINT_INTERNAL_H
#include <xcron/checkpoint.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT)
typedef enum xcroncheckpointstatus {
    XCRON_CHECKPOINT_VALID, XCRON_CHECKPOINT_INVALID, XCRON_CHECKPOINT_INCOMPATIBLE
} xcroncheckpointstatus;

xcroncheckpointstatus xcronCheckpointValidate(xbytesview Wire,xcroncheckpointkey Key);
void xcronCheckpointEncode(const xcronjobconfig* Config,const xcronjobinfo* Info,bool Selected,
    xcrondeliverypolicy Policy,xcroncheckpointkey Key,xcroncheckpoint* Out);
xcroncheckpointstatus xcronCheckpointDecode(xbytesview Wire,const xcronjobconfig* Config,
    bool Selected,xcroncheckpointkey Key,xcronjobinfo* Info,xcrondeliverypolicy* Policy);
#endif
#endif
