#include "../internal/xrt_template.h"

#include <stdio.h>



#if defined(XRT_FEATURE_TEMPLATE_CORE)



/* 释放尚未公开或最后一个引用持有的模板。 */
static void __xrtTemplateDestroy(xtemplate* pTemplate)
{
	if ( pTemplate == NULL ) {
		return;
	}
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		xrtArrayUnit(&pTemplate->Branches);
		xrtArrayUnit(&pTemplate->Expressions);
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		xrtMapDestroy(pTemplate->Definitions);
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		xrtArrayUnit(&pTemplate->Arguments);
		xrtTemplateRegistryRelease(pTemplate->Registry);
	#endif
	xrtArrayUnit(&pTemplate->Paths);
	xrtArrayUnit(&pTemplate->Nodes);
	xrtStrBufFree(&pTemplate->Text);
	xrtFree(pTemplate->Source);
	xrtFree(pTemplate);
}



/* 计算错误字节位置对应的 1 基行列。 */
static xtemplatelocation __xrtTemplateLocation(
	const xtemplate* pTemplate,
	size_t iOffset,
	size_t iSize
)
{
	xtemplatelocation Location = { 0, 0, 1u, 1u };

	if ( pTemplate == NULL ) {
		return Location;
	}
	if ( iOffset > pTemplate->SourceSize ) {
		iOffset = pTemplate->SourceSize;
	}
	if ( iSize > (pTemplate->SourceSize - iOffset) ) {
		iSize = pTemplate->SourceSize - iOffset;
	}
	Location.Offset = iOffset;
	Location.Size = iSize;
	for ( size_t i = 0; i < iOffset; i++ ) {
		if ( pTemplate->Source[i] == '\n' ) {
			Location.Line++;
			Location.Column = 1u;
		} else {
			Location.Column++;
		}
	}
	return Location;
}



/* 设置带源码位置的模板错误。 */
void __xrtTemplateError(
	xerrkind Kind,
	xtemplateerror Code,
	cstr sOperation,
	cstr sMessage,
	const xtemplate* pTemplate,
	size_t iOffset,
	size_t iSize
)
{
	char arrData[160];
	xtemplatelocation Location;
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.template";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	if ( pTemplate != NULL ) {
		Location = __xrtTemplateLocation(
			pTemplate,
			iOffset,
			iSize
		);
		(void)snprintf(
			arrData,
			sizeof(arrData),
			"offset=%llu;size=%llu;line=%llu;column=%llu",
			(unsigned long long)Location.Offset,
			(unsigned long long)Location.Size,
			(unsigned long long)Location.Line,
			(unsigned long long)Location.Column
		);
		Desc.Data = arrData;
	}
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 把当前底层错误包装为带模板源码位置的错误链。 */
void __xrtTemplateWrapCurrent(
	xtemplateerror Code,
	cstr sOperation,
	cstr sMessage,
	const xtemplate* pTemplate,
	size_t iOffset,
	size_t iSize
)
{
	char arrData[160];
	xtemplatelocation Location;
	xerrordesc Desc;
	xerror* pCause;
	xerror* pError;

	pCause = xrtTakeError();
	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : XERR_INTERNAL;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.template";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	if ( pTemplate != NULL ) {
		Location = __xrtTemplateLocation(
			pTemplate,
			iOffset,
			iSize
		);
		(void)snprintf(
			arrData,
			sizeof(arrData),
			"offset=%llu;size=%llu;line=%llu;column=%llu",
			(unsigned long long)Location.Offset,
			(unsigned long long)Location.Size,
			(unsigned long long)Location.Line,
			(unsigned long long)Location.Column
		);
		Desc.Data = arrData;
	}
	pError = xrtErrorBuild(&Desc);
	xrtErrorFree(pCause);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 验证字符串视图的指针与长度组合。 */
