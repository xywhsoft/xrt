#include "../internal/xrt_template.h"



#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)



/* 注册名称到定义节点的不可变索引，避免大量定义产生二次查找成本。 */
bool __xrtTemplateDefinitionAdd(
	xtemplate* pTemplate,
	xstrview Name,
	uint32 iNode
)
{
	uint32* pStored;
	bool bNew;

	if ( pTemplate->Definitions == NULL ) {
		pTemplate->Definitions = xrtMapCreate(sizeof(uint32));
		if ( pTemplate->Definitions == NULL ) {
			return false;
		}
	}
	pStored = (uint32*)xrtMapGetOrAdd(
		pTemplate->Definitions,
		(xbytesview){ (cbytes)Name.Data, Name.Size },
		&bNew
	);
	if ( pStored == NULL ) {
		return false;
	}
	if ( !bNew ) {
		const xrt_template_node* pNode =
			(const xrt_template_node*)xrtArrayConstGet(
				&pTemplate->Nodes,
				iNode
			);

		__xrtTemplateError(
			XERR_VALUE,
			XTEMPLATE_ERROR_SYNTAX,
			"compile-define",
			"template definition name is duplicated",
			pTemplate,
			pNode != NULL ? pNode->SourceOffset : 0,
			pNode != NULL ? pNode->SourceSize : 0
		);
		return false;
	}
	*pStored = iNode;
	return true;
}



/* 按显式长度名称查找模板内定义节点。 */
const xrt_template_node* __xrtTemplateDefinition(
	const xtemplate* pTemplate,
	xstrview Name,
	uint32* pNode
)
{
	const uint32* pStored;

	if ( pTemplate->Definitions == NULL ) {
		return NULL;
	}
	pStored = (const uint32*)xrtMapConstGet(
		pTemplate->Definitions,
		(xbytesview){ (cbytes)Name.Data, Name.Size }
	);
	if ( pStored == NULL ) {
		return NULL;
	}
	if ( pNode != NULL ) {
		*pNode = *pStored;
	}
	return (const xrt_template_node*)xrtArrayConstGet(
		&pTemplate->Nodes,
		*pStored
	);
}



/* 判断目标渲染单元是否已经出现在当前 include 栈。 */
static bool __xrtTemplateIncludeCycle(
	const xrt_template_render* pRender,
	const xtemplate* pTemplate,
	uint32 iDefinition
)
{
	const xrt_template_frame* pFrame = pRender->Frame;

	while ( pFrame != NULL ) {
		if ( (pFrame->Template == pTemplate) &&
			 (pFrame->Definition == iDefinition) ) {
			return true;
		}
		pFrame = pFrame->Parent;
	}
	return false;
}



