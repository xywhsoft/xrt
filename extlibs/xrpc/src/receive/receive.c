#include <xrpc/receive.h>
#include <xrt/memory.h>

#if defined(XRPC_FEATURE_XRPC_RECEIVE)
typedef struct xrpcreceiverange {const void* Data;size_t Bytes;} xrpcreceiverange;
static xrpcresponsestatus xrpcReceiveReport(xerrkind Kind,xrpcreceiveerror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return XRPC_RESPONSE_LOCAL_ERROR;}
XRPC_API xrpcresponsestatus xrpcReceive(xrpcpending* Pending,xstrview Input,
	const xrpcresponseconfig* Config,xrpcmessage* Scratch,size_t Capacity,
	xrpcpendingresolution* Resolutions,size_t ResolutionCapacity,xrpcresponses** Responses,xrpcreceiveinfo* Info)
{
	if(!Capacity || Capacity>SIZE_MAX/sizeof(*Scratch) || !ResolutionCapacity ||
		ResolutionCapacity>SIZE_MAX/sizeof(*Resolutions))goto Argument;
	xrpcpendinginfo PendingInfo={0};PendingInfo.Size=sizeof(PendingInfo);
	if(!xrpcPendingInfo(Pending,&PendingInfo))return XRPC_RESPONSE_LOCAL_ERROR;
	xrpcreceiverange Ranges[]={
		{Input.Data,Input.Size},{Config,Config?sizeof(*Config):0},{Scratch,Capacity*sizeof(*Scratch)},
		{Resolutions,ResolutionCapacity*sizeof(*Resolutions)},{Responses,sizeof(*Responses)},
		{Info,sizeof(*Info)},{Pending,PendingInfo.OwnedBytes}};
	for(size_t A=0;A<sizeof(Ranges)/sizeof(Ranges[0]);++A){
		if(!xrtMemRangeValid(Ranges[A].Data,Ranges[A].Bytes))goto Argument;
		for(size_t B=0;B<A;++B)if(xrtMemRangesOverlap(Ranges[A].Data,Ranges[A].Bytes,Ranges[B].Data,Ranges[B].Bytes))goto Argument;
	}
	if(Info->Size!=sizeof(*Info))goto Argument;
	xrpcresponses* Owned=NULL;xrpcresponsestatus Status=xrpcResponsesRead(Input,Config,Scratch,Capacity,&Owned);
	if(Status!=XRPC_RESPONSE_DONE)return Status;
	xrpcresponseinfo Snapshot={0};Snapshot.Size=sizeof(Snapshot);
	if(!xrpcResponsesInfo(Owned,&Snapshot)){xrpcResponsesRelease(Owned);return XRPC_RESPONSE_LOCAL_ERROR;}
	if(Snapshot.Count>ResolutionCapacity){xrpcResponsesRelease(Owned);
		return xrpcReceiveReport(XERR_RANGE,XRPC_RECEIVE_CAPACITY,"RPC response resolution capacity exceeded");}
	if(!xrpcPendingTakeBatch(Pending,Snapshot.Ids,Snapshot.Count,Resolutions)){xrpcResponsesRelease(Owned);return XRPC_RESPONSE_LOCAL_ERROR;}
	/* No failable work/callback after successful batch removal. */
	xrpcreceiveinfo Complete={0};Complete.Size=sizeof(Complete);Complete.Batch=Snapshot.Batch;Complete.Count=Snapshot.Count;
	for(size_t Index=0;Index<Snapshot.Count;++Index){
		if(Resolutions[Index].Status==XRPC_PENDING_MATCHED)++Complete.Matched;
		else if(Resolutions[Index].Status==XRPC_PENDING_REPEATED)++Complete.Repeated;
		else ++Complete.Unknown;
	}
	*Responses=Owned;*Info=Complete;return XRPC_RESPONSE_DONE;
Argument:
	return xrpcReceiveReport(XERR_ARGUMENT,XRPC_RECEIVE_ARGUMENT,"invalid RPC receive spans/overlap/info size");
}
#endif
