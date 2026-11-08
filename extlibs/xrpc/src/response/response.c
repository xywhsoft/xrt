#include <xrpc/response.h>
#include "../internal/xrpc_result_quota.h"
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_RESPONSE)
struct xrpcresponses {
	xatomic32 References;
	bool Batch,QuotaOwned;
	size_t Count, OwnedBytes;
	xstrview Wire;
	xrpcmessage Messages[];
};
static bool xrpcResponseReport(xerrkind Kind,xrpcresponseerror Code,cstr Message)
{ xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false; }
XRPC_API bool xrpcResponseConfigInit(xrpcresponseconfig* Config)
{
	if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))
		return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"invalid RPC response config output");
	xrpcresponseconfig C={0};C.Size=sizeof(C);C.MaxOwnedBytes=2u*1024u*1024u;
	xrpcDecodeConfigInit(&C.Decode);C.Decode.Mode=XRPC_DECODE_RESPONSES;*Config=C;return true;
}
static xstrview xrpcResponseRebase(xstrview View,xstrview Input,cstr Copy)
{ if(View.Data)View.Data=Copy+(size_t)(View.Data-Input.Data);return View; }
static xrpcresponsestatus xrpcResponsesReadImpl(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,xrpcresponses** Result,xrpcresultquota* Quota)
{
	(void)Quota;
	if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || !Capacity || !Scratch || Capacity>SIZE_MAX/sizeof(*Scratch) ||
		!xrtMemRangeValid(Scratch,Capacity*sizeof(*Scratch)) || !xrtMemRangeValid(Input.Data,Input.Size) ||
		(Config && !xrtMemRangeValid(Config,sizeof(*Config))))goto Argument;
	if(xrtMemRangesOverlap(Result,sizeof(*Result),Input.Data,Input.Size) ||
		xrtMemRangesOverlap(Result,sizeof(*Result),Scratch,Capacity*sizeof(*Scratch)) || (Config &&
		(xrtMemRangesOverlap(Result,sizeof(*Result),Config,sizeof(*Config)) ||
		 xrtMemRangesOverlap(Scratch,Capacity*sizeof(*Scratch),Config,sizeof(*Config)))))goto Argument;
	xrpcresponseconfig C;if(Config)C=*Config;else xrpcResponseConfigInit(&C);
	if(C.Size!=sizeof(C) || !C.MaxOwnedBytes || C.Decode.Mode!=XRPC_DECODE_RESPONSES)goto Argument;
	xrpcdocument Document;xrpcdecodestatus Status=xrpcDecode(Input,&C.Decode,Scratch,Capacity,&Document);
	if(Status==XRPC_DECODE_LOCAL_ERROR)return XRPC_RESPONSE_LOCAL_ERROR;
	if(Status==XRPC_DECODE_PARSE_ERROR)return XRPC_RESPONSE_PARSE_ERROR;
	if(Document.InvalidCount){xrpcResponseReport(XERR_VALUE,XRPC_RESPONSE_ENVELOPE,"invalid RPC response envelope/batch");
		return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_RESPONSE_LOCAL_ERROR:XRPC_RESPONSE_INVALID;}
	size_t Bytes=sizeof(xrpcresponses);
	if(Document.Count>(SIZE_MAX-Bytes)/(sizeof(xrpcmessage)+sizeof(xstrview)))goto Limit;
	Bytes+=Document.Count*(sizeof(xrpcmessage)+sizeof(xstrview));
	if(Input.Size>SIZE_MAX-Bytes || Bytes+Input.Size>C.MaxOwnedBytes)goto Limit;
	Bytes+=Input.Size;xrpcresponses* R=NULL;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
	if(Quota){bool Limited=false;R=xrpcResultQuotaAlloc(Quota,Bytes,&Limited);if(Limited)goto Limit;}
	else
