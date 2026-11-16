#ifndef XTP_READER_H
#define XTP_READER_H
#include <xtp/features.h>
#include <xtp/codec.h>
#if defined(XTP_FEATURE_XTP_READER)
typedef struct xtpreader xtpreader;
typedef enum xtpinputmode { XTP_INPUT_STREAM=0,XTP_INPUT_RECORD=1 } xtpinputmode;
typedef enum xtpflow { XTP_FLOW_ACCEPT=1,XTP_FLOW_PAUSE=2,XTP_FLOW_STOP=3 } xtpflow;
/* Normal realloc alignment/preservation; Size=0 frees and returns NULL.
 * Context/code live until Destroy. NULL selects public XRT memory functions.
 * Allocator callbacks must not reenter the instance. */
typedef void* (*xtpreallocproc)(void* User,void* Memory,size_t Size);
typedef struct xtpreaderconfig {
    xtpcodecconfig Codec;xtpinputmode Mode;
    size_t RetainBytes,MaxStorageBytes,MaxWorkBytes,MaxFrames;
    uint32_t MaxConsecutiveBad,MaxBad;
    uint64_t ReceiveTimeoutUs;
    xtpreallocproc Realloc;void* AllocatorUser;
} xtpreaderconfig;
typedef struct xtpreaderevents {
    /* Borrow valid only in callback. No recursive Input/End/Reset/Destroy.
     * PAUSE means no application effects; complete borrowed frame remains in
     * the caller's unconsumed suffix. Buffered paused frames remain in reader.
     * ACCEPT consumes exactly that frame; STOP consumes it and closes reader. */
    xtpflow (*Frame)(xtpreader*,const xtpframeview*,void* User);
    void (*Drop)(xtpreader*,xtpresult Reason,void* User);
} xtpreaderevents;
typedef struct xtpreadprogress {size_t Used,Frames,Dropped,WorkBytes;} xtpreadprogress;
typedef struct xtpreaderinfo {
    size_t BufferedBytes,Capacity,StorageBytes,PeakReservedBytes;
    uint64_t Frames,Dropped,NextDeadlineUs;
    uint32_t ConsecutiveBad;
    xtpresult Failure;bool Paused,Ended,DiscardingRecord;
} xtpreaderinfo;
XRT_EXTERN_C_BEGIN
/* Default STREAM;64KiB retention,3MiB storage,256KiB work,64 frames,
 * 8 consecutive/64 total bad frames,5s absolute frame receive deadline. */
XTP_API void xtpReaderConfigInit(xtpreaderconfig* Config);
/* Copies config/events. Only Create and fragmented-buffer growth allocate.
 * No socket/thread/timer/global state. One owner; failure preserves output. */
XTP_API xtpresult xtpReaderCreate(const xtpreaderconfig* Config,
    const xtpreaderevents* Events,void* User,xtpreader** Reader);
XTP_API xtpresult xtpReaderDestroy(xtpreader* Reader);
/* Reports Used on every valid call including a failing prefix. Progress itself
 * must not overlap input/reader storage. Input must not alias internal buffers.
 * Empty input is NOT EOF. RECORD End applies only after this input was consumed;
 * after AGAIN/PAUSED retry its unconsumed suffix with the same End flag.
 * OK=boundary,MORE=partial,AGAIN=work budget,PAUSED=explicit resume required,
 * DROPPED=recoverable record/frame discard. Fatal errors stick until Reset.
 * Budget counts copies,discarded bytes and prefix validation,not opaque borrowed
 * Body bytes. No unchecked magic search or body allocation for invalid prefixes. */
XTP_API xtpresult xtpReaderInput(xtpreader* Reader,xbytesview Input,bool RecordEnd,
    uint64_t NowUs,xtpreadprogress* Progress);
XTP_API xtpresult xtpReaderResume(xtpreader* Reader);
/* End is receive EOF; partial STREAM is sticky TRUNCATED. EndRecord is supplied
 * to Input, not End. Reset is allowed only at a trusted new stream/boundary. */
XTP_API xtpresult xtpReaderEnd(xtpreader* Reader,uint64_t NowUs);
XTP_API xtpresult xtpReaderReset(xtpreader* Reader);
/* Drives deadlines and internally buffered complete frames without new network
 * bytes. AGAIN requests another bounded turn;0 NextDue means no timed deadline.
 * A borrowed paused suffix still has to be supplied by the caller after Resume. */
XTP_API xtpresult xtpReaderDrive(xtpreader* Reader,uint64_t NowUs,uint64_t* NextDue);
XTP_API xtpresult xtpReaderInfo(const xtpreader* Reader,xtpreaderinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
