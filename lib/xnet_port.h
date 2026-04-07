#ifndef XRT_XNET_PORT_H
#define XRT_XNET_PORT_H

/*
    XRT mainline backend-neutral network port interface.

    This is the internal contract between xnet_engine workers and concrete
    platform backends such as IOCP and the Linux transport backend. It defines
    the submission, completion, timer, and wake semantics consumed by the
    transport layers.
*/


/* ============================== Opaque handles ============================== */

typedef struct xrt_net_port xnetport;
typedef struct xrt_net_port_ops xnetportops;

static const xnetportops* xrtNetPortIOCPOps(void);
static const xnetportops* xrtNetPortUringOps(void);



/* ============================== Backend and op identifiers ============================== */

#define XNET_PORT_BACKEND_AUTO    0u
#define XNET_PORT_BACKEND_IOCP    1u
#define XNET_PORT_BACKEND_URING   2u
#define XNET_PORT_BACKEND_CUSTOM  3u

#define XNET_PORT_F_NONE              0x00000000u
#define XNET_PORT_F_BATCH_COMPLETION  0x00000001u
#define XNET_PORT_F_GATHER_SEND       0x00000002u
#define XNET_PORT_F_TIMER_WAKE        0x00000004u

#define XNET_PORT_OP_NONE      0u
#define XNET_PORT_OP_ACCEPT    1u
#define XNET_PORT_OP_CONNECT   2u
#define XNET_PORT_OP_RECV      3u
#define XNET_PORT_OP_SEND      4u
#define XNET_PORT_OP_RECVFROM  5u
#define XNET_PORT_OP_SENDTO    6u
#define XNET_PORT_OP_TIMER     7u
#define XNET_PORT_OP_WAKE      8u
#define XNET_PORT_OP_CLOSE     9u

#define XNET_PORT_EVENT_NONE      0u
#define XNET_PORT_EVENT_ACCEPT    1u
#define XNET_PORT_EVENT_CONNECT   2u
#define XNET_PORT_EVENT_RECV      3u
#define XNET_PORT_EVENT_SEND      4u
#define XNET_PORT_EVENT_RECVFROM  5u
#define XNET_PORT_EVENT_SENDTO    6u
#define XNET_PORT_EVENT_TIMER     7u
#define XNET_PORT_EVENT_WAKE      8u
#define XNET_PORT_EVENT_CLOSE     9u
#define XNET_PORT_EVENT_ERROR     10u

#define XNET_PORT_SUBMIT_F_NONE      0x00000000u
#define XNET_PORT_SUBMIT_F_LINKED    0x00000001u
#define XNET_PORT_SUBMIT_F_MORE      0x00000002u
#define XNET_PORT_SUBMIT_F_MULTISHOT 0x00000004u

#define XNET_PORT_EVENT_F_NONE       0x00000000u
#define XNET_PORT_EVENT_F_EOF        0x00000001u
#define XNET_PORT_EVENT_F_PARTIAL    0x00000002u
#define XNET_PORT_EVENT_F_MORE       0x00000004u
/* Internal marker: synthetic accepted-stream open event, not listener accept completion. */
#define XNET_PORT_EVENT_F_ACCEPTED_OPEN 0x8000u



/* ============================== Config and I/O descriptors ============================== */

typedef struct {
	uint32 iBackend;
	uint32 iFlags;
	uint32 iSqEntries;
	uint32 iCqEntries;
	uint32 iAcceptBatch;
	uint32 iEventBatch;
	uint32 iBufferGroupSize;
	uint32 iBufferGroupCount;
} xnetportconfig;

typedef struct {
	uint16 iOpType;
	uint16 iFlags;
	uint32 iReserved;
	uint64 iOpId;
	intptr_t hSocket;
	ptr pUserData;
	xnetchain* pChain;
	const xnetspan* pVec;
	uint32 iVecCount;
	uint32 iTimeoutMs;
	xnetaddr tAddr;
} xnetportsubmit;

typedef struct {
	uint16 iType;
	uint16 iFlags;
	xnet_result iStatus;
	uint32 iBytes;
	uint64 iOpId;
	intptr_t hSocket;
	ptr pUserData;
	xnetchain* pChain;
	xnetaddr tAddr;
} xnetportevent;



