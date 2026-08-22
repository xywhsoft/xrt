#include "../internal/xrt_template.h"



#if defined(XRT_FEATURE_TEMPLATE_FILE)



/* 在配置源码上限内读取文件，编译完成后立即释放临时字节。 */
XRT_API xtemplate* xrtTemplateCompileFileConfig(
	cstr sPath,
	const xtemplateconfig* pConfig
)
{
	bytes pSource;
	size_t iSize = 0;
	xtemplate* pTemplate;

	if ( (sPath == NULL) || !__xrtTemplateConfigValid(pConfig) ) {
		if ( sPath == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	pSource = xrtFileReadAllLimit(
		sPath,
		pConfig->MaxSourceBytes,
		&iSize
	);
	if ( pSource == NULL ) {
		return NULL;
	}
	pTemplate = xrtTemplateCompileConfig(
		(xstrview){ (cstr)pSource, iSize },
		pConfig
	);
	xrtFree(pSource);
	return pTemplate;
}



/* 使用默认预算读取并编译模板文件。 */
XRT_API xtemplate* xrtTemplateCompileFile(cstr sPath)
{
	xtemplateconfig Config;

	xrtTemplateConfigInit(&Config);
	return xrtTemplateCompileFileConfig(sPath, &Config);
}



#endif
