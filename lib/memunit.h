


// 创建内存管理单元（iItemLength会自动增加4个字节用于确定内存位置和所属的管理器单元编号）
XXAPI MMU_Object xrtMemUnitCreate(unsigned int iItemLength)
{
	iItemLength += sizeof(MMU_Value);
	MMU_Object objUnit = xrtMalloc(sizeof(MMU_Struct) + (256 * iItemLength));
	if ( objUnit ) {
		objUnit->ItemLength = iItemLength;
		objUnit->Count = 0;
		objUnit->FreeCount = 0;
		objUnit->FreeOffset = 0;
		objUnit->Flag = 0;
	}
	return objUnit;
}

// 从内存管理单元中申请一个元素
XXAPI void* xrtMemUnitAlloc(MMU_Object objUnit)
{
	if ( objUnit == NULL ) {
		return NULL;
	}
	if ( objUnit->Count > 255 ) {
		return NULL;
	}
	int idx = objUnit->Count;
	// 优先复用已释放的数据
	if ( objUnit->FreeCount > 0 ) {
		idx = objUnit->FreeList[objUnit->FreeOffset];
		objUnit->FreeOffset++;
		objUnit->FreeCount--;
	}
	objUnit->Count++;
	MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
	v->ItemFlag = MMU_FLAG_USE | objUnit->Flag | idx;
	return (void*)&v[1];
}

// 释放内存管理单元中某个元素
XXAPI int xrtMemUnitFreeIdx(MMU_Object objUnit, unsigned char idx)
{
	if ( objUnit == NULL ) {
		return FALSE;
	}
	MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
	if ( (v->ItemFlag & MMU_FLAG_USE) == 0 ) {
		return FALSE;
	}
	// 释放内存
	objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
	objUnit->Count--;
	if ( objUnit->Count ) {
		objUnit->FreeCount++;
	} else {
		objUnit->FreeCount = 0;
		objUnit->FreeOffset = 0;
	}
	v->ItemFlag = 0;
	return TRUE;
}
XXAPI int xrtMemUnitFree(MMU_Object objUnit, void* obj)
{
	if ( objUnit == NULL ) {
		return FALSE;
	}
	MMU_ValuePtr v = obj - 4;
	if ( (v->ItemFlag & MMU_FLAG_USE) == 0 ) {
		return FALSE;
	}
	unsigned char idx = v->ItemFlag & 0xFF;
	// 释放内存
	objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
	objUnit->Count--;
	if ( objUnit->Count ) {
		objUnit->FreeCount++;
	} else {
		objUnit->FreeCount = 0;
		objUnit->FreeOffset = 0;
	}
	v->ItemFlag = 0;
	return TRUE;
}

// 进行一轮GC，将 标记 或 未标记 的内存全部回收
XXAPI int xrtMemUnitGC(MMU_Object objUnit, int bFreeMark)
{
	if ( objUnit == NULL ) {
		return 0;
	}
	if ( objUnit->Count > 0 ) {
		return 0;
	}
	int iCount = 0;
	if ( bFreeMark ) {
		// 被标记的内存将被回收
		for ( int idx = 0; idx < 256; idx++ ) {
			MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
			if ( v->ItemFlag & MMU_FLAG_USE ) {
				if ( v->ItemFlag & MMU_FLAG_GC ) {
					objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
					objUnit->Count--;
					if ( objUnit->Count ) {
						objUnit->FreeCount++;
					} else {
						objUnit->FreeCount = 0;
						objUnit->FreeOffset = 0;
					}
					v->ItemFlag = 0;
					iCount++;
				}
			}
		}
	} else {
		// 未被标记的内存将被回收
		for ( int idx = 0; idx < 256; idx++ ) {
			MMU_ValuePtr v = (MMU_ValuePtr)&(objUnit->Memory[objUnit->ItemLength * idx]);
			if ( v->ItemFlag & MMU_FLAG_USE ) {
				if ( v->ItemFlag & MMU_FLAG_GC ) {
					v->ItemFlag &= ~MMU_FLAG_GC;
				} else {
					objUnit->FreeList[(objUnit->FreeOffset + objUnit->FreeCount) & 0xFF] = idx;
					objUnit->Count--;
					if ( objUnit->Count ) {
						objUnit->FreeCount++;
					} else {
						objUnit->FreeCount = 0;
						objUnit->FreeOffset = 0;
					}
					v->ItemFlag = 0;
					iCount++;
				}
			}
		}
	}
	return iCount;
}


