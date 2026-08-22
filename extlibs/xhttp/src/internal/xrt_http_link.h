#ifndef XRT_INTERNAL_HTTP_LINK_H
#define XRT_INTERNAL_HTTP_LINK_H

#include "xrt_http.h"

#include <xrt/http_link.h>



#if defined(XHTTP_FEATURE_HTTP_LINK)

/* 验证 rel 或 rev 参数解码后的关系类型列表。 */
bool __xrtHttpLinkRelationsParamValid(const xhttpparam* pParam);



/* 验证写入侧已经解码的关系类型列表。 */
bool __xrtHttpLinkRelationsValueValid(xstrview Relations);



/* 验证参数解码后的 URI-reference。 */
bool __xrtHttpLinkUriParamValid(const xhttpparam* pParam);



/* 验证参数解码后的基本语言标签。 */
bool __xrtHttpLinkLanguageParamValid(const xhttpparam* pParam);



/* 验证参数解码后的 type-name/subtype-name。 */
bool __xrtHttpLinkTypeParamValid(const xhttpparam* pParam);



/* 重新解析并加载可信 Link 描述符。 */
bool __xrtHttpLinkDescriptorLoad(
	const xhttplink* pLink,
	xhttplink* pOutput
);

#endif

#endif
