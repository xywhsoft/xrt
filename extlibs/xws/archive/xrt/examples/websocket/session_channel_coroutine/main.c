#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <xrt.h>



#define SESSION_MESSAGE_LIMIT ((size_t)65536u)



/* Channel 中的消息拥有完整文本和一个 Connection 强引用。 */
typedef struct sessionmessage {
	xwsconn* Connection;
	size_t Size;
	size_t Capacity;
	uint8 Data[];
} sessionmessage;



/* 每条 Connection 使用独立接收状态，共享同一个业务 Channel。 */
typedef struct sessionsource {
	xchannel* Channel;
	sessionmessage* Building;
	bool Rejected;
} sessionsource;



/* 释放消息持有的 Connection 引用和按消息增长的存储。 */
static void sessionMessageDestroy(sessionmessage* pMessage)
{
	if ( pMessage == NULL ) {
		return;
	}
	if ( pMessage->Connection != NULL ) {
		xrtWsConnDestroy(pMessage->Connection);
	}
	xrtFree(pMessage);
}



/* 为当前消息扩展精确受限的动态容量。 */
static bool sessionMessageReserve(
	sessionsource* pSource,
	size_t iRequired
)
{
	sessionmessage* pMessage = pSource->Building;
	sessionmessage* pNew;
	size_t iCapacity;

	if ( iRequired > SESSION_MESSAGE_LIMIT ) {
		return false;
	}
	if ( iRequired <= pMessage->Capacity ) {
		return true;
	}
	iCapacity = (pMessage->Capacity != 0u) ?
		pMessage->Capacity : 256u;
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (SESSION_MESSAGE_LIMIT / 2u) ) {
			iCapacity = SESSION_MESSAGE_LIMIT;
		} else {
			iCapacity *= 2u;
		}
	}
	pNew = (sessionmessage*)xrtRealloc(
		pMessage,
		offsetof(sessionmessage, Data) + iCapacity
	);
	if ( pNew == NULL ) {
		return false;
	}
	pNew->Capacity = iCapacity;
	pSource->Building = pNew;
	return true;
}



/* 在 Worker 上开始一条文本消息，不预分配业务上限大小。 */
static void sessionMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	sessionsource* pSource = (sessionsource*)pData;

	if ( (pSource == NULL) || (pInfo == NULL) ) {
		return;
	}
	sessionMessageDestroy(pSource->Building);
	pSource->Building = NULL;
	pSource->Rejected = (pInfo->Opcode != XWS_OPCODE_TEXT);
	if ( pSource->Rejected ) {
		(void)xrtWsConnClose(
			pConnection,
			XWS_CLOSE_UNSUPPORTED,
			XRT_STR_LITERAL("text required")
		);
		return;
	}
	pSource->Building = (sessionmessage*)xrtMalloc(
		offsetof(sessionmessage, Data)
	);
	if ( pSource->Building == NULL ) {
		pSource->Rejected = true;
		(void)xrtWsConnClose(
			pConnection,
			XWS_CLOSE_INTERNAL,
			XRT_STR_LITERAL("out of memory")
		);
		return;
	}
	pSource->Building->Connection = NULL;
	pSource->Building->Size = 0u;
	pSource->Building->Capacity = 0u;
}



/* 复制当前借用分块；超限和 OOM 都终止本条应用消息。 */
static void sessionMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	sessionsource* pSource = (sessionsource*)pData;
	size_t iRequired;
	uint16 iClose;
	xstrview Reason;

	if ( (pSource == NULL) || pSource->Rejected ||
		(pSource->Building == NULL) ) {
		return;
	}
	if ( Data.Size > (SESSION_MESSAGE_LIMIT -
		pSource->Building->Size) ) {
		iClose = XWS_CLOSE_TOO_BIG;
		Reason = XRT_STR_LITERAL("message too large");
	} else {
		iRequired = pSource->Building->Size + Data.Size;
		if ( sessionMessageReserve(pSource, iRequired) ) {
			if ( Data.Size != 0u ) {
				memcpy(
					pSource->Building->Data +
						pSource->Building->Size,
					Data.Data,
					Data.Size
				);
			}
			pSource->Building->Size = iRequired;
			return;
		}
		iClose = XWS_CLOSE_INTERNAL;
		Reason = XRT_STR_LITERAL("out of memory");
	}
	sessionMessageDestroy(pSource->Building);
	pSource->Building = NULL;
	pSource->Rejected = true;
	(void)xrtWsConnClose(pConnection, iClose, Reason);
}