/* ============================== Backend vtable ============================== */

struct xrt_net_port_ops {
	const char* sName;
	xnet_result (*Init)(xnetport* pPort, const xnetportconfig* pCfg, ptr pOwner);
	void (*Unit)(xnetport* pPort);
	xnet_result (*Submit)(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount);
	uint32 (*Harvest)(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs);
	xnet_result (*Wake)(xnetport* pPort);
	xnet_result (*ArmTimer)(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs);
	xnet_result (*CancelTimer)(xnetport* pPort, uint64 iTimerId);
};

struct xrt_net_port {
	const xnetportops* pOps;
	ptr pOwner;
	ptr pCtx;
	xnetportconfig tConfig;
};



/* ============================== Config and wrapper helpers ============================== */

static void xrtNetPortConfigInit(xnetportconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xnetportconfig));
	pCfg->iBackend = XNET_PORT_BACKEND_AUTO;
	pCfg->iFlags = XNET_PORT_F_BATCH_COMPLETION | XNET_PORT_F_GATHER_SEND | XNET_PORT_F_TIMER_WAKE;
	pCfg->iSqEntries = 4096;
	pCfg->iCqEntries = 8192;
	pCfg->iAcceptBatch = 64;
	pCfg->iEventBatch = 256;
	pCfg->iBufferGroupSize = 2048;
	pCfg->iBufferGroupCount = 1024;
}


// 初始化网络端口
static xnet_result xrtNetPortInit(xnetport* pPort, const xnetportops* pOps, const xnetportconfig* pCfg, ptr pOwner)
{
	if ( !pPort || !pOps || !pOps->Init ) return XRT_NET_ERROR;
	memset(pPort, 0, sizeof(xnetport));
	pPort->pOps = pOps;
	pPort->pOwner = pOwner;
	if ( pCfg ) {
		pPort->tConfig = *pCfg;
	} else {
		xrtNetPortConfigInit(&pPort->tConfig);
	}
	return pOps->Init(pPort, &pPort->tConfig, pOwner);
}


// 释放网络端口
static void xrtNetPortUnit(xnetport* pPort)
{
	if ( !pPort ) return;
	if ( pPort->pOps && pPort->pOps->Unit ) {
		pPort->pOps->Unit(pPort);
	}
	memset(pPort, 0, sizeof(xnetport));
}


// 提交网络端口
static xnet_result xrtNetPortSubmit(xnetport* pPort, const xnetportsubmit* pOps, uint32 iCount)
{
	if ( !pPort || !pPort->pOps || !pPort->pOps->Submit || !pOps || iCount == 0 ) {
		return XRT_NET_ERROR;
	}
	return pPort->pOps->Submit(pPort, pOps, iCount);
}


// 提取网络端口
static uint32 xrtNetPortHarvest(xnetport* pPort, xnetportevent* pEvents, uint32 iMaxEvents, uint32 iTimeoutMs)
{
	if ( !pPort || !pPort->pOps || !pPort->pOps->Harvest || !pEvents || iMaxEvents == 0 ) {
		return 0;
	}
	return pPort->pOps->Harvest(pPort, pEvents, iMaxEvents, iTimeoutMs);
}


// 唤醒网络端口
static xnet_result xrtNetPortWake(xnetport* pPort)
{
	if ( !pPort || !pPort->pOps || !pPort->pOps->Wake ) return XRT_NET_ERROR;
	return pPort->pOps->Wake(pPort);
}


// 设置网络端口定时器
static xnet_result xrtNetPortArmTimer(xnetport* pPort, uint64 iTimerId, uint32 iDelayMs)
{
	if ( !pPort || !pPort->pOps || !pPort->pOps->ArmTimer ) return XRT_NET_ERROR;
	return pPort->pOps->ArmTimer(pPort, iTimerId, iDelayMs);
}


// 取消网络端口定时器
static xnet_result xrtNetPortCancelTimer(xnetport* pPort, uint64 iTimerId)
{
	if ( !pPort || !pPort->pOps || !pPort->pOps->CancelTimer ) return XRT_NET_ERROR;
	return pPort->pOps->CancelTimer(pPort, iTimerId);
}


#endif
