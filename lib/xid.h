


// XID 转 字符串 ( 需要使用 xrtFree 释放内存 )
XXAPI str xrtXIDtoStr(xid pXID)
{
	return xrtBase64Encode(pXID, 24);
}



// 获取 XID ( 需要使用 xrtFree 释放内存 )
XXAPI xid xrtMakeXID(int32 iData)
{
	xid pXID = xrtMalloc(24);
	if ( pXID == NULL ) {
		xrtSetError(xCore.ERROR_DESC.MALLOC, FALSE);
		return NULL;
	}
	pXID->Data = iData;
	pXID->Tick = 0;
	pXID->Time = xrtNow();
	pXID->Addr = 0;
	pXID->Rand = xrtRand32();
	return pXID;
}



// 获取 XID 字符串 ( 需要使用 xrtFree 释放内存 )
XXAPI str xrtMakeXIDS(int32 iData)
{
	xid pXID = xrtMakeXID(iData);
	if ( pXID == NULL ) { return xCore.sNull; }
	str sRet = xrtXIDtoStr(pXID);
	xrtFree(pXID);
	return sRet;
}



// 比较两个 XID 是否相同
XXAPI int xrtCompXID(xid pXID1, xid pXID2)
{
	if ( (pXID1->Data == pXID2->Data) && (pXID1->Tick == pXID2->Tick) && (pXID1->Time == pXID2->Time) && (pXID1->Addr == pXID2->Addr) && (pXID1->Rand == pXID2->Rand) ) {
		return TRUE;
	} else {
		return FALSE;
	}
}