#endif
	R=xrtMalloc(Bytes);
	if(!R){xrpcResponseReport(XERR_MEMORY,XRPC_RESPONSE_MEMORY,"RPC response snapshot allocation failed");return XRPC_RESPONSE_LOCAL_ERROR;}
	xrtAtomic32Init(&R->References,1);R->Batch=Document.Batch;R->QuotaOwned=Quota!=NULL;R->Count=Document.Count;R->OwnedBytes=Bytes;
	xstrview* Ids=(xstrview*)(R->Messages+Document.Count);char* Wire=(char*)(Ids+Document.Count);
	memcpy(Wire,Input.Data,Input.Size);R->Wire=(xstrview){Wire,Input.Size};
	for(size_t Index=0;Index<Document.Count;++Index){xrpcmessage M=Scratch[Index];
		M.Wire=xrpcResponseRebase(M.Wire,Input,Wire);
		M.Id.Raw=xrpcResponseRebase(M.Id.Raw,Input,Wire);M.Method.Raw=xrpcResponseRebase(M.Method.Raw,Input,Wire);
		M.Params.Raw=xrpcResponseRebase(M.Params.Raw,Input,Wire);M.Result.Raw=xrpcResponseRebase(M.Result.Raw,Input,Wire);
		M.Error.Raw=xrpcResponseRebase(M.Error.Raw,Input,Wire);M.ErrorCode.Raw=xrpcResponseRebase(M.ErrorCode.Raw,Input,Wire);
		M.ErrorMessage.Raw=xrpcResponseRebase(M.ErrorMessage.Raw,Input,Wire);M.ErrorData.Raw=xrpcResponseRebase(M.ErrorData.Raw,Input,Wire);
		R->Messages[Index]=M;Ids[Index]=M.Id.Raw;
	}
	*Result=R;return XRPC_RESPONSE_DONE;
Argument:
	xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"invalid RPC response config/output span/overlap");return XRPC_RESPONSE_LOCAL_ERROR;
Limit:
	xrpcResponseReport(XERR_RANGE,XRPC_RESPONSE_LIMIT,"RPC response owned-byte limit/size overflow");return XRPC_RESPONSE_LOCAL_ERROR;
}
XRPC_API xrpcresponsestatus xrpcResponsesRead(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,xrpcresponses** Result)
{return xrpcResponsesReadImpl(Input,Config,Scratch,Capacity,Result,NULL);}
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
xrpcresponsestatus xrpcResponsesReadQuota(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,xrpcresponses** Result,xrpcresultquota* Quota)
{return xrpcResponsesReadImpl(Input,Config,Scratch,Capacity,Result,Quota);}
#endif
XRPC_API bool xrpcResponsesRetain(xrpcresponses* Responses)
{
	if(!Responses)return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"RPC responses are NULL");
	uint32 Count=xrtAtomic32Load(&Responses->References,XMEMORY_RELAXED);
	for(;;){if(Count==UINT32_MAX)return xrpcResponseReport(XERR_RANGE,XRPC_RESPONSE_LIMIT,"RPC response references overflow");
		if(xrtAtomic32CompareExchange(&Responses->References,&Count,Count+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XRPC_API void xrpcResponsesRelease(xrpcresponses* Responses)
{
	if(!Responses || xrtAtomic32FetchSub(&Responses->References,1,XMEMORY_ACQ_REL)!=1)return;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
	if(Responses->QuotaOwned){xrpcResultQuotaFree(Responses);return;}
#endif
	xrtFree(Responses);
}
XRPC_API bool xrpcResponsesInfo(const xrpcresponses* Responses,xrpcresponseinfo* Info)
{
	if(!Responses || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) ||
		xrtMemRangesOverlap(Responses,Responses->OwnedBytes,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))
		return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"invalid RPC response info output");
	*Info=(xrpcresponseinfo){sizeof(*Info),Responses->Count,Responses->OwnedBytes,Responses->Batch,Responses->Wire,
		Responses->Messages,(const xstrview*)(Responses->Messages+Responses->Count)};return true;
}
#endif