bool __xrtTemplateViewValid(xstrview Text)
{
	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证编译配置和全部预算。 */
bool __xrtTemplateConfigValid(const xtemplateconfig* pConfig)
{
	bool bInvalid;

	if ( (pConfig == NULL) ||
		 !__xrtTemplateViewValid(pConfig != NULL ? pConfig->Open : (xstrview){ 0 }) ||
		 !__xrtTemplateViewValid(pConfig != NULL ? pConfig->Close : (xstrview){ 0 }) ) {
		return false;
	}
	bInvalid = (pConfig->Open.Size == 0) ||
		 (pConfig->Close.Size == 0) ||
		 (pConfig->MaxSourceBytes == 0) ||
		 (pConfig->MaxNodes == 0) ||
		 (pConfig->MaxPathSegments == 0) ||
		 (pConfig->MaxPathDepth == 0) ||
		 (pConfig->MaxSourceBytes > UINT32_MAX) ||
		 (pConfig->MaxNodes > UINT32_MAX) ||
		 (pConfig->MaxPathSegments > UINT32_MAX) ||
		 (pConfig->MaxPathDepth > UINT32_MAX);
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		bInvalid = bInvalid ||
			(pConfig->MaxExpressions == 0) ||
			(pConfig->MaxBlockDepth == 0) ||
			(pConfig->MaxExpressionDepth == 0) ||
			(pConfig->MaxExpressions > UINT32_MAX) ||
			(pConfig->MaxBlockDepth > UINT32_MAX) ||
			(pConfig->MaxExpressionDepth > UINT32_MAX);
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		bInvalid = bInvalid ||
			(pConfig->MaxArguments == 0) ||
			(pConfig->MaxCallArguments == 0) ||
			(pConfig->MaxArguments > UINT32_MAX) ||
			(pConfig->MaxCallArguments > UINT32_MAX);
	#endif
	if ( bInvalid ) {
		__xrtTemplateError(
			XERR_ARGUMENT,
			XTEMPLATE_ERROR_CONFIG,
			"compile",
			"invalid template compile configuration",
			NULL,
			0,
			0
		);
		return false;
	}
	return true;
}



/* 验证渲染配置和全部预算。 */
bool __xrtTemplateRenderConfigValid(
	const xtemplaterenderconfig* pConfig
)
{
	bool bInvalid;

	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bInvalid = (pConfig->MaxOutputBytes == 0) ||
		 (pConfig->MaxSteps == 0) ||
		 ((pConfig->Flags & ~XRT_TEMPLATE_RENDER_FLAG_MASK) != 0);
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		bInvalid = bInvalid || (pConfig->MaxDepth == 0) ||
			(pConfig->MaxLoopIterations == 0) ||
			(pConfig->MaxDepth > (size_t)INT64_MAX) ||
			(pConfig->MaxLoopIterations > (size_t)INT64_MAX);
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		bInvalid = bInvalid || (pConfig->MaxIncludeDepth == 0) ||
			(pConfig->MaxIncludeDepth > (size_t)INT64_MAX);
	#endif
	if ( bInvalid ) {
		__xrtTemplateError(
			XERR_ARGUMENT,
			XTEMPLATE_ERROR_CONFIG,
			"render",
			"invalid template render configuration",
			NULL,
			0,
			0
		);
		return false;
	}
	return true;
}



/* 初始化默认括号和有限编译预算。 */
XRT_API void xrtTemplateConfigInit(xtemplateconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Open = XRT_STR_LITERAL("{");
	pConfig->Close = XRT_STR_LITERAL("}");
	pConfig->MaxSourceBytes = XTEMPLATE_SOURCE_DEFAULT;
	pConfig->MaxNodes = XTEMPLATE_NODES_DEFAULT;
	pConfig->MaxPathSegments = XTEMPLATE_PATH_SEGMENTS_DEFAULT;
	pConfig->MaxPathDepth = XTEMPLATE_PATH_DEPTH_DEFAULT;
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		pConfig->MaxExpressions = XTEMPLATE_EXPRESSIONS_DEFAULT;
		pConfig->MaxBlockDepth = XTEMPLATE_BLOCK_DEPTH_DEFAULT;
		pConfig->MaxExpressionDepth = XTEMPLATE_EXPRESSION_DEPTH_DEFAULT;
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		pConfig->MaxArguments = XTEMPLATE_ARGUMENTS_DEFAULT;
		pConfig->MaxCallArguments = XTEMPLATE_CALL_ARGUMENTS_DEFAULT;
	#endif
}



/* 初始化默认作用域和有限渲染预算。 */
XRT_API void xrtTemplateRenderConfigInit(xtemplaterenderconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->MaxOutputBytes = XTEMPLATE_OUTPUT_DEFAULT;
	pConfig->MaxSteps = XTEMPLATE_STEPS_DEFAULT;
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		pConfig->MaxDepth = XTEMPLATE_RENDER_DEPTH_DEFAULT;
		pConfig->MaxLoopIterations = XTEMPLATE_LOOP_DEFAULT;
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		pConfig->MaxIncludeDepth = XTEMPLATE_INCLUDE_DEPTH_DEFAULT;
	#endif
}



/* 使用显式配置编译模板源码。 */
XRT_API xtemplate* xrtTemplateCompileConfig(
	xstrview Source,
	const xtemplateconfig* pConfig
)
{
	xtemplate* pTemplate;

	if ( !__xrtTemplateViewValid(Source) ||
		 !__xrtTemplateConfigValid(pConfig) ) {
		return NULL;
	}
	if ( Source.Size > pConfig->MaxSourceBytes ) {
		__xrtTemplateError(
			XERR_RANGE,
			XTEMPLATE_ERROR_LIMIT,
			"compile",
			"template source exceeds its byte limit",
			NULL,
			0,
			0
		);
		return NULL;
	}
	if ( Source.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pTemplate = (xtemplate*)xrtCalloc(1, sizeof(*pTemplate));
	if ( pTemplate == NULL ) {
		return NULL;
	}
	pTemplate->RefCount = 1;
	xrtStrBufInit(&pTemplate->Text);
	if ( !xrtArrayInit(&pTemplate->Nodes, sizeof(xrt_template_node)) ||
		 !xrtArrayInit(&pTemplate->Paths, sizeof(xrt_template_path)) ) {
		__xrtTemplateDestroy(pTemplate);
		return NULL;
	}
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		if ( !xrtArrayInit(
			&pTemplate->Expressions,
			sizeof(xrt_template_expr)
		) || !xrtArrayInit(
			&pTemplate->Branches,
			sizeof(xrt_template_branch)
		) ) {
			__xrtTemplateDestroy(pTemplate);
			return NULL;
		}
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		if ( !xrtArrayInit(
			&pTemplate->Arguments,
			sizeof(xrt_template_argument)
		) ) {
			__xrtTemplateDestroy(pTemplate);
			return NULL;
		}
		if ( pConfig->Registry != NULL ) {
			pTemplate->Registry = xrtTemplateRegistryRef(
				pConfig->Registry
			);
			if ( pTemplate->Registry == NULL ) {
				__xrtTemplateDestroy(pTemplate);
				return NULL;
			}
		}
	#endif
	pTemplate->Source = xrtStrDupView(Source);
	if ( pTemplate->Source == NULL ) {
		__xrtTemplateDestroy(pTemplate);
		return NULL;
	}
	pTemplate->SourceSize = Source.Size;
	if ( !__xrtTemplateParse(pTemplate, pConfig) ) {
		__xrtTemplateDestroy(pTemplate);
		return NULL;
	}
	return pTemplate;
}



/* 使用默认配置编译模板源码。 */
XRT_API xtemplate* xrtTemplateCompile(xstrview Source)
{
	xtemplateconfig Config;

	xrtTemplateConfigInit(&Config);
	return xrtTemplateCompileConfig(Source, &Config);
}



/* 增加不可变模板引用并返回原指针。 */
XRT_API xtemplate* xrtTemplateRef(xtemplate* pTemplate)
{
	if ( pTemplate == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pTemplate->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pTemplate;
}



/* 释放模板引用。 */
XRT_API void xrtTemplateRelease(xtemplate* pTemplate)
{
	if ( (pTemplate != NULL) &&
		 (xrtRefRelease(&pTemplate->RefCount) == 0) ) {
		__xrtTemplateDestroy(pTemplate);
	}
}



/* 返回模板持有的原始源码视图。 */
XRT_API xstrview xrtTemplateSource(const xtemplate* pTemplate)
{
	if ( pTemplate == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){ pTemplate->Source, pTemplate->SourceSize };
}



/* 返回模板中的全部编译节点数量。 */
XRT_API size_t xrtTemplateNodeCount(const xtemplate* pTemplate)
{
	if ( pTemplate == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pTemplate->Nodes.Count;
}



/* 返回指定编译节点的只读视图。 */
XRT_API bool xrtTemplateNode(
	const xtemplate* pTemplate,
	size_t iIndex,
	xtemplatenodeview* pNode
)
{
	const xrt_template_node* pCompiled;

	if ( (pTemplate == NULL) || (pNode == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pCompiled = (const xrt_template_node*)xrtArrayConstGet(
		&pTemplate->Nodes,
		iIndex
	);
	if ( pCompiled == NULL ) {
		return false;
	}
	memset(pNode, 0, sizeof(*pNode));
	pNode->Type = (xtemplatenodetype)pCompiled->Type;
	pNode->Output = (xtemplateoutputtype)pCompiled->Output;
	pNode->Location = __xrtTemplateLocation(
		pTemplate,
		pCompiled->SourceOffset,
		pCompiled->SourceSize
	);
	pNode->Source = (xstrview){
		pTemplate->Source + pCompiled->SourceOffset,
		pCompiled->SourceSize
	};
	if ( pCompiled->Type == XTEMPLATE_NODE_OUTPUT ) {
		pNode->Expression = (xstrview){
			pTemplate->Source + pCompiled->Data.Output.ExpressionOffset,
			pCompiled->Data.Output.ExpressionSize
		};
		pNode->Format = (xstrview){
			pTemplate->Text.Data != NULL
				? pTemplate->Text.Data +
					pCompiled->Data.Output.FormatOffset
				: NULL,
			pCompiled->Data.Output.FormatSize
		};
	}
	#if defined(XRT_FEATURE_TEMPLATE_CONTROL)
		else if ( pCompiled->Type == XTEMPLATE_NODE_INLINE_IF ) {
			pNode->Expression = (xstrview){
				pTemplate->Source +
					pCompiled->Data.InlineIf.ExpressionOffset,
				pCompiled->Data.InlineIf.ExpressionSize
			};
		} else if ( pCompiled->Type == XTEMPLATE_NODE_FOREACH ) {
			pNode->Expression = (xstrview){
				pTemplate->Source +
					pCompiled->Data.Foreach.ExpressionOffset,
				pCompiled->Data.Foreach.ExpressionSize
			};
		}
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_COMPOSE)
		if ( pCompiled->Type == XTEMPLATE_NODE_DEFINE ) {
			pNode->Name = (xstrview){
				pTemplate->Text.Data +
					pCompiled->Data.Define.NameOffset,
				pCompiled->Data.Define.NameSize
			};
		} else if ( pCompiled->Type == XTEMPLATE_NODE_INCLUDE ) {
			pNode->Expression = (xstrview){
				pTemplate->Source +
					pCompiled->Data.Include.ExpressionOffset,
				pCompiled->Data.Include.ExpressionSize
			};
		}
	#endif
	#if defined(XRT_FEATURE_TEMPLATE_EXTENSION)
		if ( pCompiled->Type == XTEMPLATE_NODE_EXTENSION ) {
			pNode->Name = (xstrview){
				pTemplate->Source +
					pCompiled->Data.Extension.NameOffset,
				pCompiled->Data.Extension.NameSize
			};
		}
	#endif
	return true;
}



/* 把字符串构建器适配为模板 writer。 */
static bool __xrtTemplateBufferWrite(ptr pUserData, xstrview Text)
{
	return xrtStrBufAppend((xstrbuf*)pUserData, Text);
}



/* 把渲染分片写入回调。 */
XRT_API bool xrtTemplateWrite(
	const xtemplate* pTemplate,
	const xtemplaterenderconfig* pConfig,
	xtemplatewritefn pWrite,
	ptr pUserData
)
{
	xrt_template_render Render;

	if ( (pTemplate == NULL) || (pWrite == NULL) ||
		 !__xrtTemplateRenderConfigValid(pConfig) ) {
		if ( (pTemplate == NULL) || (pWrite == NULL) ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	memset(&Render, 0, sizeof(Render));
	Render.Template = pTemplate;
	Render.Config = pConfig;
	Render.Write = pWrite;
	Render.UserData = pUserData;
	Render.Current = pConfig->Current != NULL
		? pConfig->Current : pConfig->Root;
	return __xrtTemplateRender(&Render);
}



/* 把渲染结果事务追加到字符串构建器。 */
XRT_API bool xrtTemplateRenderTo(
	const xtemplate* pTemplate,
	const xtemplaterenderconfig* pConfig,
	xstrbuf* pOutput
)
{
	size_t iOldSize;

	if ( !xrtStrBufValid(pOutput) ) {
		return false;
	}
	iOldSize = pOutput->Size;
	if ( !xrtTemplateWrite(
			pTemplate,
			pConfig,
			__xrtTemplateBufferWrite,
			pOutput
		 ) ) {
		(void)xrtStrBufResize(pOutput, iOldSize);
		return false;
	}
	return true;
}



/* 使用当前值作为根和当前作用域并分配结果。 */
XRT_API str xrtTemplateRender(
	const xtemplate* pTemplate,
	const xvalue* pData,
	size_t* pSize
)
{
	xtemplaterenderconfig Config;
	xstrbuf Output;
	str sResult;

	if ( pSize != NULL ) {
		*pSize = 0;
	}
	xrtTemplateRenderConfigInit(&Config);
	Config.Root = pData;
	Config.Current = pData;
	xrtStrBufInit(&Output);
	if ( !xrtTemplateRenderTo(pTemplate, &Config, &Output) ) {
		xrtStrBufFree(&Output);
		return NULL;
	}
	if ( pSize != NULL ) {
		*pSize = Output.Size;
	}
	sResult = xrtStrBufTake(&Output);
	xrtStrBufFree(&Output);
	return sResult;
}



/* 从模板错误的数据字段读取源码位置。 */
XRT_API bool xrtTemplateErrorLocation(
	const xerror* pError,
	xtemplatelocation* pLocation
)
{
	const char* sData;
	unsigned long long iOffset;
	unsigned long long iSize;
	unsigned long long iLine;
	unsigned long long iColumn;

	if ( (pError == NULL) || (pLocation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (xrtErrorDomain(pError) == NULL) ||
		 (strcmp(xrtErrorDomain(pError), "xrt.template") != 0) ) {
		return false;
	}
	sData = xrtErrorData(pError);
	if ( (sData == NULL) ||
		 (sscanf(
			sData,
			"offset=%llu;size=%llu;line=%llu;column=%llu",
			&iOffset,
			&iSize,
			&iLine,
			&iColumn
		 ) != 4) ) {
		return false;
	}
	pLocation->Offset = (size_t)iOffset;
	pLocation->Size = (size_t)iSize;
	pLocation->Line = (size_t)iLine;
	pLocation->Column = (size_t)iColumn;
	return true;
}

#endif