/* 调用外部解析器并隔离回调留下的线程错误。 */
static bool __xrtTemplateResolveExternal(
	xrt_template_render* pRender,
	const xrt_template_node* pNode,
	xstrview Name,
	xtemplate** pTemplate
)
{
	xerror* pPrevious;
	xerror* pCallback;
	bool bResult;

	*pTemplate = NULL;
	if ( pRender->Config->Resolve == NULL ) {
		return true;
	}
	pPrevious = xrtTakeError();
	bResult = pRender->Config->Resolve(
		pRender->Config->ResolveData,
		Name,
		pTemplate
	);
	pCallback = xrtTakeError();
	if ( bResult ) {
		xrtErrorFree(pCallback);
		if ( pPrevious != NULL ) {
			__xrtErrorSetOwned(pPrevious);
		}
		return true;
	}
	xrtTemplateRelease(*pTemplate);
	*pTemplate = NULL;
	xrtErrorFree(pPrevious);
	if ( pCallback != NULL ) {
		__xrtErrorSetOwned(pCallback);
		__xrtTemplateWrapCurrent(
			XTEMPLATE_ERROR_CALLBACK,
			"resolve-include",
			"template include resolver failed",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
	} else {
		__xrtTemplateError(
			XERR_INTERNAL,
			XTEMPLATE_ERROR_CALLBACK,
			"resolve-include",
			"template include resolver failed",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
	}
	return false;
}



/* 在共享预算和作用域内执行一个本地定义或外部模板。 */
static xrt_template_flow __xrtTemplateRenderTarget(
	xrt_template_render* pRender,
	const xrt_template_node* pInclude,
	const xtemplate* pTarget,
	uint32 iDefinition,
	uint32 iStart,
	uint32 iEnd
)
{
	xrt_template_frame Frame;
	const xtemplate* pPrevious = pRender->Template;
	xrt_template_flow Flow;

	if ( pRender->IncludeDepth >= pRender->Config->MaxIncludeDepth ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"render-include",
			"template include depth limit exceeded",
			pPrevious,
			pInclude->SourceOffset,
			pInclude->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( __xrtTemplateIncludeCycle(
		pRender,
		pTarget,
		iDefinition
	) ) {
		__xrtTemplateError(
			XERR_STATE,
			XTEMPLATE_ERROR_CYCLE,
			"render-include",
			"template include cycle detected",
			pPrevious,
			pInclude->SourceOffset,
			pInclude->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	Frame.Parent = pRender->Frame;
	Frame.Template = pTarget;
	Frame.Definition = iDefinition;
	pRender->Template = pTarget;
	pRender->Frame = &Frame;
	pRender->IncludeDepth++;
	Flow = __xrtTemplateRenderSpan(pRender, iStart, iEnd);
	pRender->IncludeDepth--;
	pRender->Frame = Frame.Parent;
	pRender->Template = pPrevious;
	if ( (Flow == XRT_TEMPLATE_FLOW_BREAK) ||
		 (Flow == XRT_TEMPLATE_FLOW_CONTINUE) ) {
		__xrtTemplateError(
			XERR_STATE,
			XTEMPLATE_ERROR_SYNTAX,
			"render-include",
			"template loop control cannot cross an include boundary",
			pPrevious,
			pInclude->SourceOffset,
			pInclude->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	return Flow;
}



/* 本地定义优先，缺失时通过显式解析器取得并释放外部模板引用。 */
xrt_template_flow __xrtTemplateRenderInclude(
	xrt_template_render* pRender,
	const xrt_template_node* pNode
)
{
	xrt_template_eval Value;
	xstrview Name;
	const xrt_template_node* pDefinition;
	xtemplate* pExternal = NULL;
	uint32 iDefinition;
	xrt_template_flow Flow;

	if ( !__xrtTemplateEvalExpression(
		pRender,
		pNode,
		pNode->Data.Include.Expression,
		&Value
	) ) {
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( !__xrtTemplateEvalText(&Value, &Name) ) {
		__xrtTemplateError(
			XERR_TYPE,
			XTEMPLATE_ERROR_TYPE,
			"render-include",
			"template include name must be text or bytes",
			pRender->Template,
			pNode->Data.Include.ExpressionOffset,
			pNode->Data.Include.ExpressionSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	pDefinition = __xrtTemplateDefinition(
		pRender->Template,
		Name,
		&iDefinition
	);
	if ( pDefinition != NULL ) {
		return __xrtTemplateRenderTarget(
			pRender,
			pNode,
			pRender->Template,
			iDefinition,
			pDefinition->Data.Define.BodyStart,
			pDefinition->Data.Define.BodyEnd
		);
	}
	if ( !__xrtTemplateResolveExternal(
		pRender,
		pNode,
		Name,
		&pExternal
	) ) {
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	if ( pExternal == NULL ) {
		__xrtTemplateError(
			XERR_NOT_FOUND,
			XTEMPLATE_ERROR_INCLUDE,
			"render-include",
			"template include was not found",
			pRender->Template,
			pNode->SourceOffset,
			pNode->SourceSize
		);
		return XRT_TEMPLATE_FLOW_ERROR;
	}
	Flow = __xrtTemplateRenderTarget(
		pRender,
		pNode,
		pExternal,
		XRT_TEMPLATE_INDEX_NONE,
		0,
		(uint32)pExternal->Nodes.Count
	);
	xrtTemplateRelease(pExternal);
	return Flow;
}



#endif
