


// 静态值 : null、true、false
static xvalue_struct XVO_VALUE_NULL = {
	XVO_DT_NULL,
	0,
	TRUE,
	0,
	0,
	0
};
static xvalue_struct XVO_VALUE_TRUE = {
	XVO_DT_BOOL,
	0,
	TRUE,
	0,
	8,
	1
};
static xvalue_struct XVO_VALUE_FALSE = {
	XVO_DT_BOOL,
	0,
	TRUE,
	0,
	8,
	0
};



// 引用计数操作
XXAPI void xvoAddRef(xvalue pVal)
{
	if ( pVal ) {
		if ( pVal->RefCount >= 0x3FFFFF ) {
			// 引用计数太多，就转为静态值
			pVal->IsStatic = 1;
		} else {
			pVal->RefCount++;
		}
	}
}
int xvoListClear_FreeProc(int64 pKey, xvalue pVal, xvalue pList)
{
	xvoUnref(pVal);
	return FALSE;
}
int xvoTableClear_FreeProc(Dict_Key* pKey, xvalue pVal, xvalue pTbl)
{
	xvoUnref(pVal);
	return FALSE;
}
XXAPI void xvoUnref(xvalue pVal)
{
	if ( pVal ) {
		if ( pVal->IsStatic == 0 ) {
			pVal->RefCount--;
			// 引用计数用完了就销毁对象
			if ( pVal->RefCount == 0 ) {
				// 释放值
				if ( pVal->Type == XVO_DT_TEXT ) {
					xrtFree(pVal->vText);
				} else if ( pVal->Type == XVO_DT_ARRAY ) {
					for ( int i = 1; i <= pVal->vArray->Count; i++ ) {
						xvalue pVal = xrtPtrArrayGet_Inline(pVal->vArray, i);
						xvoUnref(pVal);
					}
					xrtPtrArrayDestroy(pVal->vArray);
				} else if ( pVal->Type == XVO_DT_LIST ) {
					xrtListWalk(pVal->vList, (ptr)xvoListClear_FreeProc, pVal->vList);
					xrtListDestroy(pVal->vList);
				} else if ( pVal->Type == XVO_DT_COLL ) {
					
				} else if ( pVal->Type == XVO_DT_TABLE ) {
					xrtDictWalk(pVal->vTable, (ptr)xvoTableClear_FreeProc, pVal->vTable);
					xrtDictDestroy(pVal->vTable);
				} else if ( pVal->Type == XVO_DT_STRUCT ) {
				} else if ( pVal->Type == XVO_DT_OBJECT ) {
				} else if ( pVal->Type == XVO_DT_CUSTOM ) {
				}
				// 释放变量本身
				xrtFree(pVal);
			}
		}
	}
}



