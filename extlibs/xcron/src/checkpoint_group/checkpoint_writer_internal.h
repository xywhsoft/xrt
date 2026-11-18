#ifndef XCRON_CHECKPOINT_WRITER_INTERNAL_H
#define XCRON_CHECKPOINT_WRITER_INTERNAL_H
#include <xcron/checkpoint_writer.h>
#if defined(XCRON_FEATURE_XCRON_CHECKPOINT_WRITER)
/* Signal is a resident non-reentrant wake operation. It runs under the writer
 * mutex so successful collection also ends any use of its context. */
bool xcronCheckpointWriterSubmitSignal(xcroncheckpointwriter* Writer,uint64 ExpectedGeneration,
    const xcroncheckpoint* Records,uint32 Count,xcroncheckpointwriteticket* Ticket,
    void (*Signal)(ptr),ptr Context);
bool xcronCheckpointWriterOutside(const xcroncheckpointwriter* Writer,const void* Out,size_t Size);
bool xcronCheckpointWriterCurrent(const xcroncheckpointwriter* Writer);
#endif
#endif
