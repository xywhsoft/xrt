


// 创建结构体动态栈
XXAPI xdynstack xrtDynStackCreate(uint32 iItemLength)
{
	xdynstack objSTK = xrtMalloc(sizeof(xdynstack_struct));
	if ( objSTK ) {
		xrtDynStackInit(objSTK, iItemLength);
	}
	return objSTK;
}

// 销毁结构体动态栈
XXAPI void xrtDynStackDestroy(xdynstack objSTK)
{
	if ( objSTK ) {
			(xrtDynStackUnit)(objSTK);
		xrtFree(objSTK);
	}
}

// 初始化结构体动态栈（对自维护结构体指针使用，和 SDSTK_Create 功能类似）
XXAPI void xrtDynStackInit(xdynstack objSTK, uint32 iItemLength)
{
	objSTK->ItemLength = iItemLength;
	objSTK->Count = 0;
	xrtPtrArrayInit(&objSTK->MMU, XRT_OBJMODE_LOCAL);
	objSTK->MMU.AllocStep = 64;
}

// 释放结构体动态栈（对自维护结构体指针使用，和 SDSTK_Create 功能类似）
XXAPI void xrtDynStackUnit(xdynstack objSTK)
{
	objSTK->Count = 0;
	// 循环释放所有内存块
	for ( uint32 i = 0; i < objSTK->MMU.Count; i++ ) {
		xrtFree(objSTK->MMU.Memory[i]);
	}
	xrtPtrArrayUnit(&objSTK->MMU);
}

#ifdef XRT_MEM_DEBUG
// 创建 dyn 栈调试
XXAPI xdynstack xrtDynStackCreateDbg(uint32 iItemLength, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xdynstack objSTK = xrtDynStackCreate(iItemLength);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objSTK, XRT_MEMDEBUG_OBJECT_DYNSTACK, XRT_MEMDEBUG_OBJECT_ORIGIN_CREATE, sFile, iLine);
	return objSTK;
}


// 初始化 dyn 栈调试
XXAPI void xrtDynStackInitDbg(xdynstack objSTK, uint32 iItemLength, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	xrtDynStackInit(objSTK, iItemLength);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugRegisterObject(objSTK, XRT_MEMDEBUG_OBJECT_DYNSTACK, XRT_MEMDEBUG_OBJECT_ORIGIN_INIT, sFile, iLine);
}


// 销毁 dyn 栈调试
XXAPI void xrtDynStackDestroyDbg(xdynstack objSTK, const char* sFile, uint32 iLine)
{
	if ( objSTK ) {
		xrtMemDebugSiteScope tScope;
		if ( !__xrtMemDebugObjectGuardDestroy(objSTK, XRT_MEMDEBUG_OBJECT_DYNSTACK, sFile, iLine) ) {
			return;
		}
		tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
		(xrtDynStackUnit)(objSTK);
		__xrtMemDebugLeaveSiteScope(&tScope);
		__xrtMemDebugUnregisterObject(objSTK, XRT_MEMDEBUG_OBJECT_DYNSTACK, sFile, iLine);
		xrtFreeDbg(objSTK, sFile, iLine);
	}
}


// 释放 dyn 栈调试
XXAPI void xrtDynStackUnitDbg(xdynstack objSTK, const char* sFile, uint32 iLine)
{
	xrtMemDebugSiteScope tScope;
	if ( objSTK == NULL ) {
		return;
	}
	if ( !__xrtMemDebugObjectGuardDestroy(objSTK, XRT_MEMDEBUG_OBJECT_DYNSTACK, sFile, iLine) ) {
		return;
	}
	tScope = __xrtMemDebugEnterSiteScope(sFile, iLine);
	(xrtDynStackUnit)(objSTK);
	__xrtMemDebugLeaveSiteScope(&tScope);
	__xrtMemDebugUnregisterObject(objSTK, XRT_MEMDEBUG_OBJECT_DYNSTACK, sFile, iLine);
}
#endif

// 压栈
XXAPI ptr xrtDynStackPush(xdynstack objSTK)
{
	str pBlock = NULL;
	uint32 iBlock = objSTK->Count >> 8;
	if ( objSTK->MMU.Count > iBlock ) {
		// 直接获取现有的内存块
		pBlock = objSTK->MMU.Memory[iBlock];
	} else {
		// 需要创建新的内存块
		if ( objSTK->ItemLength > (uint32)(SIZE_MAX / 256u) ) {
			return NULL;
		}
		pBlock = xrtMalloc((size_t)objSTK->ItemLength * 256u);
		if ( pBlock == NULL ) {
			return NULL;
		}
		uint32 idx = xrtPtrArrayAppend(&objSTK->MMU, pBlock);
		// !!! 错误处理 !!! 无法将内存添加到内存阵列
		if ( idx == 0 ) {
			xrtFree(pBlock);
			xrtSetError("Dynamic Stack : add memory unit failed.", FALSE);
			return NULL;
		}
	}
	// 数据压栈
	uint32 iPos = objSTK->Count & 0xFF;
	objSTK->Count++;
	return &pBlock[iPos * objSTK->ItemLength];
}


