#ifndef XTP_TYPES_H
#define XTP_TYPES_H
#include <xtp/features.h>
#include <xrt/core.h>
#include <string.h>

#define XTP_HEADER_SIZE 32u
#define XTP_WIRE_VERSION 3u
#define XTP_PARAMS_MAX 256u
#define XTP_BATCH_MAX 64u
#define XTP_FLAG_ROUTE_ID 0x01u
#define XTP_FLAG_PARAM_ARRAY 0x02u
#define XTP_FLAG_RESULT_ACK 0x04u
#define XTP_FLAG_REPLAYABLE 0x08u

typedef enum xtpcommandkind { XTP_COMMAND_NONE=0, XTP_COMMAND_ID, XTP_COMMAND_NAME } xtpcommandkind;
typedef struct xtpcommand {
    xtpcommandkind Kind;
    union { uint32_t ID; xstrview Name; } As;
} xtpcommand;
typedef enum xtpparamkind { XTP_PARAMS_TABLE=0, XTP_PARAMS_ARRAY } xtpparamkind;
typedef struct xtpfield { xstrview Key; xbytesview Value; } xtpfield;
typedef struct xtpparams {
    xtpparamkind Kind;
    size_t Count;
    union { const xtpfield* Table; const xbytesview* Array; } As;
} xtpparams;
/* Library-produced borrowed view only. Do not alter _Wire/Count/Kind. */
typedef struct xtpparamsview { xtpparamkind Kind; size_t Count; xbytesview _Wire; } xtpparamsview;
typedef enum xtpmessagekind { XTP_REQUEST=1, XTP_RESPONSE, XTP_PUSH, XTP_ACK, XTP_NOTIFY } xtpmessagekind;
typedef enum xtpstatus {
    XTP_STATUS_OK=0, XTP_STATUS_BUSY=-1, XTP_STATUS_REJECTED=-2,
    XTP_STATUS_UNSUPPORTED=-3, XTP_STATUS_BAD_ARGUMENT=-4,
    XTP_STATUS_RESULT_TOO_LARGE=-5, XTP_STATUS_INTERNAL=-6,
    XTP_STATUS_NOT_FOUND=-7, XTP_STATUS_STATE_LOST=-8,
    XTP_STATUS_DEADLINE_EXCEEDED=-9
} xtpstatus;
typedef enum xtpresult {
    XTP_OK=0, XTP_AGAIN=1, XTP_CLOSED=2, XTP_MORE=3, XTP_MESSAGE=4,
    XTP_DROPPED=5, XTP_PAUSED=6, XTP_END=7,
    XTP_E_ARGUMENT=-1, XTP_E_LIMIT=-2, XTP_E_MEMORY=-3, XTP_E_STATE=-4,
    XTP_E_MAGIC=-5, XTP_E_VERSION=-6, XTP_E_TYPE=-7, XTP_E_LENGTH=-8,
    XTP_E_VALUE=-9, XTP_E_TRUNCATED=-10, XTP_E_CAPACITY=-11, XTP_E_DEADLINE=-12
} xtpresult;
/* Session submission descriptions; all contents borrow until the call returns. */
typedef struct xtpmessage {
    xtpmessagekind Type; xtpcommand Command; xtpparams Params; xbytesview Body;
} xtpmessage;
typedef struct xtpresponse { int32_t Status; xtpparams Params; xbytesview Body; } xtpresponse;
/* Low-level codec descriptor. Session users do not assign MessageID/flags. */
typedef struct xtppacket {
    uint64_t MessageID; xtpmessagekind Type; xtpcommand Command;
    xtpparams Params; xbytesview Body; int32_t Status;
    bool Replayable, ResultAck;
} xtppacket;
typedef struct xtpframeview {
    uint64_t MessageID; xtpmessagekind Type; int32_t Status;
    xtpcommand Command; xtpparamsview Params; xbytesview Body, Packet;
    bool Replayable, ResultAck;
} xtpframeview;
typedef enum xtpparamresult {
    XTP_PARAM_FOUND=1, XTP_PARAM_MISSING=0, XTP_PARAM_WRONG_MODE=-1, XTP_PARAM_ARGUMENT=-2
} xtpparamresult;
typedef struct xtpparamiter { const xtpparamsview* _Params; size_t _Next; } xtpparamiter;
typedef struct xtpparamentry { size_t Index; bool HasKey; xstrview Key; xbytesview Value; } xtpparamentry;

static inline xtpcommand xtpCmdID(uint32_t ID)
{ xtpcommand c={XTP_COMMAND_ID,{0}}; c.As.ID=ID; return c; }
static inline xtpcommand xtpCmdName(xstrview Name)
{ xtpcommand c={XTP_COMMAND_NAME,{0}}; c.As.Name=Name; return c; }
static inline xtpparams xtpArray(const xbytesview* Items,size_t Count)
{ xtpparams p={XTP_PARAMS_ARRAY,0,{NULL}}; p.Count=Count; p.As.Array=Items; return p; }
static inline xtpparams xtpTable(const xtpfield* Items,size_t Count)
{ xtpparams p={XTP_PARAMS_TABLE,0,{NULL}}; p.Count=Count; p.As.Table=Items; return p; }
static inline bool xtpCommandIsName(const xtpcommand* Command,xstrview Name)
{
    return Command && Command->Kind==XTP_COMMAND_NAME && Command->As.Name.Size==Name.Size &&
        (!Name.Size || (Name.Data && Command->As.Name.Data && !memcmp(Command->As.Name.Data,Name.Data,Name.Size)));
}
#endif
