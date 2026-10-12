#ifndef XRT_INTERNAL_WEBSOCKET_GROUP_H
#define XRT_INTERNAL_WEBSOCKET_GROUP_H

#include <string.h>
#include <xrt/websocket_group.h>
#include <xrt/memory.h>
/* 构建并安装扩展库自己的结构化错误。 */
static inline void __xwsGroupErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 取走当前原因链，并用扩展库稳定域包装后重新安装。 */
static inline void __xwsGroupErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Domain = sDomain;
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



#include <xrt/websocket_group.h>



#if defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP)

/* 验证 Group 固定结构并设置稳定的 Group 域错误。 */
bool __xrtWsGroupCheck(
	const xwsgroup* pGroup,
	cstr sOperation
);



/* 判断一个已验证范围是否覆盖 Group 的私有固定结构。 */
bool __xrtWsGroupOverlaps(
	const xwsgroup* pGroup,
	cbytes pData,
	size_t iSize
);



/* 设置带稳定 WebSocket Group 域的结构化错误。 */
void __xrtWsGroupError(
	xerrkind Kind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
);



/* 为底层失败补充稳定的连接组操作边界。 */
void __xrtWsGroupWrap(
	xerrkind DefaultKind,
	xwsgrouperror Code,
	cstr sOperation,
	cstr sMessage
);

#endif

#endif
