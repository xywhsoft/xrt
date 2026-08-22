#ifndef XRT_ENVIRONMENT_H
#define XRT_ENVIRONMENT_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_ENVIRONMENT) && !defined(XRT_FEATURE_UNICODE)
	#error "XRT_FEATURE_ENVIRONMENT requires XRT_FEATURE_UNICODE"
#endif



#if defined(XRT_FEATURE_ENVIRONMENT)

/* 环境变量错误代码在 xrt.environment 域内稳定。 */
typedef enum xenverror {
	XENV_ERROR_NAME = 1,
	XENV_ERROR_VALUE,
	XENV_ERROR_SYSTEM
} xenverror;



XRT_EXTERN_C_BEGIN



/* 读取环境变量的 UTF-8 副本；变量不存在是成功状态并把输出设为空。 */
XRT_API bool xrtEnvLookup(cstr sName, str* psValue);



/* 读取环境变量的 UTF-8 副本；变量不存在或失败时返回空指针。 */
XRT_API str xrtEnvGet(cstr sName);



/* 设置 UTF-8 环境变量；允许空值并覆盖已有值。 */
XRT_API bool xrtEnvSet(cstr sName, cstr sValue);



/* 删除环境变量；变量原本不存在也视为成功。 */
XRT_API bool xrtEnvRemove(cstr sName);



XRT_EXTERN_C_END

#endif

#endif