/* 把完整消息和 Connection 强引用原子移交给有界 Channel。 */
static void sessionMessageEnd(xwsconn* pConnection, ptr pData)
{
	sessionsource* pSource = (sessionsource*)pData;
	sessionmessage* pMessage;
	xchannelresult Result;

	if ( pSource == NULL ) {
		return;
	}
	if ( pSource->Rejected || (pSource->Building == NULL) ) {
		pSource->Rejected = false;
		return;
	}
	pMessage = pSource->Building;
	pSource->Building = NULL;
	pMessage->Connection = xrtWsConnRef(pConnection);
	if ( pMessage->Connection == NULL ) {
		sessionMessageDestroy(pMessage);
		return;
	}
	Result = xrtChannelTrySend(pSource->Channel, pMessage);
	if ( Result == XCHANNEL_OK ) {
		return;
	}
	if ( Result == XCHANNEL_FULL ) {
		if ( xrtWsConnText(
			pConnection,
			XRT_STR_LITERAL("busy")
		) != XNET_RESULT_OK ) {
			(void)xrtWsConnClose(
				pConnection,
				XWS_CLOSE_TRY_AGAIN,
				XRT_STR_LITERAL("service busy")
			);
		}
	}
	sessionMessageDestroy(pMessage);
}



/* 连接终结时释放尚未移交给 Channel 的半条业务消息。 */
static void sessionClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	sessionsource* pSource = (sessionsource*)pData;

	(void)pConnection;
	(void)pClose;
	if ( pSource == NULL ) {
		return;
	}
	sessionMessageDestroy(pSource->Building);
	pSource->Building = NULL;
	pSource->Rejected = false;
}



/* 协程顺序消费业务消息，并异步投递回原 Connection 的 Worker。 */
static ptr sessionConsume(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;
	sessionmessage* pMessage;
	xfuture* pFuture;
	xstrview Text;
	ptr pItem;

	while ( xrtChannelRecvAwait(pChannel, &pItem) ==
		XWAIT_OK ) {
		pMessage = (sessionmessage*)pItem;
		Text.Data = (pMessage->Size != 0u) ?
			(const char*)pMessage->Data : NULL;
		Text.Size = pMessage->Size;
		pFuture = xrtWsConnTextAsync(
			pMessage->Connection,
			Text
		);
		if ( pFuture != NULL ) {
			(void)xrtFutureAwaitFor(
				pFuture,
				UINT64_C(5000000)
			);
			xrtFutureDestroy(pFuture);
		}
		sessionMessageDestroy(pMessage);
	}
	return pChannel;
}



/* Channel 销毁前释放尚未由协程取得的正式所有权消息。 */
static void sessionDrain(ptr pItem, ptr pData)
{
	(void)pData;
	sessionMessageDestroy((sessionmessage*)pItem);
}



/*
	展示会话事件、Channel 和协程的组合边界。
	真实服务器为每条 Upgrade Connection 创建一个 sessionsource，并把同一 Channel
	交给业务调度器；Connection Close 后再释放对应 source。
*/
int main(void)
{
	xwsconnevents Events;
	xchannel* pChannel = NULL;
	xcosched* pScheduler = NULL;
	xcoro* pConsumer = NULL;
	int iResult = 1;

	memset(&Events, 0, sizeof(Events));
	Events.MessageBegin = sessionMessageBegin;
	Events.MessageData = sessionMessageData;
	Events.MessageEnd = sessionMessageEnd;
	Events.Close = sessionClose;
	pChannel = xrtChannelCreate(64u);
	pScheduler = xrtCoSchedCreate();
	if ( (pChannel == NULL) || (pScheduler == NULL) ) {
		goto Cleanup;
	}
	pConsumer = xrtCoSpawn(
		pScheduler,
		sessionConsume,
		pChannel,
		NULL
	);
	if ( pConsumer == NULL ) {
		goto Cleanup;
	}
	xrtChannelClose(pChannel);
	if ( !xrtCoSchedRun(pScheduler) ) {
		goto Cleanup;
	}
	printf("WebSocket session Channel example is ready\n");
	iResult = 0;

Cleanup:
	if ( pChannel != NULL ) {
		xrtChannelClose(pChannel);
	}
	if ( pScheduler != NULL ) {
		xrtCoSchedClose(pScheduler);
		(void)xrtCoSchedRun(pScheduler);
	}
	xrtCoDestroy(pConsumer);
	xrtCoSchedDestroy(pScheduler);
	xrtCoThreadDetach();
	if ( pChannel != NULL ) {
		(void)xrtChannelDrain(
			pChannel,
			sessionDrain,
			NULL
		);
		xrtChannelDestroy(pChannel);
	}
	(void)Events;
	return iResult;
}
