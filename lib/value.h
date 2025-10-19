


// 静态值 : null、true、false
static xvalue_struct XVO_VALUE_NULL = {
	XVO_DT_NULL,
	0,
	TRUE,
	FALSE,
	0,
	0,
	0,
	0
};
static xvalue_struct XVO_VALUE_TRUE = {
	XVO_DT_BOOL,
	0,
	TRUE,
	FALSE,
	0,
	0,
	0,
	1
};
static xvalue_struct XVO_VALUE_FALSE = {
	XVO_DT_BOOL,
	0,
	TRUE,
	FALSE,
	0,
	0,
	0,
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
				if ( pVal->IsFree ) {
					// 释放值
					if ( pVal->Type == XVO_DT_TEXT ) {
						xrtFree(pVal->vText);
					} else if ( pVal->Type == XVO_DT_ARRAY ) {
					} else if ( pVal->Type == XVO_DT_LIST ) {
					} else if ( pVal->Type == XVO_DT_COLLECT ) {
					} else if ( pVal->Type == XVO_DT_TABLE ) {
					} else if ( pVal->Type == XVO_DT_STRUCT ) {
					} else if ( pVal->Type == XVO_DT_OBJECT ) {
					} else if ( pVal->Type == XVO_DT_CUSTOM ) {
					}
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
		pVal->IsFree = FALSE;
		pVal->IsReserve = 0;
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
		pVal->IsFree = FALSE;
		pVal->IsReserve = 0;
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
		bFree = FALSE;
	}
	xvalue pVal = xrtMalloc(sizeof(xvalue_struct));
	if ( pVal ) {
		pVal->Type = XVO_DT_TEXT;
		pVal->IsStatic = FALSE;
		pVal->IsFree = bFree;
		pVal->IsReserve = 0;
		pVal->RefCount = 1;
		if ( iCharset == XVO_SDT_STR_U16 ) {
			pVal->SubType = XVO_SDT_STR_U16;
			if ( iSize == 0 ) {
				pVal->Size = u16len(sVal);
			} else {
				pVal->Size = iSize;
			}
		} else if ( iCharset == XVO_SDT_STR_U32 ) {
			pVal->SubType = XVO_SDT_STR_U32;
			if ( iSize == 0 ) {
				pVal->Size = u32len(sVal);
			} else {
				pVal->Size = iSize;
			}
		} else if ( iCharset == XVO_SDT_STR_BIN ) {
			pVal->SubType = XVO_SDT_STR_BIN;
			if ( iSize == 0 ) {
				pVal->Size = 0;
				pVal->IsFree = FALSE;
			} else {
				pVal->Size = iSize;
			}
		} else {
			pVal->SubType = XVO_SDT_STR_U8;
			if ( iSize == 0 ) {
				pVal->Size = strlen(sVal);
			} else {
				pVal->Size = iSize;
			}
		}
		
		pVal->vFloat = fVal;
	}
	return pVal;
}


