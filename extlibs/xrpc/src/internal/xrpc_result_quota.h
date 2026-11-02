#ifndef XRPC_RESULT_QUOTA_H
#define XRPC_RESULT_QUOTA_H
#include <xrpc/response.h>
typedef struct xrpcresultquota xrpcresultquota;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
/* Per-client storage ledger, not a runtime/engine owner. A charged allocation
 * owns one real ledger reference and refunds only after its physical free. */
bool xrpcResultQuotaCreate(size_t Blocks,size_t Bytes,xrpcresultquota** Result);
void xrpcResultQuotaRelease(xrpcresultquota* Quota);
void xrpcResultQuotaInfo(xrpcresultquota* Quota,size_t* Blocks,size_t* Bytes,uint64* Rejected);
ptr xrpcResultQuotaAlloc(xrpcresultquota* Quota,size_t Bytes,bool* Limited);
void xrpcResultQuotaFree(ptr Value);
xrpcresponsestatus xrpcResponsesReadQuota(xstrview Input,const xrpcresponseconfig* Config,
    xrpcmessage* Scratch,size_t Capacity,xrpcresponses** Result,xrpcresultquota* Quota);
#endif
#endif
