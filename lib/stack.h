


// 创建结构体静态栈
XXAPI SSSTK_Object SSSTK_Create(unsigned int iMaxCount, unsigned int iItemLength)
{
	SSSTK_Object objSTK = xrtMalloc(sizeof(SSSTK_Struct) + (iMaxCount * iItemLength));
	if ( objSTK ) {
		objSTK->Memory = (void*)&objSTK[1];
		objSTK->ItemLength = iItemLength;
		objSTK->MaxCount = iMaxCount;
		objSTK->Count = 0;
	}
	return objSTK;
}

// 压栈
XXAPI void* SSSTK_Push(SSSTK_Object objSTK)
{
	if ( objSTK->Count < objSTK->MaxCount ) {
		objSTK->Count++;
		return &objSTK->Memory[(objSTK->Count - 1) * objSTK->ItemLength];
	}
	return NULL;
}
XXAPI unsigned int SSSTK_PushData(SSSTK_Object objSTK, void* pData)
{
	if ( objSTK->Count < objSTK->MaxCount ) {
		memcpy(&objSTK->Memory[objSTK->Count * objSTK->ItemLength], pData, objSTK->ItemLength);
		objSTK->Count++;
		return objSTK->Count;
	}
	return 0;
}

// 出栈
XXAPI void* SSSTK_Pop(SSSTK_Object objSTK)
{
	if ( objSTK->Count == 0 ) {
		return NULL;
	}
	objSTK->Count--;
	return &objSTK->Memory[objSTK->Count * objSTK->ItemLength];
}

// 获取栈顶对象
XXAPI void* SSSTK_Top(SSSTK_Object objSTK)
{
	if ( objSTK->Count == 0 ) {
		return NULL;
	}
	return &objSTK->Memory[(objSTK->Count - 1) * objSTK->ItemLength];
}

// 获取任意位置对象
XXAPI void* SSSTK_GetPos(SSSTK_Object objSTK, unsigned int iPos)
{
	if ( iPos > 0 ) {
		iPos--;
		if ( iPos < objSTK->Count ) {
			return &objSTK->Memory[iPos * objSTK->ItemLength];
		}
	}
	return NULL;
}
XXAPI void* SSSTK_GetPos_Unsafe(SSSTK_Object objSTK, unsigned int iPos)
{
	return &objSTK->Memory[(iPos - 1) * objSTK->ItemLength];
}


