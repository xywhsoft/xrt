


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
				} else if ( pVal->Type == XVO_DT_LIST ) {
				} else if ( pVal->Type == XVO_DT_COLL ) {
				} else if ( pVal->Type == XVO_DT_TABLE ) {
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
		xarray objArr = xrtArrayCreate(sizeof(xvalue));
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
XXAPI str xvoGetText(xvalue pVal)
{
	if ( pVal == NULL ) {
		return NULL;
	} else if ( pVal->Type == XVO_DT_BOOL ) {
		return pVal->vInt ? "true" : "false";
	} else if ( pVal->Type == XVO_DT_INT ) {
		str sRet = xrtTempMemory(24);
		xrtI64ToStr(pVal->vInt, sRet);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FLOAT ) {
		str sRet = xrtTempMemory(32);
		xrtNumToStr(pVal->vFloat, sRet);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TEXT ) {
		return pVal->vText;
	} else if ( pVal->Type == XVO_DT_TIME ) {
		str sRet = xrtTempMemory(24);
		int64 iYear;
		int iMonth, iDay, iHour, iMinute, iSecond;
		xrtDecodeSerial(pVal->vTime, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, NULL, NULL);
		sprintf(sRet, "%d-%02d-%02d %02d:%02d:%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
		return sRet;
	} else if ( pVal->Type == XVO_DT_FUNC ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[function:%x]", pVal->vFunc);
		return sRet;
	} else if ( pVal->Type == XVO_DT_ARRAY ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[array:%x]", pVal->vArray);
		return sRet;
	} else if ( pVal->Type == XVO_DT_LIST ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[list:%x]", pVal->vList);
		return sRet;
	} else if ( pVal->Type == XVO_DT_COLL ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[coll:%x]", pVal->vColl);
		return sRet;
	} else if ( pVal->Type == XVO_DT_TABLE ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[table:%x]", pVal->vTable);
		return sRet;
	} else if ( pVal->Type == XVO_DT_STRUCT ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[struct:%x]", pVal->vStruct);
		return sRet;
	} else if ( pVal->Type == XVO_DT_OBJECT ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[object:%x]", pVal->vObject);
		return sRet;
	} else if ( pVal->Type == XVO_DT_CUSTOM ) {
		str sRet = xrtTempMemory(32);
		sprintf(sRet, "[custom:%x]", pVal->vPoint);
		return sRet;
	} else {
		return NULL;
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


