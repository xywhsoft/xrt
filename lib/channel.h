#ifndef XRT_CHANNEL_H
#define XRT_CHANNEL_H

/*
	XRT channel runtime.

	The channel is a small blocking message queue wrapper built on top of
	xrtMPSCQWait.  It stores opaque pointers only; value ownership is defined by
	the caller so C and xlang can both use the same primitive without coupling.
*/

#ifndef XRT_CHANNEL_DEFAULT_CAPACITY
	#define XRT_CHANNEL_DEFAULT_CAPACITY 64u
#endif


// 内部函数：修正 channel 容量
static uint32 __xrtChannelCapacity(uint32 iCapacity)
{
	return iCapacity == 0u ? XRT_CHANNEL_DEFAULT_CAPACITY : iCapacity;
}


// 初始化 channel
XXAPI bool xrtChannelInit(xchannel pChannel, uint32 iCapacity)
{
	xqueue_config tCfg;

	if ( pChannel == NULL ) {
		xrtSetError("channel is null.", FALSE);
		return FALSE;
	}

	memset(pChannel, 0, sizeof(*pChannel));
	memset(&tCfg, 0, sizeof(tCfg));
	tCfg.iCapacity = __xrtChannelCapacity(iCapacity);
	return xrtMPSCQWaitInit(&pChannel->tQueue, &tCfg);
}


// 释放 channel 内部资源
XXAPI void xrtChannelUnit(xchannel pChannel)
{
	if ( pChannel == NULL ) {
		return;
	}
	xrtMPSCQWaitUnit(&pChannel->tQueue);
}


// 创建 channel
XXAPI xchannel xrtChannelCreate(uint32 iCapacity)
{
	xchannel pChannel = (xchannel)xrtCalloc(1, sizeof(xchannel_struct));

	if ( pChannel == NULL ) {
		return NULL;
	}
	if ( !xrtChannelInit(pChannel, iCapacity) ) {
		xrtFree(pChannel);
		return NULL;
	}
	return pChannel;
}


// 销毁 channel
XXAPI void xrtChannelDestroy(xchannel pChannel)
{
	if ( pChannel == NULL ) {
		return;
	}
	xrtChannelUnit(pChannel);
	xrtFree(pChannel);
}


// 向 channel 发送一个指针值
XXAPI xqueue_result xrtChannelSend(xchannel pChannel, ptr pItem)
{
	if ( pChannel == NULL ) {
		xrtSetError("channel is null.", FALSE);
		return XQUEUE_ERROR;
	}
	return xrtMPSCQWaitTryPush(&pChannel->tQueue, pItem);
}


// 从 channel 尝试接收一个指针值
XXAPI xqueue_result xrtChannelTryRecv(xchannel pChannel, ptr* ppItem)
{
	if ( pChannel == NULL || ppItem == NULL ) {
		xrtSetError("channel receive argument is null.", FALSE);
		return XQUEUE_ERROR;
	}
	return xrtMPSCQWaitTryPop(&pChannel->tQueue, ppItem);
}


// 从 channel 阻塞接收一个指针值
XXAPI xqueue_result xrtChannelRecv(xchannel pChannel, ptr* ppItem)
{
	if ( pChannel == NULL || ppItem == NULL ) {
		xrtSetError("channel receive argument is null.", FALSE);
		return XQUEUE_ERROR;
	}
	return xrtMPSCQWaitPop(&pChannel->tQueue, ppItem);
}


// 从 channel 限时接收一个指针值
XXAPI xqueue_result xrtChannelRecvTimeout(xchannel pChannel, ptr* ppItem, uint32 iTimeoutMs)
{
	if ( pChannel == NULL || ppItem == NULL ) {
		xrtSetError("channel receive argument is null.", FALSE);
		return XQUEUE_ERROR;
	}
	return xrtMPSCQWaitPopTimeout(&pChannel->tQueue, ppItem, iTimeoutMs);
}


// 关闭 channel
XXAPI void xrtChannelClose(xchannel pChannel)
{
	if ( pChannel == NULL ) {
		return;
	}
	xrtMPSCQWaitClose(&pChannel->tQueue);
}


// 获取 channel 中近似元素数量
XXAPI uint32 xrtChannelCount(xchannel pChannel)
{
	if ( pChannel == NULL ) {
		return 0u;
	}
	return xrtMPSCQWaitApproxCount(&pChannel->tQueue);
}


// 判断 channel 是否已经关闭
XXAPI bool xrtChannelIsClosed(xchannel pChannel)
{
	if ( pChannel == NULL ) {
		return TRUE;
	}
	return xrtQueueIsClosed(&pChannel->tQueue.tQueue.tBase);
}

#endif