// 压入 dyn 栈数据
XXAPI uint32 xrtDynStackPushData(xdynstack objSTK, ptr pData)
{
	ptr p = xrtDynStackPush(objSTK);
	if ( p == NULL ) {
		return 0;
	}
	memcpy(p, pData, objSTK->ItemLength);
	return objSTK->Count;
}


// 压入 dyn 栈指针
XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal)
{
	ptr* p = (ptr*)xrtDynStackPush(objSTK);
	if ( p == NULL ) {
		return 0;
	}
	p[0] = pVal;
	return objSTK->Count;
}

// 出栈
XXAPI ptr xrtDynStackPop(xdynstack objSTK)
{
	if ( objSTK == NULL || objSTK->Count == 0 ) {
		return NULL;
	}
	ptr pRet = xrtDynStackTop(objSTK);
	objSTK->Count--;
	// 延迟释放内存块（最大内存长度超过已使用内存长度 256 + 32 个结构体，释放掉内存块）
	if ( (objSTK->MMU.Count << 8) > (objSTK->Count + 288) ) {
		objSTK->MMU.Count--;
		xrtFree(objSTK->MMU.Memory[objSTK->MMU.Count]);
	}
	return pRet;
}


// 弹出 dyn 栈指针
XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK)
{
	if ( objSTK == NULL || objSTK->Count == 0 ) {
		return NULL;
	}
	ptr pRet = xrtDynStackTopPtr(objSTK);
	objSTK->Count--;
	// 延迟释放内存块（最大内存长度超过已使用内存长度 256 + 32 个结构体，释放掉内存块）
	if ( (objSTK->MMU.Count << 8) > (objSTK->Count + 288) ) {
		objSTK->MMU.Count--;
		xrtFree(objSTK->MMU.Memory[objSTK->MMU.Count]);
	}
	return pRet;
}

// 获取栈顶对象
XXAPI ptr xrtDynStackTop(xdynstack objSTK)
{
	if ( objSTK == NULL || objSTK->Count == 0 ) {
		return NULL;
	}
	return xrtDynStackGetPos_Unsafe(objSTK, objSTK->Count);
}


// 获取顶部 dyn 栈指针
XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK)
{
	if ( objSTK == NULL || objSTK->Count == 0 ) {
		return NULL;
	}
	ptr* p = (ptr*)xrtDynStackGetPos_Unsafe(objSTK, objSTK->Count);
	return p[0];
}

// 获取任意位置对象
XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, uint32 iPos)
{
	if ( iPos > 0 ) {
		iPos--;
		if ( iPos < objSTK->Count ) {
			str pBlock = objSTK->MMU.Memory[iPos >> 8];
			return &pBlock[(iPos & 0xFF) * objSTK->ItemLength];
		}
	}
	return NULL;
}


// xrtDynStackGetPos_Unsafe 相关处理
XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, uint32 iPos)
{
	if ( objSTK == NULL || iPos == 0 ) { return NULL; }
	iPos--;
	str pBlock = objSTK->MMU.Memory[iPos >> 8];
	return &pBlock[(iPos & 0xFF) * objSTK->ItemLength];
}


// xrtDynStackGetPosPtr 相关处理
XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, uint32 iPos)
{
	if ( iPos > 0 ) {
		iPos--;
		if ( iPos < objSTK->Count ) {
			str pBlock = objSTK->MMU.Memory[iPos >> 8];
			ptr* p = (ptr*)&pBlock[(iPos & 0xFF) * objSTK->ItemLength];
			return p[0];
		}
	}
	return NULL;
}


// xrtDynStackGetPosPtr_Unsafe 相关处理
XXAPI ptr xrtDynStackGetPosPtr_Unsafe(xdynstack objSTK, uint32 iPos)
{
	if ( objSTK == NULL || iPos == 0 ) { return NULL; }
	iPos--;
	str pBlock = objSTK->MMU.Memory[iPos >> 8];
	ptr* p = (ptr*)&pBlock[(iPos & 0xFF) * objSTK->ItemLength];
	return p[0];
}


