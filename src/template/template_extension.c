#include "../internal/xrt_template.h"



#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)



/* 判断扩展名称首字节是否合法。 */
static bool __xrtTemplateExtensionNameStart(char iByte)
{
	return ((iByte >= 'a') && (iByte <= 'z')) ||
		((iByte >= 'A') && (iByte <= 'Z')) || (iByte == '_');
}



/* 判断扩展名称后续字节是否合法。 */
static bool __xrtTemplateExtensionNameByte(char iByte)
{
	return __xrtTemplateExtensionNameStart(iByte) ||
		((iByte >= '0') && (iByte <= '9'));
}



/* 验证扩展描述的名称、类型、参数范围和回调。 */
static bool __xrtTemplateExtensionValid(
	const xtemplateextension* pExtension
)
{
	if ( !__xrtTemplateViewValid(pExtension->Name) ||
		 (pExtension->Name.Size == 0) ||
		 !__xrtTemplateExtensionNameStart(pExtension->Name.Data[0]) ||
		 (pExtension->Call == NULL) ||
		 (pExtension->MinArguments > pExtension->MaxArguments) ||
		 (pExtension->Type < XTEMPLATE_EXTENSION_FUNCTION) ||
		 (pExtension->Type > XTEMPLATE_EXTENSION_RAW_BLOCK) ) {
		return false;
	}
	for ( size_t i = 1u; i < pExtension->Name.Size; i++ ) {
		if ( !__xrtTemplateExtensionNameByte(pExtension->Name.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 映射释放稳定注册项时按注册表所有权状态析构用户数据。 */
static void __xrtTemplateExtensionDrop(
	xbytesview Key,
	ptr pValue,
	ptr pUserData
)
{
	xrt_template_extension_def* pDefinition =
		(xrt_template_extension_def*)pValue;
	xtemplateregistry* pRegistry = (xtemplateregistry*)pUserData;

	(void)Key;
	if ( pRegistry->OwnData && (pDefinition->Drop != NULL) ) {
		pDefinition->Drop(pDefinition->Data);
	}
}



/* 释放尚未公开或最后一个引用持有的扩展注册表。 */
static void __xrtTemplateRegistryDestroy(xtemplateregistry* pRegistry)
{
	if ( pRegistry == NULL ) {
		return;
	}
	xrtMapUnit(&pRegistry->Statements);
	xrtMapUnit(&pRegistry->Functions);
	xrtFree(pRegistry);
}



/* 校验并复制全部扩展定义，成功后接管每项用户数据。 */
XRT_API xtemplateregistry* xrtTemplateRegistryCreate(
	const xtemplateextension* pExtensions,
	size_t iCount
)
{
	xtemplateregistry* pRegistry;

	if ( (pExtensions == NULL) && (iCount != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pRegistry = (xtemplateregistry*)xrtCalloc(1, sizeof(*pRegistry));
	if ( pRegistry == NULL ) {
		return NULL;
	}
	pRegistry->RefCount = 1;
	if ( !xrtMapInit(
		&pRegistry->Functions,
		sizeof(xrt_template_extension_def)
	) || !xrtMapInit(
		&pRegistry->Statements,
		sizeof(xrt_template_extension_def)
	) || !xrtMapSetDrop(
		&pRegistry->Functions,
		__xrtTemplateExtensionDrop,
		pRegistry
	) || !xrtMapSetDrop(
		&pRegistry->Statements,
		__xrtTemplateExtensionDrop,
		pRegistry
	) ) {
		__xrtTemplateRegistryDestroy(pRegistry);
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xtemplateextension* pSource = &pExtensions[i];
		xmap* pMap;
		xrt_template_extension_def* pDefinition;
		bool bNew;

		if ( !__xrtTemplateExtensionValid(pSource) ) {
			__xrtTemplateError(
				XERR_ARGUMENT,
				XTEMPLATE_ERROR_CONFIG,
				"create-registry",
				"invalid template extension descriptor",
				NULL,
				0,
				0
			);
			__xrtTemplateRegistryDestroy(pRegistry);
			return NULL;
		}
		pMap = pSource->Type == XTEMPLATE_EXTENSION_FUNCTION
			? &pRegistry->Functions : &pRegistry->Statements;
		pDefinition = (xrt_template_extension_def*)xrtMapGetOrAdd(
			pMap,
			(xbytesview){
				(cbytes)pSource->Name.Data,
				pSource->Name.Size
			},
			&bNew
		);
		if ( pDefinition == NULL ) {
			__xrtTemplateRegistryDestroy(pRegistry);
			return NULL;
		}
		if ( !bNew ) {
			__xrtTemplateError(
				XERR_EXISTS,
				XTEMPLATE_ERROR_CONFIG,
				"create-registry",
				"template extension name is duplicated",
				NULL,
				0,
				0
			);
			__xrtTemplateRegistryDestroy(pRegistry);
			return NULL;
		}
		pDefinition->Type = pSource->Type;
		pDefinition->MinArguments = pSource->MinArguments;
		pDefinition->MaxArguments = pSource->MaxArguments;
		pDefinition->Call = pSource->Call;
		pDefinition->Data = pSource->Data;
		pDefinition->Drop = pSource->Drop;
	}
	pRegistry->OwnData = true;
	return pRegistry;
}



/* 增加不可变注册表引用并返回原指针。 */
XRT_API xtemplateregistry* xrtTemplateRegistryRef(
	const xtemplateregistry* pRegistry
)
{
	xtemplateregistry* pMutable = (xtemplateregistry*)pRegistry;

	if ( pMutable == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pMutable->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pMutable;
}



/* 释放注册表引用及其最终拥有的用户数据。 */
XRT_API void xrtTemplateRegistryRelease(
	const xtemplateregistry* pRegistry
)
{
	xtemplateregistry* pMutable = (xtemplateregistry*)pRegistry;

	if ( (pMutable != NULL) &&
		 (xrtRefRelease(&pMutable->RefCount) == 0) ) {
		__xrtTemplateRegistryDestroy(pMutable);
	}
}



/* 按函数或语句语法类别查找不可变扩展定义。 */
const xrt_template_extension_def* __xrtTemplateExtensionFind(
	const xtemplateregistry* pRegistry,
	xtemplateextensiontype Type,
	xstrview Name
)
{
	const xmap* pMap;

	if ( pRegistry == NULL ) {
		return NULL;
	}
	pMap = Type == XTEMPLATE_EXTENSION_FUNCTION
		? &pRegistry->Functions : &pRegistry->Statements;
	return (const xrt_template_extension_def*)xrtMapConstGet(
		pMap,
		(xbytesview){ (cbytes)Name.Data, Name.Size }
	);
}



/* 返回调用节点在模板源码中的名称视图。 */
XRT_API xstrview xrtTemplateCallName(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pCall->Render->Template->Source +
			pCall->Node->Data.Extension.NameOffset,
		pCall->Node->Data.Extension.NameSize
	};
}



/* 返回注册描述项携带的用户数据。 */
XRT_API ptr xrtTemplateCallData(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pCall->Definition->Data;
}



/* 返回当前调用参数数量。 */
XRT_API size_t xrtTemplateCallArgumentCount(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pCall->Node->Data.Extension.ArgumentCount;
}



/* 把内部参数记录转换成借用的公共视图。 */
static bool __xrtTemplateCallArgumentView(
	const xtemplatecall* pCall,
	size_t iIndex,
	xtemplateargview* pArgument
)
{
	const xrt_template_argument* pCompiled;

	if ( iIndex >= pCall->Node->Data.Extension.ArgumentCount ) {
		__xrtErrorSetRange();
		return false;
	}
	pCompiled = (const xrt_template_argument*)xrtArrayConstGet(
		&pCall->Render->Template->Arguments,
		(size_t)pCall->Node->Data.Extension.ArgumentStart + iIndex
	);
	if ( pCompiled == NULL ) {
		return false;
	}
	memset(pArgument, 0, sizeof(*pArgument));
	pArgument->Index = iIndex;
	if ( pCompiled->NameSize != 0 ) {
		pArgument->Name = (xstrview){
			pCall->Render->Template->Source + pCompiled->NameOffset,
			pCompiled->NameSize
		};
	}
	pArgument->Source = (xstrview){
		pCall->Render->Template->Source + pCompiled->SourceOffset,
		pCompiled->SourceSize
	};
	return true;
}



/* 返回指定位置参数的借用视图。 */
XRT_API bool xrtTemplateCallArgument(
	const xtemplatecall* pCall,
	size_t iIndex,
	xtemplateargview* pArgument
)
{
	if ( (pCall == NULL) || (pArgument == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTemplateCallArgumentView(pCall, iIndex, pArgument);
}



/* 按显式长度名称查找命名参数，缺失不设置错误。 */
XRT_API bool xrtTemplateCallFind(
	const xtemplatecall* pCall,
	xstrview Name,
	xtemplateargview* pArgument
)
{
	if ( (pCall == NULL) || (pArgument == NULL) ||
		 !__xrtTemplateViewValid(Name) ) {
		if ( (pCall == NULL) || (pArgument == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	for ( size_t i = 0;
		i < pCall->Node->Data.Extension.ArgumentCount; i++ ) {
		xtemplateargview Argument;

		if ( !__xrtTemplateCallArgumentView(pCall, i, &Argument) ) {
			return false;
		}
		if ( (Argument.Name.Size != 0) &&
			 (Argument.Name.Size == Name.Size) &&
			 ((Name.Size == 0) ||
			  (memcmp(Argument.Name.Data, Name.Data, Name.Size) == 0)) ) {
			*pArgument = Argument;
			return true;
		}
	}
	memset(pArgument, 0, sizeof(*pArgument));
	return false;
}



/* 把内部无分配求值结果复制成稳定公共布局。 */
static bool __xrtTemplateCallValue(
	const xrt_template_eval* pSource,
	xtemplatevalue* pValue
)
{
	memset(pValue, 0, sizeof(*pValue));
	pValue->Type = pSource->Type;
	pValue->Value = pSource->Value;
	if ( pSource->Value == NULL ) {
		switch ( pSource->Type ) {
			case XVALUE_BOOL: pValue->Bool = pSource->Data.Bool; break;
			case XVALUE_INT: pValue->Integer = pSource->Data.Integer; break;
			case XVALUE_FLOAT: pValue->Float = pSource->Data.Float; break;
			case XVALUE_STRING:
			case XVALUE_BYTES: pValue->Text = pSource->Data.String; break;
			case XVALUE_TIME: pValue->Time = pSource->Data.Time; break;
			default: break;
		}
		return true;
	}
	switch ( pSource->Type ) {
		case XVALUE_BOOL:
			return xrtValueGetBool(pSource->Value, &pValue->Bool);
		case XVALUE_INT:
			return xrtValueGetInt(pSource->Value, &pValue->Integer);
		case XVALUE_FLOAT:
			return xrtValueGetFloat(pSource->Value, &pValue->Float);
		case XVALUE_STRING:
			return xrtValueGetString(pSource->Value, &pValue->Text);
		case XVALUE_BYTES:
		{
			xbytesview Data;

			if ( !xrtValueGetBytes(pSource->Value, &Data) ) {
				return false;
			}
			pValue->Text = (xstrview){ (cstr)Data.Data, Data.Size };
			return true;
		}
		case XVALUE_TIME:
			return xrtValueGetTime(pSource->Value, &pValue->Time);
		default:
			return true;
	}
}



/* 在当前渲染作用域内求值指定参数。 */
XRT_API bool xrtTemplateCallEval(
	xtemplatecall* pCall,
	const xtemplateargview* pArgument,
	xtemplatevalue* pValue
)
{
	const xrt_template_argument* pCompiled;
	xrt_template_eval Value;

	if ( (pCall == NULL) || (pArgument == NULL) || (pValue == NULL) ||
		 (pArgument->Index >=
		  pCall->Node->Data.Extension.ArgumentCount) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pCompiled = (const xrt_template_argument*)xrtArrayConstGet(
		&pCall->Render->Template->Arguments,
		(size_t)pCall->Node->Data.Extension.ArgumentStart +
			pArgument->Index
	);
	if ( pCompiled == NULL ) {
		return false;
	}
	if ( !__xrtTemplateEvalExpression(
		pCall->Render,
		pCall->Node,
		pCompiled->Expression,
		&Value
	) ) {
		return false;
	}
	return __xrtTemplateCallValue(&Value, pValue);
}



/* 通过当前 writer 和共享预算直接写出分片。 */
XRT_API bool xrtTemplateCallWrite(
	xtemplatecall* pCall,
	xstrview Text
)
{
	if ( (pCall == NULL) || !__xrtTemplateViewValid(Text) ) {
		if ( pCall == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	return __xrtTemplateEmit(pCall->Render, pCall->Node, Text);
}



/* 渲染解析块主体，并禁止循环控制跨过扩展回调边界。 */
static bool __xrtTemplateCallRenderBody(xtemplatecall* pCall)
{
	xrt_template_flow Flow;

	if ( pCall->Definition->Type != XTEMPLATE_EXTENSION_BLOCK ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	Flow = __xrtTemplateRenderSpan(
		pCall->Render,
		pCall->Node->Data.Extension.BodyStart,
		pCall->Node->Data.Extension.BodyEnd
	);
	if ( (Flow == XRT_TEMPLATE_FLOW_BREAK) ||
		 (Flow == XRT_TEMPLATE_FLOW_CONTINUE) ) {
		__xrtTemplateError(
			XERR_STATE,
			XTEMPLATE_ERROR_SYNTAX,
			"render-extension",
			"template loop control cannot cross an extension boundary",
			pCall->Render->Template,
			pCall->Node->SourceOffset,
			pCall->Node->SourceSize
		);
		return false;
	}
	return Flow == XRT_TEMPLATE_FLOW_OK;
}



/* 使用当前作用域渲染解析块主体。 */
XRT_API bool xrtTemplateCallRender(xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtTemplateCallRenderBody(pCall);
}



/* 临时替换当前值并在返回前恢复原作用域。 */
XRT_API bool xrtTemplateCallRenderCurrent(
	xtemplatecall* pCall,
	const xvalue* pCurrent
)
{
	const xvalue* pPrevious;
	bool bResult;

	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pPrevious = pCall->Render->Current;
	pCall->Render->Current = pCurrent;
	bResult = __xrtTemplateCallRenderBody(pCall);
	pCall->Render->Current = pPrevious;
	return bResult;
}



/* 返回原样块主体，其他类型返回空视图。 */
XRT_API xstrview xrtTemplateCallRaw(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	if ( pCall->Definition->Type != XTEMPLATE_EXTENSION_RAW_BLOCK ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pCall->Render->Template->Source +
			pCall->Node->Data.Extension.RawOffset,
		pCall->Node->Data.Extension.RawSize
	};
}



/* 返回当前作用域借用值。 */
XRT_API const xvalue* xrtTemplateCallCurrent(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pCall->Render->Current;
}



/* 返回根作用域借用值。 */
XRT_API const xvalue* xrtTemplateCallRoot(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pCall->Render->Config->Root;
}



/* 返回全局作用域借用值。 */
XRT_API const xvalue* xrtTemplateCallGlobal(const xtemplatecall* pCall)
{
	if ( pCall == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pCall->Render->Config->Global;
}



/* 调用扩展并隔离成功回调留下的错误，失败时保留原因链。 */
xrt_template_flow __xrtTemplateRenderExtension(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	xtemplatecall Call;
	xerror* pPrevious;
	xerror* pCallback;
	bool bResult;

	Call.Render = pRender;
	Call.Node = pNode;
	Call.Definition = pNode->Data.Extension.Definition;
	pPrevious = xrtTakeError();
	bResult = Call.Definition->Call(&Call);
	pCallback = xrtTakeError();
	if ( bResult ) {
		xrtErrorFree(pCallback);
		if ( pPrevious != NULL ) {
			__xrtErrorSetOwned(pPrevious);
		}
		return XRT_TEMPLATE_FLOW_OK;
	}
	xrtErrorFree(pPrevious);
	if ( pCallback != NULL ) {
		__xrtErrorSetOwned(pCallback);
		if ( (xrtErrorDomain(pCallback) == NULL) ||
			 (strcmp(xrtErrorDomain(pCallback), "xrt.template") != 0) ) {
			__xrtTemplateWrapCurrent(
				XTEMPLATE_ERROR_CALLBACK,
				"render-extension",
				"template extension callback failed",
				pRender->Template,
				pNode->SourceOffset,
				pNode->SourceSize
			);
		}
	} else {
		__xrtTemplateError(
			XERR_INTERNAL,
			XTEMPLATE_ERROR_CALLBACK,
			"render-extension",
			"template extension callback failed",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
	}
	return XRT_TEMPLATE_FLOW_ERROR;
}



#endif