// 创建值
XXAPI xvalue xvoCreateNull()
{
	return &XVO_VALUE_NULL;
}
XXAPI xvalue xvoCreateBool(int bVal)
{
	if ( bVal ) {
		return &XVO_VALUE_TRUE;
	} else {
		return &XVO_VALUE_FALSE;
	}
}
XXAPI xvalue xvoCreateInt(int64 iVal)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_INT;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 8;
		pVal->vInt = iVal;
	}
	return pVal;
}
XXAPI xvalue xvoCreateFloat(double fVal)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_FLOAT;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 8;
		pVal->vFloat = fVal;
	}
	return pVal;
}
XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, int iCharset, int bColloc)
{
	if ( sVal == NULL ) {
		sVal = xCore.sNull;
		bColloc = TRUE;
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TEXT;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		if ( iCharset == XVO_SDT_STR_U16 ) {
			pVal->SubType = XVO_SDT_STR_U16;
			if ( iSize == 0 ) {
				iSize = u16len(sVal);
			}
			pVal->Size = iSize;
			if ( iSize == 0 ) {
				pVal->vText = xCore.sNull;
			} else {
				if ( bColloc ) {
					pVal->vText16 = sVal;
				} else {
					pVal->vText16 = xrtCopyStrU16(sVal, iSize);
				}
			}
		} else if ( iCharset == XVO_SDT_STR_U32 ) {
			pVal->SubType = XVO_SDT_STR_U32;
			if ( iSize == 0 ) {
				iSize = u32len(sVal);
			}
			pVal->Size = iSize;
			if ( iSize == 0 ) {
				pVal->vText = xCore.sNull;
			} else {
				if ( bColloc ) {
					pVal->vText32 = sVal;
				} else {
					pVal->vText32 = xrtCopyStrU32(sVal, iSize);
				}
			}
		} else if ( iCharset == XVO_SDT_STR_BIN ) {
			pVal->SubType = XVO_SDT_STR_BIN;
			pVal->Size = iSize;
			if ( iSize == 0 ) {
				pVal->vText = xCore.sNull;
			} else {
				if ( bColloc ) {
					pVal->vPoint = sVal;
				} else {
					pVal->vPoint = xrtCopyMem(sVal, iSize);
				}
			}
		} else {
			pVal->SubType = XVO_SDT_STR_U8;
			if ( iSize == 0 ) {
				iSize = strlen(sVal);
			}
			pVal->Size = iSize;
			if ( iSize == 0 ) {
				pVal->vText = xCore.sNull;
			} else {
				if ( bColloc ) {
					pVal->vText = sVal;
				} else {
					pVal->vText = xrtCopyStr(sVal, iSize);
				}
			}
		}
	}
	return pVal;
}
XXAPI xvalue xvoCreateTime(xtime tVal)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TIME;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 8;
		pVal->vTime = tVal;
	}
	return pVal;
}
XXAPI xvalue xvoCreateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TIME;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 8;
		pVal->vTime = xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
	}
	return pVal;
}
XXAPI xvalue xvoCreateFunc(ptr pFunc, int iType)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_FUNC;
		if ( iType == XVO_SDT_FUNC_CDECL ) {
			pVal->SubType = XVO_SDT_FUNC_CDECL;
		} else if ( iType == XVO_SDT_FUNC_STDCALL ) {
			pVal->SubType = XVO_SDT_FUNC_STDCALL;
		} else if ( iType == XVO_SDT_FUNC_FASTCALL ) {
			pVal->SubType = XVO_SDT_FUNC_FASTCALL;
		} else {
			pVal->SubType = XVO_SDT_FUNC_XCALL;
		}
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 8;
		pVal->vFunc = pFunc;
	}
	return pVal;
}
XXAPI xvalue xvoCreateArray()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xparray objArr = xrtPtrArrayCreate();
		if ( objArr == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_ARRAY;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vArray = objArr;
	}
	return pVal;
}
XXAPI xvalue xvoCreateList()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xlist objList = xrtListCreate(sizeof(xvalue));
		if ( objList == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_LIST;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vList = objList;
	}
	return pVal;
}
XXAPI xvalue xvoCreateColl()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xavltree objColl = xrtAVLTreeCreate(sizeof(xvalue), NULL);
		if ( objColl == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_COLL;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vColl = objColl;
	}
	return pVal;
}
XXAPI xvalue xvoCreateTable()
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		xdict objTbl = xrtDictCreate(sizeof(xvalue));
		if ( objTbl == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_TABLE;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vTable = objTbl;
	}
	return pVal;
}
XXAPI xvalue xvoCreateStruct(uint32 iSize)
{
	if ( iSize == 0 ) {
		return NULL;
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		ptr pStruct = xrtMalloc(iSize);
		if ( pStruct == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_STRUCT;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vStruct = pStruct;
	}
	return pVal;
}
XXAPI xvalue xvoCreateObject(uint32 iSize)
{
	if ( iSize == 0 ) {
		return NULL;
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		ptr pStruct = xrtMalloc(iSize);
		if ( pStruct == NULL ) {
			xrtFree(pVal);
			return NULL;
		}
		pVal->Type = XVO_DT_OBJECT;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vStruct = pStruct;
	}
	return pVal;
}
XXAPI xvalue xvoCreateCustom(ptr pObj)
{
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_CUSTOM;
		pVal->SubType = 0;
		pVal->IsStatic = FALSE;
		pVal->RefCount = 1;
		pVal->Size = 0;
		pVal->vPoint = pObj;
	}
	return pVal;
}



