#ifndef XREDIS_TRANSACTION_H
#define XREDIS_TRANSACTION_H
#include <xredis/client.h>

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS_TRANSACTION requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#define XREDIS_TRANSACTION_COMMANDS_DEFAULT 256u
#define XREDIS_TRANSACTION_COMMANDS_MAX 65536u

typedef struct xredistransactioncommand {
    const xbytesview* Arguments;
    size_t Count;
} xredistransactioncommand;

typedef struct xredistransactionconfig {
    size_t Size;
    size_t MaxCommands;
} xredistransactionconfig;

/* Execution certainty is independent from local timeout/cancellation and from
 * error elements inside a valid EXEC array. Redis does not roll those back. */
typedef enum xredistransactionexecution {
    XREDIS_TRANSACTION_NOT_EXECUTED = 0,
    XREDIS_TRANSACTION_MAY_HAVE_EXECUTED,
    XREDIS_TRANSACTION_EXECUTED,
    XREDIS_TRANSACTION_ABORTED
} xredistransactionexecution;

/* Owned by the returned Future; Request is the first member. Request.Reply is
 * the final EXEC array/error or a rejected MULTI reply. QueueError retains the
 * first queue error; other queue errors are counted and promptly released.
 * Queue rejection uses SERVER_ERROR, no EXEC reply, and NOT_EXECUTED after a
 * validated DISCARD. Counts describe knowledge at logical completion and do
 * not change while an abandoned operation drains. Retain replies explicitly
 * to use them after destroying the Future. Results do not retain the client. */
typedef struct xredistransactionresult {
    xredisrequestresult Request;
    xredistransactionexecution Execution;
    size_t Commands;
    size_t Queued;
    size_t QueueErrors;
    xredisreply* QueueError;
} xredistransactionresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientTransactionConfigInit(xredistransactionconfig* Config);

/* One exclusive MULTI / queue / EXEC (or DISCARD) operation on a managed
 * connection. Raw Connect is refused. MULTI is acknowledged before business
 * commands are sent; preceding requests drain, and later requests cannot enter
 * the transaction. Input is borrowed only during admission and encoded once
 * into owned storage. Count=0 is an empty transaction. Config=NULL uses bounded
 * defaults. Client request/byte limits include transaction wire and metadata.
 * Timeout includes admission/encoding/worker/setup/transaction/cleanup time.
 * Before EXEC is accepted, cancellation/timeout suppresses EXEC and drains
 * DISCARD; afterwards execution may be uncertain. DrainTimeout still bounds
 * physical cleanup. Cleanup rejection/malformed or unknown EXEC replies retire
 * the connection; a recognized EXECABORT confirms abort and permits reuse.
 * No automatic replay, rollback, WATCH/read/decision session or retry is added.
 * Connection/mode controls are not transaction payload commands. Unknown module
 * commands must obey Redis transactional queue/reply semantics. Output changes
 * only on ACCEPTED. Future cancellation returns a typed immutable result. */
XREDIS_API xredisadmission xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future);

/* Same deadline and outcome, then waits. Rejects every worker of this engine. */
XREDIS_API xredisadmission xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
