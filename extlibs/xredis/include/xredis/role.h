#ifndef XREDIS_ROLE_H
#define XREDIS_ROLE_H
#include <xredis/reply.h>
#if defined(XREDIS_FEATURE_XREDIS_ROLE) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "ROLE inspection requires immutable replies"
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE)
typedef enum xredisrolekind {XREDIS_ROLE_MASTER=1,XREDIS_ROLE_REPLICA,XREDIS_ROLE_SENTINEL} xredisrolekind;
typedef enum xredisrolestatus {
    XREDIS_ROLE_ERROR=-1,XREDIS_ROLE_MALFORMED=-2,XREDIS_ROLE_LIMIT=-3,
    XREDIS_ROLE_OTHER=0,XREDIS_ROLE_VALUE=1,XREDIS_ROLE_SERVER_ERROR=2
} xredisrolestatus;
typedef enum xredisrolelink {
    XREDIS_ROLE_LINK_OTHER=0,XREDIS_ROLE_LINK_CONNECT,XREDIS_ROLE_LINK_CONNECTING,
    XREDIS_ROLE_LINK_SYNC,XREDIS_ROLE_LINK_CONNECTED
} xredisrolelink;
typedef struct xredisrolelimits {size_t Size,MaxEntries,MaxPayloadBytes;} xredisrolelimits;
/* Borrowed cursor: Source and all returned views require an existing live Reply
 * reference. Read consumes no reference and copies no payload. Use existing
 * ReplyRetain/Release for escaping ownership. Do not edit cursor fields. */
typedef struct xredisrolecursor {const xredisreply* Source;size_t Next,Remaining;uint32 Kind;} xredisrolecursor;
typedef struct xredisrolereplica {xbytesview Host;uint16 Port;int64 Offset;} xredisrolereplica;
typedef struct xredisroleinfo {
    xredisrolekind Kind;int64 Offset;xbytesview MasterHost,LinkText;uint16 MasterPort;
    xredisrolelink Link;xredisrolecursor Replicas,Monitored;
} xredisroleinfo;
XRT_EXTERN_C_BEGIN
/* Defaults4096 entries/1MiB scalar payload; zero is a real limit, not unlimited.
 * MaxPayloadBytes counts the whole immutable reply, including attributes and
 * numeric lexemes. MaxEntries limits the replica/monitored list, not fixed
 * replica-role fields. Output must not overlap any source reply allocation. The caller's bounded reply reader still owns parsing costs. */
XREDIS_API bool xredisRoleLimitsInit(xredisrolelimits* Limits);
/* Validate the complete recognized ROLE shape, then publish one borrowed view.
 * Master replica tuples accept decimal port/offset strings used by Redis and
 * integer numeric forms. Replica offset -1 means unavailable; master offsets
 * are nonnegative. Port0 remains represented, not invented as a usable endpoint.
 * Unknown replica link text is preserved as LINK_OTHER. Protocol role text
 * "slave" maps to REPLICA; it is a wire value, not an API spelling.
 * VALUE alone writes Info. OTHER/server-error/malformed/limit preserve output
 * and ambient error. Invalid arguments report ERROR. No successful/protocol/
 * limit path allocates, copies payload or retains ownership. Validation is
 * O(entries + decimal bytes), bounded by Limits; iteration is O(1) per
 * entry for Redis canonical numeric fields. No host/name payload is scanned. */
XREDIS_API xredisrolestatus xredisRoleRead(const xredisreply* Reply,
    const xredisrolelimits* Limits,xredisroleinfo* Info);
/* Transfer one borrowed view and advance only on success. Exhaustion preserves
 * outputs and ambient error. Pass an unmodified cursor from Read and a live source Reply. Detected invalid
 * cursor/output is an argument error;
 * cursor and output storage must be disjoint and caller-owned writable objects. */
XREDIS_API bool xredisRoleReplicaNext(xredisrolecursor* Cursor,xredisrolereplica* Replica);
XREDIS_API bool xredisRoleMasterNext(xredisrolecursor* Cursor,xbytesview* Name);
XRT_EXTERN_C_END
#endif
#endif
