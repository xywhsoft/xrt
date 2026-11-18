#ifndef XCRON_CHECKPOINT_INTERNAL_H
#define XCRON_CHECKPOINT_INTERNAL_H
#include <xcron/checkpoint.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT)
typedef enum xcroncheckpointstatus {
    XCRON_CHECKPOINT_VALID, XCRON_CHECKPOINT_INVALID, XCRON_CHECKPOINT_INCOMPATIBLE
} xcroncheckpointstatus;

enum { XCRON_CHECKPOINT_CRC_OFFSET=XCRON_CHECKPOINT_BYTES-4 };

static inline void xcronCheckpointPut(uint8* Out,uint64 Value,size_t Bytes)
{
    for(size_t I=0;I<Bytes;++I){Out[I]=(uint8)Value;Value>>=8;}
}

static inline uint64 xcronCheckpointGet(const uint8* In,size_t Bytes)
{
    uint64 Value=0;
    for(size_t I=Bytes;I;--I)Value=(Value<<8)|In[I-1];
    return Value;
}

/* Envelope validation for fixed-size records; semantic config and instant
 * validation still belongs to scheduler restore. */
bool xcronCheckpointIdentify(xbytesview Wire,xcroncheckpointkey* Key);
xcroncheckpointstatus xcronCheckpointValidate(xbytesview Wire,xcroncheckpointkey Key);
void xcronCheckpointEncode(const xcronjobconfig* Config,const xcronjobinfo* Info,bool Selected,
    xcrondeliverypolicy Policy,xcroncheckpointkey Key,xcroncheckpoint* Out);
xcroncheckpointstatus xcronCheckpointDecode(xbytesview Wire,const xcronjobconfig* Config,
    bool Selected,xcroncheckpointkey Key,xcronjobinfo* Info,xcrondeliverypolicy* Policy);
#endif
#endif