// 读取值
XXAPI int xvoGetBool(xvalue pVal)
{
	if ( pVal == NULL ) {
		return FALSE;
	} else if ( pVal->Type == XVO_DT_NULL ) {
		return FALSE;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt != 0;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat != 0.0;
	} else {
		return TRUE;
	}
}
XXAPI int64 xvoGetInt(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vInt ? 1 : 0;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToI64(pVal->vText);
	} else {
		return 0;
	}
}
XXAPI double xvoGetFloat(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0.0;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vInt ? 1.0 : 0.0;
	} else if ( pVal->Type == XVO_DT_INT ) {
		return pVal->vInt;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		return pVal->vFloat;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return xrtStrToNum(pVal->vText);
	} else {
		return 0;
	}
}
XXAPI str xvoGetText(xvalue pVal, int* pType)
{
	if ( pVal == NULL ) {
		return xCore.sNull;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		return pVal->vInt ? "true" : "false";
	} else if ( pVal->Type == XVO_DT_INT ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(24);
		xrtI64ToStr(pVal->vInt, sRet);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		xrtNumToStr(pVal->vFloat, sRet);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		if ( pType ) {
			*pType = pVal->SubType;
		}
		return pVal->vText;
	} else if ( pVal->Type == XVO_DT_TIME ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(24);
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(pVal->vTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		sprintf(sRet, "%d-%02d-%02d %02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[function:%x]", pVal->vFunc);
		return sRet;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[array:%x]", pVal->vArray);
		return sRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[list:%x]", pVal->vList);
		return sRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[coll:%x]", pVal->vColl);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[table:%x]", pVal->vTable);
		return sRet;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[struct:%x]", pVal->vStruct);
		return sRet;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[object:%x]", pVal->vObject);
		return sRet;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		if ( pType ) {
			*pType = XVO_SDT_STR_U8;
		}
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[custom:%x]", pVal->vPoint);
		return sRet;
	} else {
		return xCore.sNull;
	}
}
XXAPI xtime xvoGetTime(xvalue pVal)
{
	if ( pVal == NULL ) {
		return 0;
	} else if ( pVal->Type == XVO_DT_TIME ) {
		return pVal->vTime;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return 0;			// 未来应该支持字符串转换为 xtime
	} else {
		return 0;
	}
}
XXAPI ptr xvoGetFunc(xvalue pVal, int* pType)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		if ( pType ) {
			*pType = pVal->SubType;
		}
		return pVal->vFunc;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetArray(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		return pVal->vArray;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetList(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		return pVal->vList;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetColl(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		return pVal->vColl;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetTable(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		return pVal->vTable;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetStruct(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		return pVal->vStruct;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetObject(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		return pVal->vObject;
	} else {
		return NULL;
	}
}
XXAPI ptr xvoGetCustom(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		return pVal->vPoint;
	} else {
		return NULL;
	}
}



// Array 读数据
XXAPI xvalue xvoArrayGetValue(xvalue pArr, uint32 index)
{
	if ( pArr == NULL ) {
		return NULL;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return NULL;
	}
	return xrtPtrArrayGet(pArr->vArray, index + 1);
}



// Array 追加数据
XXAPI int xvoArrayAppendValue(xvalue pArr, xvalue pVal, int bColloc)
{
	if ( (pArr || pVal) == 0 ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	uint32 index = xrtPtrArrayAppend(pArr->vArray, pVal);
	if ( index == 0 ) {
		return FALSE;
	}
	if ( bColloc == FALSE ) {
		xvoAddRef(pVal);
	}
	return TRUE;
}



// Array 插入操作
XXAPI int xvoArrayInsertValue(xvalue pArr, uint32 index, xvalue pVal, int bColloc)
{
	if ( (pArr || pVal) == 0 ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	uint32 idx = xrtPtrArrayInsert(pArr->vArray, index, pVal);
	if ( idx == 0 ) {
		return FALSE;
	}
	if ( bColloc == FALSE ) {
		xvoAddRef(pVal);
	}
	return TRUE;
}



// Array 修改操作
XXAPI int xvoArraySetValue(xvalue pArr, uint32 index, xvalue pVal, int bColloc)
{
	if ( (pArr || pVal) == 0 ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	xvalue pOldVal = xrtPtrArrayGet(pArr->vArray, index + 1);
	if ( pOldVal == NULL ) {
		return FALSE;
	}
	xvoUnref(pOldVal);
	xrtPtrArraySet_Inline(pArr->vArray, index + 1, pVal);
	if ( bColloc == FALSE ) {
		xvoAddRef(pVal);
	}
	return TRUE;
}



// Array 操作
XXAPI int xvoArraySwap(xvalue pArr, uint32 index1, uint32 index2)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArraySwap(pArr->vArray, index1, index2);
}
XXAPI int xvoArrayRemove(xvalue pArr, uint32 index, uint32 count)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArrayRemove(pArr->vArray, index, count);
}
XXAPI uint32 xvoArraySize(xvalue pArr)
{
	if ( pArr == NULL ) {
		return 0;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return 0;
	}
	return pArr->vArray->Count;
}
XXAPI uint32 xvoArrayClear(xvalue pArr)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	for ( int i = 1; i <= pArr->vArray->Count; i++ ) {
		xvalue pVal = xrtPtrArrayGet_Inline(pArr->vArray, i);
		xvoUnref(pVal);
	}
	xrtPtrArrayClear(pArr->vArray);
	return TRUE;
}
XXAPI int xvoArrayAlloc(xvalue pArr, uint32 count)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArrayMalloc(pArr->vArray, count);
}
XXAPI int xvoArraySort(xvalue pArr, ptr proc)
{
	if ( pArr == NULL ) {
		return FALSE;
	}
	if ( pArr->Type != XVO_DT_ARRAY ) {
		return FALSE;
	}
	return xrtPtrArraySort(pArr->vArray, proc);
}



// List 读数据
XXAPI xvalue xvoListGetValue(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return NULL;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return NULL;
	}
	return xrtListGetPtr(pList->vList, index);
}



// List 写数据
XXAPI int xvoListSetValue(xvalue pList, int64 index, xvalue pVal, int bColloc)
{
	if ( (pList || pVal) == 0 ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	xvalue pOldVal = NULL;
	int iRet = xrtListSetPtr(pList->vList, index, pVal, (ptr*)&pOldVal);
	if ( iRet == FALSE ) {
		return FALSE;
	}
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	if ( bColloc == FALSE ) {
		xvoAddRef(pVal);
	}
	return TRUE;
}



// List 操作
XXAPI int xvoListExists(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	return xrtListExists(pList->vList, index);
}
XXAPI int xvoListRemove(xvalue pList, int64 index)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	xvalue pOldVal = xrtListRemovePtr(pList->vList, index);
	if ( pOldVal ) {
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
}
XXAPI int xvoListSize(xvalue pList)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	return xrtListCount(pList->vList);
}
XXAPI int xvoListClear(xvalue pList)
{
	if ( pList == NULL ) {
		return FALSE;
	}
	if ( pList->Type != XVO_DT_LIST ) {
		return FALSE;
	}
	xrtListWalk(pList->vList, (ptr)xvoListClear_FreeProc, pList);
	xrtListClear(pList->vList);
	return TRUE;
}



// Coll 功能实现



// Coll 写数据
XXAPI int xvoCollSetValue(xvalue pColl, xvalue pVal, int bColloc)
{
	if ( (pColl || pVal) == 0 ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	int bNew = FALSE;
	xvalue* ppVal = xrtAVLTreeInsert(pColl->vColl, pVal, &bNew);
	if ( ppVal ) {
		
	}
	/*
	xvalue pOldVal = NULL;
	int iRet = xrtDictSetPtr(pColl->vColl, key, kl, pVal, (ptr*)&pOldVal);
	if ( iRet == FALSE ) {
		return FALSE;
	}
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	if ( bColloc == FALSE ) {
		xvoAddRef(pVal);
	}
	*/
	return TRUE;
}



// Coll 集合操作



// Coll 操作
XXAPI int xvoCollExists(xvalue pColl, xvalue pVal)
{
	if ( pColl == NULL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	xvalue pRetVal = xrtAVLTreeSearch(pColl->vColl, pVal);
	if ( pRetVal ) {
		return TRUE;
	} else {
		return FALSE;
	}
}
XXAPI int xvoCollRemove(xvalue pColl, xvalue pVal)
{
	if ( pColl == NULL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	/*
	xvalue pOldVal = xrtDictRemovePtr(pColl->vColl, key, kl);
	if ( pOldVal ) {
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
	*/
	return FALSE;
}
XXAPI int xvoCollSize(xvalue pColl)
{
	if ( pColl == NULL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	return pColl->vColl->Count;
}
XXAPI int xvoCollClear(xvalue pColl)
{
	if ( pColl == NULL ) {
		return FALSE;
	}
	if ( pColl->Type != XVO_DT_COLL ) {
		return FALSE;
	}
	/*
	xrtDictWalk(pColl->vColl, (ptr)xvoTableClear_FreeProc, pColl);
	*/
	xrtAVLTreeClear(pColl->vColl);
	return TRUE;
}



// Table 读数据
XXAPI xvalue xvoTableGetValue(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return NULL;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return NULL;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(key);
	}
	return xrtDictGetPtr(pTbl->vTable, key, kl);
}



// Table 写数据
XXAPI int xvoTableSetValue(xvalue pTbl, str key, uint32 kl, xvalue pVal, int bColloc)
{
	if ( (pTbl || pVal) == 0 ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	if ( key == NULL ) {
		key = xCore.sNull;
		kl = 0;
	} else if ( kl == 0 ) {
		kl = strlen(key);
	}
	xvalue pOldVal = NULL;
	int iRet = xrtDictSetPtr(pTbl->vTable, key, kl, pVal, (ptr*)&pOldVal);
	if ( iRet == FALSE ) {
		return FALSE;
	}
	if ( pOldVal ) {
		xvoUnref(pOldVal);
	}
	if ( bColloc == FALSE ) {
		xvoAddRef(pVal);
	}
	return TRUE;
}



// Table 操作
XXAPI int xvoTableExists(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	return xrtDictExists(pTbl->vTable, key, kl);
}
XXAPI int xvoTableRemove(xvalue pTbl, str key, uint32 kl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	xvalue pOldVal = xrtDictRemovePtr(pTbl->vTable, key, kl);
	if ( pOldVal ) {
		xvoUnref(pOldVal);
		return TRUE;
	} else {
		return FALSE;
	}
}
XXAPI int xvoTableSize(xvalue pTbl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	return xrtDictCount(pTbl->vTable);
}
XXAPI int xvoTableClear(xvalue pTbl)
{
	if ( pTbl == NULL ) {
		return FALSE;
	}
	if ( pTbl->Type != XVO_DT_TABLE ) {
		return FALSE;
	}
	xrtDictWalk(pTbl->vTable, (ptr)xvoTableClear_FreeProc, pTbl);
	xrtDictClear(pTbl->vTable);
	return TRUE;
}


